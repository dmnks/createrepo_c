#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include "fixtures.h"
#include "createrepo/error.h"
#include "createrepo/load_metadata.h"
#include "createrepo/package.h"
#include "createrepo/misc.h"
#include "createrepo/xml_dump_primary.c"

#define IF_NULL_EMPTY(x) (x) ? x : ""

cr_Package *
get_package()
{
    cr_Package *p;
    cr_Dependency *dep;
    cr_PackageFile *file;

    p = cr_package_new();
    p->pkgId = "123456";
    p->name = "foo";
    p->arch = "x86_64";
    p->version = "1.2.3";
    p->epoch = "1";
    p->release = "2";
    p->summary = "foo package";
    p->description = "super cool package";
    p->url = "http://package.com";
    p->time_file = 123456;
    p->time_build = 234567;
    p->rpm_license = "GPL";
    p->rpm_vendor = NULL;
    p->rpm_group = NULL;
    p->rpm_buildhost = NULL;
    p->rpm_sourcerpm = "foo.src.rpm";
    p->rpm_header_start = 20;
    p->rpm_header_end = 120;
    p->rpm_packager = NULL;
    p->size_package = 123;
    p->size_installed = 20;
    p->size_archive = 30;
    p->location_href = "foo.rpm";
    p->location_base = "/test/";
    p->checksum_type = "sha256";

    dep = cr_dependency_new();
    dep->name = "foobar_provide";
    dep->flags = NULL;
    dep->pre = FALSE;
    p->provides = (g_slist_prepend(p->provides, dep));

    dep = cr_dependency_new();
    dep->name = "foobar_dep";
    dep->flags = NULL;
    dep->pre = FALSE;
    dep->epoch = "3";
    p->requires = (g_slist_prepend(p->requires, dep));

    dep = cr_dependency_new();
    dep->name = "foobar_pre_dep";
    dep->flags = "LE";
    dep->epoch = "3";
    dep->pre = TRUE;
    p->requires = g_slist_prepend(p->requires, dep);

    file = cr_package_file_new();
    file->type = "";
    file->path = "/bin/";
    file->name = "foo";
    p->files = g_slist_prepend(p->files, file);

    file = cr_package_file_new();
    file->type = "dir";
    file->path = "/var/foo/";
    file->name = NULL;
    p->files = g_slist_prepend(p->files, file);

    file = cr_package_file_new();
    file->type = "dir";
    file->path = "/var/foo/";
    file->name = "baz";
    p->files = g_slist_prepend(p->files, file);
    return p;
}

cr_Package *
get_empty_package()
{
    cr_Package *p;
    cr_Dependency *dep;
    cr_PackageFile *file;

    p = cr_package_new();
    p->name = "foo";

    dep = cr_dependency_new();
    dep->name   = NULL;
    dep->flags  = NULL;
    dep->pre    = FALSE;
    p->requires = (g_slist_prepend(p->requires, dep));

    dep = cr_dependency_new();
    dep->name   = NULL;
    dep->flags  = NULL;
    dep->pre    = TRUE;
    p->requires = g_slist_prepend(p->requires, dep);

    file = cr_package_file_new();
    file->type = NULL;
    file->path = NULL;
    file->name = NULL;
    p->files   = g_slist_prepend(p->files, file);

    return p;
}


xmlNodePtr cmp_package_files_and_xml(GSList *files, xmlNodePtr current, int only_primary_files)
{
    if (!current || !files) {
        return current;
    }


    GSList *element = NULL;
    for(element = files; element; element=element->next) {
        cr_PackageFile *entry = (cr_PackageFile*) element->data;

        if (!(entry->path) || !(entry->name)) {
            continue;
        }

        gchar *fullname;
        fullname = g_strconcat(entry->path, entry->name, NULL);

        if (!fullname) {
            continue;
        }

        if (only_primary_files && !cr_is_primary(fullname)) {
            g_free(fullname);
            continue;
        }
        g_assert_cmpstr(current->name, ==, "file");
        g_assert_cmpstr(current->children->content, ==, fullname);
        if (entry->type && entry->type[0] != '\0' && strcmp(entry->type, "file")) {
            g_assert_cmpstr(current->properties->name, ==, "type");
            g_assert_cmpstr(current->properties->children->content, ==, IF_NULL_EMPTY(entry->type));
        }
    }

    return current->next;
}

xmlNodePtr cmp_package_pco_and_xml(GSList *pco_list, xmlNodePtr current, PcoType pcotype)
{
    const char *elem_name;

    if (pcotype >= PCO_TYPE_SENTINEL)
        return NULL;

    elem_name = pco_info[pcotype].elemname;
    GSList  *elem;
    cr_Dependency *item;
    int is_first = 1;
    if (pco_list){
        for(elem = pco_list; elem; elem = elem->next) {
            item = elem->data;
            if (!item->name || item->name[0] == '\0') {
                continue;
            }
            if (is_first){
                g_assert_cmpstr(current->name, ==, elem_name);
                current = current->children;
                is_first = 0;
            }else{
                current = current->next;
            }
            g_assert_cmpstr(current->name, ==, "rpm:entry");
            xmlAttrPtr current_attrs;
            current_attrs = current->properties;
            g_assert_cmpstr(current_attrs->name, ==, "name");
            g_assert_cmpstr(current_attrs->children->content, ==, item->name);

            if (item->flags && item->flags[0] != '\0') {
                current_attrs = current_attrs->next;
                g_assert_cmpstr(current_attrs->name, ==, "flags");
                g_assert_cmpstr(current_attrs->children->content, ==, IF_NULL_EMPTY(item->flags));

                if (item->epoch && item->epoch[0] != '\0') {
                    current_attrs = current_attrs->next;
                    g_assert_cmpstr(current_attrs->name, ==, "epoch");
                    g_assert_cmpstr(current_attrs->children->content, ==, IF_NULL_EMPTY(item->epoch));
                }

                if (item->version && item->version[0] != '\0') {
                    current_attrs = current_attrs->next;
                    g_assert_cmpstr(current_attrs->name, ==, "ver");
                    g_assert_cmpstr(current_attrs->children->content, ==, IF_NULL_EMPTY(item->version));
                }

                if (item->release && item->release[0] != '\0') {
                    current_attrs = current_attrs->next;
                    g_assert_cmpstr(current_attrs->name, ==, "rel");
                    g_assert_cmpstr(current_attrs->children->content, ==, IF_NULL_EMPTY(item->release));
                }
            }

            if (pcotype == PCO_TYPE_REQUIRES && item->pre) {
                current_attrs = current_attrs->next;
                g_assert_cmpstr(current_attrs->name, ==, "pre");
                g_assert_cmpstr(current_attrs->children->content, ==, "1");
            }
        }
    }
    if (!is_first){
        current = current->parent->next;
    }

    return current;
}

void
cmp_package_and_xml_node(cr_Package *pkg, xmlNodePtr node)
{
    xmlNodePtr current;

    current = node->children;
    g_assert_cmpstr(current->name, ==, "name");
    g_assert_cmpstr(current->children->content, ==, IF_NULL_EMPTY(pkg->name));

    current = current->next;
    g_assert_cmpstr(current->name, ==, "arch");
    g_assert_cmpstr(current->children->content, ==, IF_NULL_EMPTY(pkg->arch));

    current = current->next;
    g_assert_cmpstr(current->name, ==, "version");
    g_assert_cmpstr(current->properties->name, ==, "epoch");
    g_assert_cmpstr(current->properties->children->content, ==, IF_NULL_EMPTY(pkg->epoch));
    g_assert_cmpstr(current->properties->next->name, ==, "ver");
    g_assert_cmpstr(current->properties->next->children->content, ==, IF_NULL_EMPTY(pkg->version));
    g_assert_cmpstr(current->properties->next->next->name, ==, "rel");
    g_assert_cmpstr(current->properties->next->next->children->content, ==, IF_NULL_EMPTY(pkg->release));

    current = current->next;
    g_assert_cmpstr(current->name, ==, "checksum");
    g_assert_cmpstr(current->children->content, ==, IF_NULL_EMPTY(pkg->pkgId));
    g_assert_cmpstr(current->properties->name, ==, "type");
    g_assert_cmpstr(current->properties->children->content, ==, IF_NULL_EMPTY(pkg->checksum_type));
    g_assert_cmpstr(current->properties->next->name, ==, "pkgid");
    g_assert_cmpstr(current->properties->next->children->content, ==, "YES" );

    current = current->next;
    g_assert_cmpstr(current->name, ==, "summary");
    g_assert_cmpstr(current->children->content, ==, IF_NULL_EMPTY(pkg->summary));

    current = current->next;
    g_assert_cmpstr(current->name, ==, "description");
    g_assert_cmpstr(current->children->content, ==, IF_NULL_EMPTY(pkg->description));

    current = current->next;
    g_assert_cmpstr(current->name, ==, "packager");
    g_assert_cmpstr(current->children->content, ==, IF_NULL_EMPTY(pkg->rpm_packager));

    current = current->next;
    g_assert_cmpstr(current->name, ==, "url");
    g_assert_cmpstr(current->children->content, ==, IF_NULL_EMPTY(pkg->url));

    current = current->next;
    g_assert_cmpstr(current->name, ==, "time");
    g_assert_cmpstr(current->properties->name, ==, "file");
    gchar *tmp = g_strdup_printf("%i", (gint32) pkg->time_file);
    g_assert_cmpstr(current->properties->children->content, ==, tmp);
    g_free(tmp);
    g_assert_cmpstr(current->properties->next->name, ==, "build");
    tmp = g_strdup_printf("%i", (gint32) pkg->time_build);
    g_assert_cmpstr(current->properties->next->children->content, ==, tmp);
    g_free(tmp);

    current = current->next;
    g_assert_cmpstr(current->name, ==, "size");
    g_assert_cmpstr(current->properties->name, ==, "package");
    tmp = g_strdup_printf("%i", (gint32) pkg->size_package);
    g_assert_cmpstr(current->properties->children->content, ==, tmp);
    g_free(tmp);
    g_assert_cmpstr(current->properties->next->name, ==, "installed");
    tmp = g_strdup_printf("%i", (gint32) pkg->size_installed);
    g_assert_cmpstr(current->properties->next->children->content, ==, tmp);
    g_free(tmp);
    g_assert_cmpstr(current->properties->next->next->name, ==, "archive");
    tmp = g_strdup_printf("%i", (gint32) pkg->size_archive);
    g_assert_cmpstr(current->properties->next->next->children->content, ==, tmp);
    g_free(tmp);

    current = current->next;
    g_assert_cmpstr(current->name, ==, "location");

    xmlAttrPtr current_attrs = current->properties;
    if (pkg->location_base){
        g_assert_cmpstr(current_attrs->name, ==, "xml:base");
        gchar *location_base_with_protocol = NULL;
        location_base_with_protocol = cr_prepend_protocol(pkg->location_base);
        g_assert_cmpstr(current_attrs->children->content, ==, IF_NULL_EMPTY(location_base_with_protocol));
        g_free(location_base_with_protocol);
        current_attrs = current_attrs->next;
    }

    g_assert_cmpstr(current_attrs->name, ==, "href");
    g_assert_cmpstr(current_attrs->children->content, ==, IF_NULL_EMPTY(pkg->location_href));

    current = current->next;
    g_assert_cmpstr(current->name, ==, "format");

    current = current->children;
    g_assert_cmpstr(current->name, ==, "rpm:license");
    g_assert_cmpstr(current->children->content, ==, IF_NULL_EMPTY(pkg->rpm_license));
    current = current->next;
    g_assert_cmpstr(current->name, ==, "rpm:vendor");
    g_assert_cmpstr(current->children->content, ==, IF_NULL_EMPTY(pkg->rpm_vendor));
    current = current->next;
    g_assert_cmpstr(current->name, ==, "rpm:group");
    g_assert_cmpstr(current->children->content, ==, IF_NULL_EMPTY(pkg->rpm_group));
    current = current->next;
    g_assert_cmpstr(current->name, ==, "rpm:buildhost");
    g_assert_cmpstr(current->children->content, ==, IF_NULL_EMPTY(pkg->rpm_buildhost));
    current = current->next;
    g_assert_cmpstr(current->name, ==, "rpm:sourcerpm");
    g_assert_cmpstr(current->children->content, ==, IF_NULL_EMPTY(pkg->rpm_sourcerpm));
    current = current->next;
    g_assert_cmpstr(current->name, ==, "rpm:header-range");
    g_assert_cmpstr(current->properties->name, ==, "start");
    tmp = g_strdup_printf("%i", (gint32) pkg->rpm_header_start);
    g_assert_cmpstr(current->properties->children->content, ==, tmp);
    g_free(tmp);
    g_assert_cmpstr(current->properties->next->name, ==, "end");
    tmp = g_strdup_printf("%i", (gint32) pkg->rpm_header_end);
    g_assert_cmpstr(current->properties->next->children->content, ==, tmp);
    g_free(tmp);

    current = current->next;

    current = cmp_package_pco_and_xml(pkg->provides, current, PCO_TYPE_PROVIDES);
    current = cmp_package_pco_and_xml(pkg->requires, current, PCO_TYPE_REQUIRES);
    current = cmp_package_pco_and_xml(pkg->conflicts, current, PCO_TYPE_CONFLICTS);
    current = cmp_package_pco_and_xml(pkg->obsoletes, current, PCO_TYPE_OBSOLETES);
    current = cmp_package_pco_and_xml(pkg->suggests, current, PCO_TYPE_SUGGESTS);
    current = cmp_package_pco_and_xml(pkg->enhances, current, PCO_TYPE_ENHANCES);
    current = cmp_package_pco_and_xml(pkg->recommends, current, PCO_TYPE_RECOMMENDS);
    current = cmp_package_pco_and_xml(pkg->supplements, current, PCO_TYPE_SUPPLEMENTS);

    //for primary.xml we always want just primary files
    int only_primary_files = 1;
    current = cmp_package_files_and_xml(pkg->files, current, only_primary_files);
}

// Tests

static void
test_cr_xml_dump_primary_dump_pco_00(void)
{
    cr_Package *p;
    p = cr_package_new();
    cr_Dependency *dep;
    GSList *dep_list = NULL;

    dep = cr_dependency_new();
    dep->name = "foobar_provide";
    dep->flags = NULL;
    dep->pre = FALSE;
    p->requires = (g_slist_prepend(p->requires, dep));

    dep = cr_dependency_new();
    dep->name = "foobar_provide";
    dep->flags = "LE";
    dep->pre = 1;
    dep->epoch = "44";
    dep->version = "1.2.3";
    p->requires = (g_slist_prepend(p->requires, dep));

    xmlNodePtr node;
    node = xmlNewNode(NULL, BAD_CAST "wrapper");
    cr_xml_dump_primary_dump_pco(node, p, PCO_TYPE_REQUIRES);
    node = node->children;
    node = cmp_package_pco_and_xml(p->requires, node, PCO_TYPE_REQUIRES);
}

static void
test_cr_xml_dump_primary_dump_pco_01(void)
{
    cr_Package *p;
    p = cr_package_new();
    cr_Dependency *dep;
    GSList *dep_list = NULL;

    dep = cr_dependency_new();
    dep->name = "foobar_provide";
    dep->flags = NULL;
    dep->pre = FALSE;
    p->requires = (g_slist_prepend(p->requires, dep));

    dep = cr_dependency_new();
    dep->name = "foobar_provide";
    dep->flags = "LE";
    dep->pre = 1;
    dep->epoch = "44";
    dep->version = "1.2.3";
    p->requires = (g_slist_prepend(p->requires, dep));

    dep = cr_dependency_new();
    dep->name = "foobar_provide";
    dep->flags = NULL;
    dep->pre = 0;
    dep->epoch = "44";
    dep->version = "1.2.3";
    p->requires = (g_slist_prepend(p->requires, dep));

    dep = cr_dependency_new();
    dep->name = "foobar_provide";
    dep->flags = "LE";
    dep->pre = 1;
    dep->epoch = "44";
    dep->version = "1.2.3";
    p->obsoletes = (g_slist_prepend(p->obsoletes, dep));

    dep = cr_dependency_new();
    dep->name = "foobar_provide";
    dep->flags = "";
    dep->pre = 0;
    dep->epoch = "12";
    dep->version = "1.2.3";
    p->obsoletes = (g_slist_prepend(p->obsoletes, dep));

    xmlNodePtr node;
    node = xmlNewNode(NULL, BAD_CAST "wrapper");

    cr_xml_dump_primary_dump_pco(node, p, PCO_TYPE_REQUIRES);
    cr_xml_dump_primary_dump_pco(node, p, PCO_TYPE_OBSOLETES);

    node = node->children;
    node = cmp_package_pco_and_xml(p->requires, node, PCO_TYPE_REQUIRES);
    node = cmp_package_pco_and_xml(p->obsoletes, node, PCO_TYPE_OBSOLETES);
}

static void
test_cr_xml_dump_primary_base_items_00(void)
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "package");
    cr_Package *pkg = NULL;

    pkg = get_package();

    g_assert(pkg);

    cr_xml_dump_primary_base_items(node, pkg);
    cmp_package_and_xml_node(pkg, node);

    cr_package_free(pkg);
}

static void
test_cr_xml_dump_primary_base_items_01(void)
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "package");
    cr_Package *pkg = NULL;

    pkg = get_package();
    pkg->location_base = "http://url/";

    g_assert(pkg);

    cr_xml_dump_primary_base_items(node, pkg);
    cmp_package_and_xml_node(pkg, node);

    cr_package_free(pkg);
}

static void
test_cr_xml_dump_primary_base_items_02(void)
{
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "package");
    cr_Package *pkg = NULL;

    pkg = get_empty_package();

    g_assert(pkg);

    cr_xml_dump_primary_base_items(node, pkg);
    cmp_package_and_xml_node(pkg, node);

    cr_package_free(pkg);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/xml_dump_primary/test_cr_xml_dump_primary_base_items_00",
                    test_cr_xml_dump_primary_base_items_00);
    g_test_add_func("/xml_dump_primary/test_cr_xml_dump_primary_base_items_01",
                    test_cr_xml_dump_primary_base_items_02);
    g_test_add_func("/xml_dump_primary/test_cr_xml_dump_primary_base_items_02",
                    test_cr_xml_dump_primary_base_items_01);

    g_test_add_func("/xml_dump_primary/test_cr_xml_dump_primary_dump_pco_00",
                    test_cr_xml_dump_primary_dump_pco_00);
    g_test_add_func("/xml_dump_primary/test_cr_xml_dump_primary_dump_pco_01",
                    test_cr_xml_dump_primary_dump_pco_01);
    return g_test_run();
}
