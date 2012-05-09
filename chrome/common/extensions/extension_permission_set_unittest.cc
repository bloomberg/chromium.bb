// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_permission_set.h"

#include "base/command_line.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;
namespace values = extension_manifest_values;
namespace {

static scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                             const std::string& test_file,
                                             int extra_flags) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions")
             .AppendASCII(dir)
             .AppendASCII(test_file);

  JSONFileValueSerializer serializer(path);
  std::string error;
  scoped_ptr<Value> result(serializer.Deserialize(NULL, &error));
  if (!result.get()) {
    EXPECT_EQ("", error);
    return NULL;
  }

  scoped_refptr<Extension> extension = Extension::Create(
      path.DirName(), Extension::INVALID,
      *static_cast<DictionaryValue*>(result.get()),
      Extension::STRICT_ERROR_CHECKS | extra_flags, &error);
  EXPECT_TRUE(extension) << error;
  return extension;
}

static scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                             const std::string& test_file) {
  return LoadManifest(dir, test_file, Extension::NO_FLAGS);
}

void CompareLists(const std::vector<std::string>& expected,
                  const std::vector<std::string>& actual) {
  ASSERT_EQ(expected.size(), actual.size());

  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

} // namespace

class ExtensionPermissionsTest : public testing::Test {
};

// Tests GetByID.
TEST(ExtensionPermissionsTest, GetByID) {
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  ExtensionAPIPermissionSet ids = info->GetAll();
  for (ExtensionAPIPermissionSet::iterator i = ids.begin();
       i != ids.end(); ++i) {
    EXPECT_EQ(*i, info->GetByID(*i)->id());
  }
}

// Tests that GetByName works with normal permission names and aliases.
TEST(ExtensionPermissionsTest, GetByName) {
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  EXPECT_EQ(ExtensionAPIPermission::kTab, info->GetByName("tabs")->id());
  EXPECT_EQ(ExtensionAPIPermission::kManagement,
            info->GetByName("management")->id());
  EXPECT_FALSE(info->GetByName("alsdkfjasldkfj"));
}

TEST(ExtensionPermissionsTest, GetAll) {
  size_t count = 0;
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  ExtensionAPIPermissionSet apis = info->GetAll();
  for (ExtensionAPIPermissionSet::iterator api = apis.begin();
       api != apis.end(); ++api) {
    // Make sure only the valid permission IDs get returned.
    EXPECT_NE(ExtensionAPIPermission::kInvalid, *api);
    EXPECT_NE(ExtensionAPIPermission::kUnknown, *api);
    count++;
  }
  EXPECT_EQ(count, info->get_permission_count());
}

TEST(ExtensionPermissionsTest, GetAllByName) {
  std::set<std::string> names;
  names.insert("background");
  names.insert("management");

  // This is an alias of kTab
  names.insert("windows");

  // This unknown name should get dropped.
  names.insert("sdlkfjasdlkfj");

  ExtensionAPIPermissionSet expected;
  expected.insert(ExtensionAPIPermission::kBackground);
  expected.insert(ExtensionAPIPermission::kManagement);
  expected.insert(ExtensionAPIPermission::kTab);

  EXPECT_EQ(expected,
            ExtensionPermissionsInfo::GetInstance()->GetAllByName(names));
}

// Tests that the aliases are properly mapped.
TEST(ExtensionPermissionsTest, Aliases) {
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  // tabs: tabs, windows
  std::string tabs_name = "tabs";
  EXPECT_EQ(tabs_name, info->GetByID(ExtensionAPIPermission::kTab)->name());
  EXPECT_EQ(ExtensionAPIPermission::kTab, info->GetByName("tabs")->id());
  EXPECT_EQ(ExtensionAPIPermission::kTab, info->GetByName("windows")->id());

  // unlimitedStorage: unlimitedStorage, unlimited_storage
  std::string storage_name = "unlimitedStorage";
  EXPECT_EQ(storage_name, info->GetByID(
      ExtensionAPIPermission::kUnlimitedStorage)->name());
  EXPECT_EQ(ExtensionAPIPermission::kUnlimitedStorage,
            info->GetByName("unlimitedStorage")->id());
  EXPECT_EQ(ExtensionAPIPermission::kUnlimitedStorage,
            info->GetByName("unlimited_storage")->id());
}

TEST(ExtensionPermissionsTest, EffectiveHostPermissions) {
  scoped_refptr<Extension> extension;
  scoped_refptr<const ExtensionPermissionSet> permissions;

  extension = LoadManifest("effective_host_permissions", "empty.json");
  permissions = extension->GetActivePermissions();
  EXPECT_EQ(0u, extension->GetEffectiveHostPermissions().patterns().size());
  EXPECT_FALSE(permissions->HasEffectiveAccessToURL(
      GURL("http://www.google.com")));
  EXPECT_FALSE(permissions->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "one_host.json");
  permissions = extension->GetActivePermissions();
  EXPECT_TRUE(permissions->HasEffectiveAccessToURL(
      GURL("http://www.google.com")));
  EXPECT_FALSE(permissions->HasEffectiveAccessToURL(
      GURL("https://www.google.com")));
  EXPECT_FALSE(permissions->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "one_host_wildcard.json");
  permissions = extension->GetActivePermissions();
  EXPECT_TRUE(permissions->HasEffectiveAccessToURL(GURL("http://google.com")));
  EXPECT_TRUE(permissions->HasEffectiveAccessToURL(
      GURL("http://foo.google.com")));
  EXPECT_FALSE(permissions->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "two_hosts.json");
  permissions = extension->GetActivePermissions();
  EXPECT_TRUE(permissions->HasEffectiveAccessToURL(
      GURL("http://www.google.com")));
  EXPECT_TRUE(permissions->HasEffectiveAccessToURL(
      GURL("http://www.reddit.com")));
  EXPECT_FALSE(permissions->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "https_not_considered.json");
  permissions = extension->GetActivePermissions();
  EXPECT_TRUE(permissions->HasEffectiveAccessToURL(GURL("http://google.com")));
  EXPECT_TRUE(permissions->HasEffectiveAccessToURL(GURL("https://google.com")));
  EXPECT_FALSE(permissions->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "two_content_scripts.json");
  permissions = extension->GetActivePermissions();
  EXPECT_TRUE(permissions->HasEffectiveAccessToURL(GURL("http://google.com")));
  EXPECT_TRUE(permissions->HasEffectiveAccessToURL(
      GURL("http://www.reddit.com")));
  EXPECT_TRUE(permissions->HasEffectiveAccessToURL(
      GURL("http://news.ycombinator.com")));
  EXPECT_FALSE(permissions->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts.json");
  permissions = extension->GetActivePermissions();
  EXPECT_TRUE(permissions->HasEffectiveAccessToURL(GURL("http://test/")));
  EXPECT_FALSE(permissions->HasEffectiveAccessToURL(GURL("https://test/")));
  EXPECT_TRUE(
      permissions->HasEffectiveAccessToURL(GURL("http://www.google.com")));
  EXPECT_TRUE(permissions->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts2.json");
  permissions = extension->GetActivePermissions();
  EXPECT_TRUE(permissions->HasEffectiveAccessToURL(GURL("http://test/")));
  EXPECT_TRUE(
      permissions->HasEffectiveAccessToURL(GURL("http://www.google.com")));
  EXPECT_TRUE(permissions->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts3.json");
  permissions = extension->GetActivePermissions();
  EXPECT_FALSE(permissions->HasEffectiveAccessToURL(GURL("http://test/")));
  EXPECT_TRUE(permissions->HasEffectiveAccessToURL(GURL("https://test/")));
  EXPECT_TRUE(
      permissions->HasEffectiveAccessToURL(GURL("http://www.google.com")));
  EXPECT_TRUE(permissions->HasEffectiveAccessToAllHosts());
}

TEST(ExtensionPermissionsTest, ExplicitAccessToOrigin) {
  ExtensionAPIPermissionSet apis;
  URLPatternSet explicit_hosts;
  URLPatternSet scriptable_hosts;

  AddPattern(&explicit_hosts, "http://*.google.com/*");
  // The explicit host paths should get set to /*.
  AddPattern(&explicit_hosts, "http://www.example.com/a/particular/path/*");

  scoped_refptr<ExtensionPermissionSet> perm_set = new ExtensionPermissionSet(
      apis, explicit_hosts, scriptable_hosts);
  ASSERT_TRUE(perm_set->HasExplicitAccessToOrigin(
      GURL("http://www.google.com/")));
  ASSERT_TRUE(perm_set->HasExplicitAccessToOrigin(
      GURL("http://test.google.com/")));
  ASSERT_TRUE(perm_set->HasExplicitAccessToOrigin(
      GURL("http://www.example.com")));
  ASSERT_TRUE(perm_set->HasEffectiveAccessToURL(
      GURL("http://www.example.com")));
  ASSERT_FALSE(perm_set->HasExplicitAccessToOrigin(
      GURL("http://test.example.com")));
}

TEST(ExtensionPermissionsTest, CreateUnion) {
  ExtensionAPIPermissionSet apis1;
  ExtensionAPIPermissionSet apis2;
  ExtensionAPIPermissionSet expected_apis;

  URLPatternSet explicit_hosts1;
  URLPatternSet explicit_hosts2;
  URLPatternSet expected_explicit_hosts;

  URLPatternSet scriptable_hosts1;
  URLPatternSet scriptable_hosts2;
  URLPatternSet expected_scriptable_hosts;

  ExtensionOAuth2Scopes scopes1;
  ExtensionOAuth2Scopes scopes2;
  ExtensionOAuth2Scopes expected_scopes;

  URLPatternSet effective_hosts;

  scoped_refptr<ExtensionPermissionSet> set1;
  scoped_refptr<ExtensionPermissionSet> set2;
  scoped_refptr<ExtensionPermissionSet> union_set;

  // Union with an empty set.
  apis1.insert(ExtensionAPIPermission::kTab);
  apis1.insert(ExtensionAPIPermission::kBackground);
  expected_apis.insert(ExtensionAPIPermission::kTab);
  expected_apis.insert(ExtensionAPIPermission::kBackground);

  AddPattern(&explicit_hosts1, "http://*.google.com/*");
  AddPattern(&expected_explicit_hosts, "http://*.google.com/*");
  AddPattern(&effective_hosts, "http://*.google.com/*");

  scopes1.insert("first-scope");
  scopes1.insert("second-scope");
  expected_scopes.insert("first-scope");
  expected_scopes.insert("second-scope");

  set1 = new ExtensionPermissionSet(
      apis1, explicit_hosts1, scriptable_hosts1, scopes1);
  set2 = new ExtensionPermissionSet(
      apis2, explicit_hosts2, scriptable_hosts2, scopes2);
  union_set = ExtensionPermissionSet::CreateUnion(set1.get(), set2.get());
  EXPECT_TRUE(set1->Contains(*set2));
  EXPECT_TRUE(set1->Contains(*union_set));
  EXPECT_FALSE(set2->Contains(*set1));
  EXPECT_FALSE(set2->Contains(*union_set));
  EXPECT_TRUE(union_set->Contains(*set1));
  EXPECT_TRUE(union_set->Contains(*set2));

  EXPECT_FALSE(union_set->HasEffectiveFullAccess());
  EXPECT_EQ(expected_apis, union_set->apis());
  EXPECT_EQ(expected_explicit_hosts, union_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, union_set->scriptable_hosts());
  EXPECT_EQ(expected_explicit_hosts, union_set->effective_hosts());
  EXPECT_EQ(expected_scopes, union_set->scopes());

  // Now use a real second set.
  apis2.insert(ExtensionAPIPermission::kTab);
  apis2.insert(ExtensionAPIPermission::kProxy);
  apis2.insert(ExtensionAPIPermission::kClipboardWrite);
  apis2.insert(ExtensionAPIPermission::kPlugin);
  expected_apis.insert(ExtensionAPIPermission::kTab);
  expected_apis.insert(ExtensionAPIPermission::kProxy);
  expected_apis.insert(ExtensionAPIPermission::kClipboardWrite);
  expected_apis.insert(ExtensionAPIPermission::kPlugin);

  AddPattern(&explicit_hosts2, "http://*.example.com/*");
  AddPattern(&scriptable_hosts2, "http://*.google.com/*");
  AddPattern(&expected_explicit_hosts, "http://*.example.com/*");
  AddPattern(&expected_scriptable_hosts, "http://*.google.com/*");

  scopes2.insert("real-scope");
  scopes2.insert("anotherscope");
  expected_scopes.insert("real-scope");
  expected_scopes.insert("anotherscope");

  URLPatternSet::CreateUnion(
      explicit_hosts2, scriptable_hosts2, &effective_hosts);

  set2 = new ExtensionPermissionSet(
      apis2, explicit_hosts2, scriptable_hosts2, scopes2);
  union_set = ExtensionPermissionSet::CreateUnion(set1.get(), set2.get());

  EXPECT_FALSE(set1->Contains(*set2));
  EXPECT_FALSE(set1->Contains(*union_set));
  EXPECT_FALSE(set2->Contains(*set1));
  EXPECT_FALSE(set2->Contains(*union_set));
  EXPECT_TRUE(union_set->Contains(*set1));
  EXPECT_TRUE(union_set->Contains(*set2));

  EXPECT_TRUE(union_set->HasEffectiveFullAccess());
  EXPECT_TRUE(union_set->HasEffectiveAccessToAllHosts());
  EXPECT_EQ(expected_apis, union_set->apis());
  EXPECT_EQ(expected_explicit_hosts, union_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, union_set->scriptable_hosts());
  EXPECT_EQ(expected_scopes, union_set->scopes());
  EXPECT_EQ(effective_hosts, union_set->effective_hosts());
}

TEST(ExtensionPermissionsTest, CreateIntersection) {
  ExtensionAPIPermissionSet apis1;
  ExtensionAPIPermissionSet apis2;
  ExtensionAPIPermissionSet expected_apis;

  URLPatternSet explicit_hosts1;
  URLPatternSet explicit_hosts2;
  URLPatternSet expected_explicit_hosts;

  URLPatternSet scriptable_hosts1;
  URLPatternSet scriptable_hosts2;
  URLPatternSet expected_scriptable_hosts;

  URLPatternSet effective_hosts;

  scoped_refptr<ExtensionPermissionSet> set1;
  scoped_refptr<ExtensionPermissionSet> set2;
  scoped_refptr<ExtensionPermissionSet> new_set;

  // Intersection with an empty set.
  apis1.insert(ExtensionAPIPermission::kTab);
  apis1.insert(ExtensionAPIPermission::kBackground);

  AddPattern(&explicit_hosts1, "http://*.google.com/*");
  AddPattern(&scriptable_hosts1, "http://www.reddit.com/*");

  set1 = new ExtensionPermissionSet(apis1, explicit_hosts1, scriptable_hosts1);
  set2 = new ExtensionPermissionSet(apis2, explicit_hosts2, scriptable_hosts2);
  new_set = ExtensionPermissionSet::CreateIntersection(set1.get(), set2.get());
  EXPECT_TRUE(set1->Contains(*new_set));
  EXPECT_TRUE(set2->Contains(*new_set));
  EXPECT_TRUE(set1->Contains(*set2));
  EXPECT_FALSE(set2->Contains(*set1));
  EXPECT_FALSE(new_set->Contains(*set1));
  EXPECT_TRUE(new_set->Contains(*set2));

  EXPECT_TRUE(new_set->IsEmpty());
  EXPECT_FALSE(new_set->HasEffectiveFullAccess());
  EXPECT_EQ(expected_apis, new_set->apis());
  EXPECT_EQ(expected_explicit_hosts, new_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, new_set->scriptable_hosts());
  EXPECT_EQ(expected_explicit_hosts, new_set->effective_hosts());

  // Now use a real second set.
  apis2.insert(ExtensionAPIPermission::kTab);
  apis2.insert(ExtensionAPIPermission::kProxy);
  apis2.insert(ExtensionAPIPermission::kClipboardWrite);
  apis2.insert(ExtensionAPIPermission::kPlugin);
  expected_apis.insert(ExtensionAPIPermission::kTab);

  AddPattern(&explicit_hosts2, "http://*.example.com/*");
  AddPattern(&explicit_hosts2, "http://*.google.com/*");
  AddPattern(&scriptable_hosts2, "http://*.google.com/*");
  AddPattern(&expected_explicit_hosts, "http://*.google.com/*");

  effective_hosts.ClearPatterns();
  AddPattern(&effective_hosts, "http://*.google.com/*");

  set2 = new ExtensionPermissionSet(apis2, explicit_hosts2, scriptable_hosts2);
  new_set = ExtensionPermissionSet::CreateIntersection(set1.get(), set2.get());

  EXPECT_TRUE(set1->Contains(*new_set));
  EXPECT_TRUE(set2->Contains(*new_set));
  EXPECT_FALSE(set1->Contains(*set2));
  EXPECT_FALSE(set2->Contains(*set1));
  EXPECT_FALSE(new_set->Contains(*set1));
  EXPECT_FALSE(new_set->Contains(*set2));

  EXPECT_FALSE(new_set->HasEffectiveFullAccess());
  EXPECT_FALSE(new_set->HasEffectiveAccessToAllHosts());
  EXPECT_EQ(expected_apis, new_set->apis());
  EXPECT_EQ(expected_explicit_hosts, new_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, new_set->scriptable_hosts());
  EXPECT_EQ(effective_hosts, new_set->effective_hosts());
}

TEST(ExtensionPermissionsTest, CreateDifference) {
  ExtensionAPIPermissionSet apis1;
  ExtensionAPIPermissionSet apis2;
  ExtensionAPIPermissionSet expected_apis;

  URLPatternSet explicit_hosts1;
  URLPatternSet explicit_hosts2;
  URLPatternSet expected_explicit_hosts;

  URLPatternSet scriptable_hosts1;
  URLPatternSet scriptable_hosts2;
  URLPatternSet expected_scriptable_hosts;

  ExtensionOAuth2Scopes scopes1;
  ExtensionOAuth2Scopes scopes2;
  ExtensionOAuth2Scopes expected_scopes;

  URLPatternSet effective_hosts;

  scoped_refptr<ExtensionPermissionSet> set1;
  scoped_refptr<ExtensionPermissionSet> set2;
  scoped_refptr<ExtensionPermissionSet> new_set;

  // Difference with an empty set.
  apis1.insert(ExtensionAPIPermission::kTab);
  apis1.insert(ExtensionAPIPermission::kBackground);

  AddPattern(&explicit_hosts1, "http://*.google.com/*");
  AddPattern(&scriptable_hosts1, "http://www.reddit.com/*");

  scopes1.insert("my-scope");
  scopes1.insert("other-scope");

  set1 = new ExtensionPermissionSet(
      apis1, explicit_hosts1, scriptable_hosts1, scopes1);
  set2 = new ExtensionPermissionSet(
      apis2, explicit_hosts2, scriptable_hosts2, scopes2);
  new_set = ExtensionPermissionSet::CreateDifference(set1.get(), set2.get());
  EXPECT_EQ(*set1, *new_set);

  // Now use a real second set.
  apis2.insert(ExtensionAPIPermission::kTab);
  apis2.insert(ExtensionAPIPermission::kProxy);
  apis2.insert(ExtensionAPIPermission::kClipboardWrite);
  apis2.insert(ExtensionAPIPermission::kPlugin);
  expected_apis.insert(ExtensionAPIPermission::kBackground);

  AddPattern(&explicit_hosts2, "http://*.example.com/*");
  AddPattern(&explicit_hosts2, "http://*.google.com/*");
  AddPattern(&scriptable_hosts2, "http://*.google.com/*");
  AddPattern(&expected_scriptable_hosts, "http://www.reddit.com/*");

  scopes2.insert("other-scope");
  expected_scopes.insert("my-scope");

  effective_hosts.ClearPatterns();
  AddPattern(&effective_hosts, "http://www.reddit.com/*");

  set2 = new ExtensionPermissionSet(
      apis2, explicit_hosts2, scriptable_hosts2, scopes2);
  new_set = ExtensionPermissionSet::CreateDifference(set1.get(), set2.get());

  EXPECT_TRUE(set1->Contains(*new_set));
  EXPECT_FALSE(set2->Contains(*new_set));

  EXPECT_FALSE(new_set->HasEffectiveFullAccess());
  EXPECT_FALSE(new_set->HasEffectiveAccessToAllHosts());
  EXPECT_EQ(expected_apis, new_set->apis());
  EXPECT_EQ(expected_explicit_hosts, new_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, new_set->scriptable_hosts());
  EXPECT_EQ(expected_scopes, new_set->scopes());
  EXPECT_EQ(effective_hosts, new_set->effective_hosts());

  // |set3| = |set1| - |set2| --> |set3| intersect |set2| == empty_set
  set1 = ExtensionPermissionSet::CreateIntersection(new_set.get(), set2.get());
  EXPECT_TRUE(set1->IsEmpty());
}

TEST(ExtensionPermissionsTest, HasLessPrivilegesThan) {
  const struct {
    const char* base_name;
    bool expect_increase;
  } kTests[] = {
    { "allhosts1", false },  // all -> all
    { "allhosts2", false },  // all -> one
    { "allhosts3", true },  // one -> all
    { "hosts1", false },  // http://a,http://b -> http://a,http://b
    { "hosts2", true },  // http://a,http://b -> https://a,http://*.b
    { "hosts3", false },  // http://a,http://b -> http://a
    { "hosts4", true },  // http://a -> http://a,http://b
    { "hosts5", false },  // http://a,b,c -> http://a,b,c + https://a,b,c
    { "hosts6", false },  // http://a.com -> http://a.com + http://a.co.uk
    { "permissions1", false },  // tabs -> tabs
    { "permissions2", true },  // tabs -> tabs,bookmarks
    { "permissions3", true },  // http://a -> http://a,tabs
    { "permissions5", true },  // bookmarks -> bookmarks,history
    { "equivalent_warnings", false },  // tabs --> tabs, webNavigation
#if !defined(OS_CHROMEOS)  // plugins aren't allowed in ChromeOS
    { "permissions4", false },  // plugin -> plugin,tabs
    { "plugin1", false },  // plugin -> plugin
    { "plugin2", false },  // plugin -> none
    { "plugin3", true },  // none -> plugin
#endif
    { "storage", false },  // none -> storage
    { "notifications", false },  // none -> notifications
    { "scopes1", true },  // scope1 -> scope1,scope2
    { "scopes2", false },  // scope1,scope2 -> scope1
    { "scopes3", true },  // none -> scope1
  };

  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnablePlatformApps);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTests); ++i) {
    scoped_refptr<Extension> old_extension(
        LoadManifest("allow_silent_upgrade",
                     std::string(kTests[i].base_name) + "_old.json"));
    scoped_refptr<Extension> new_extension(
        LoadManifest("allow_silent_upgrade",
                     std::string(kTests[i].base_name) + "_new.json"));

    EXPECT_TRUE(new_extension.get()) << kTests[i].base_name << "_new.json";
    if (!new_extension.get())
      continue;

    scoped_refptr<const ExtensionPermissionSet> old_p(
        old_extension->GetActivePermissions());
    scoped_refptr<const ExtensionPermissionSet> new_p(
        new_extension->GetActivePermissions());

    EXPECT_EQ(kTests[i].expect_increase,
              old_p->HasLessPrivilegesThan(new_p)) << kTests[i].base_name;
  }
}

TEST(ExtensionPermissionsTest, PermissionMessages) {
  // Ensure that all permissions that needs to show install UI actually have
  // strings associated with them.
  ExtensionAPIPermissionSet skip;

  // These are considered "nuisance" or "trivial" permissions that don't need
  // a prompt.
  skip.insert(ExtensionAPIPermission::kAlarms);
  skip.insert(ExtensionAPIPermission::kAppNotifications);
  skip.insert(ExtensionAPIPermission::kBrowsingData);
  skip.insert(ExtensionAPIPermission::kContextMenus);
  skip.insert(ExtensionAPIPermission::kDeclarative);
  skip.insert(ExtensionAPIPermission::kIdle);
  skip.insert(ExtensionAPIPermission::kNotification);
  skip.insert(ExtensionAPIPermission::kUnlimitedStorage);
  skip.insert(ExtensionAPIPermission::kStorage);
  skip.insert(ExtensionAPIPermission::kTts);

  // TODO(erikkay) add a string for this permission.
  skip.insert(ExtensionAPIPermission::kBackground);

  skip.insert(ExtensionAPIPermission::kClipboardWrite);

  // The cookie permission does nothing unless you have associated host
  // permissions.
  skip.insert(ExtensionAPIPermission::kCookie);

  // The ime, proxy, and webRequest permissions are warned as part of host
  // permission checks.
  skip.insert(ExtensionAPIPermission::kInput);
  skip.insert(ExtensionAPIPermission::kProxy);
  skip.insert(ExtensionAPIPermission::kWebRequest);
  skip.insert(ExtensionAPIPermission::kWebRequestBlocking);
  skip.insert(ExtensionAPIPermission::kDeclarativeWebRequest);


  // This permission requires explicit user action (context menu handler)
  // so we won't prompt for it for now.
  skip.insert(ExtensionAPIPermission::kFileBrowserHandler);

  // This permission requires explicit user action (shortcut) so we don't
  // prompt for it.
  skip.insert(ExtensionAPIPermission::kKeybinding);

  // If you've turned on the experimental command-line flag, we don't need
  // to warn you further.
  skip.insert(ExtensionAPIPermission::kExperimental);

  // These are private.
  skip.insert(ExtensionAPIPermission::kChromeAuthPrivate);
  skip.insert(ExtensionAPIPermission::kChromeosInfoPrivate);
  skip.insert(ExtensionAPIPermission::kFileBrowserPrivate);
  skip.insert(ExtensionAPIPermission::kInputMethodPrivate);
  skip.insert(ExtensionAPIPermission::kManagedModePrivate);
  skip.insert(ExtensionAPIPermission::kMediaPlayerPrivate);
  skip.insert(ExtensionAPIPermission::kMetricsPrivate);
  skip.insert(ExtensionAPIPermission::kEchoPrivate);
  skip.insert(ExtensionAPIPermission::kSystemPrivate);
  skip.insert(ExtensionAPIPermission::kTerminalPrivate);
  skip.insert(ExtensionAPIPermission::kWebSocketProxyPrivate);
  skip.insert(ExtensionAPIPermission::kWebstorePrivate);

  // Warned as part of host permissions.
  skip.insert(ExtensionAPIPermission::kDevtools);

  // Platform apps. TODO(miket): must we skip?
  skip.insert(ExtensionAPIPermission::kSocket);
  skip.insert(ExtensionAPIPermission::kUsb);

  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  ExtensionAPIPermissionSet permissions = info->GetAll();
  for (ExtensionAPIPermissionSet::const_iterator i = permissions.begin();
       i != permissions.end(); ++i) {
    ExtensionAPIPermission* permission = info->GetByID(*i);
    EXPECT_TRUE(permission);
    if (skip.count(*i)) {
      EXPECT_EQ(ExtensionPermissionMessage::kNone, permission->message_id())
          << "unexpected message_id for " << permission->name();
    } else {
      EXPECT_NE(ExtensionPermissionMessage::kNone, permission->message_id())
          << "missing message_id for " << permission->name();
    }
  }
}

// Tests the default permissions (empty API permission set).
TEST(ExtensionPermissionsTest, DefaultFunctionAccess) {
  const struct {
    const char* permission_name;
    bool expect_success;
  } kTests[] = {
    // Negative test.
    { "non_existing_permission", false },
    // Test default module/package permission.
    { "browserAction",  true },
    { "devtools",       true },
    { "extension",      true },
    { "i18n",           true },
    { "pageAction",     true },
    { "pageActions",    true },
    { "test",           true },
    // Some negative tests.
    { "bookmarks",      false },
    { "cookies",        false },
    { "history",        false },
    { "tabs.onUpdated", false },
    // Make sure we find the module name after stripping '.' and '/'.
    { "browserAction/abcd/onClick",  true },
    { "browserAction.abcd.onClick",  true },
    // Test Tabs functions.
    { "tabs.create",      true},
    { "tabs.update",      true},
    { "tabs.getSelected", false},
  };

  scoped_refptr<ExtensionPermissionSet> empty = new ExtensionPermissionSet();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTests); ++i) {
    EXPECT_EQ(kTests[i].expect_success,
              empty->HasAccessToFunction(kTests[i].permission_name));
  }
}

// Tests the default permissions (empty API permission set).
TEST(ExtensionPermissionSetTest, DefaultAnyAPIAccess) {
  const struct {
    const char* api_name;
    bool expect_success;
  } kTests[] = {
    // Negative test.
    { "non_existing_permission", false },
    // Test default module/package permission.
    { "browserAction",  true },
    { "devtools",       true },
    { "extension",      true },
    { "i18n",           true },
    { "pageAction",     true },
    { "pageActions",    true },
    { "test",           true },
    // Some negative tests.
    { "bookmarks",      false },
    { "cookies",        false },
    { "history",        false },
    // Negative APIs that have positive individual functions.
    { "management",     true},
    { "tabs",           true},
  };

  scoped_refptr<ExtensionPermissionSet> empty = new ExtensionPermissionSet();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTests); ++i) {
    EXPECT_EQ(kTests[i].expect_success,
              empty->HasAnyAccessToAPI(kTests[i].api_name));
  }
}

TEST(ExtensionPermissionsTest, GetWarningMessages_ManyHosts) {
  scoped_refptr<Extension> extension;

  extension = LoadManifest("permissions", "many-hosts.json");
  std::vector<string16> warnings =
      extension->GetActivePermissions()->GetWarningMessages();
  ASSERT_EQ(1u, warnings.size());
  EXPECT_EQ("Your data on encrypted.google.com and www.google.com",
            UTF16ToUTF8(warnings[0]));
}

TEST(ExtensionPermissionsTest, GetWarningMessages_Plugins) {
  scoped_refptr<Extension> extension;
  scoped_refptr<ExtensionPermissionSet> permissions;

  extension = LoadManifest("permissions", "plugins.json");
  std::vector<string16> warnings =
      extension->GetActivePermissions()->GetWarningMessages();
  // We don't parse the plugins key on Chrome OS, so it should not ask for any
  // permissions.
#if defined(OS_CHROMEOS)
  ASSERT_EQ(0u, warnings.size());
#else
  ASSERT_EQ(1u, warnings.size());
  EXPECT_EQ("All data on your computer and the websites you visit",
            UTF16ToUTF8(warnings[0]));
#endif
}

TEST(ExtensionPermissionsTest, GetDistinctHostsForDisplay) {
  scoped_refptr<ExtensionPermissionSet> perm_set;
  ExtensionAPIPermissionSet empty_perms;
  std::set<std::string> expected;
  expected.insert("www.foo.com");
  expected.insert("www.bar.com");
  expected.insert("www.baz.com");
  URLPatternSet explicit_hosts;
  URLPatternSet scriptable_hosts;

  {
    SCOPED_TRACE("no dupes");

    // Simple list with no dupes.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.bar.com/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.baz.com/path"));
    perm_set = new ExtensionPermissionSet(
        empty_perms, explicit_hosts, scriptable_hosts);
    EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
  }

  {
    SCOPED_TRACE("two dupes");

    // Add some dupes.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.baz.com/path"));
    perm_set = new ExtensionPermissionSet(
        empty_perms, explicit_hosts, scriptable_hosts);
    EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
  }

  {
    SCOPED_TRACE("schemes differ");

    // Add a pattern that differs only by scheme. This should be filtered out.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTPS, "https://www.bar.com/path"));
    perm_set = new ExtensionPermissionSet(
        empty_perms, explicit_hosts, scriptable_hosts);
    EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
  }

  {
    SCOPED_TRACE("paths differ");

    // Add some dupes by path.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.bar.com/pathypath"));
    perm_set = new ExtensionPermissionSet(
        empty_perms, explicit_hosts, scriptable_hosts);
    EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
  }

  {
    SCOPED_TRACE("subdomains differ");

    // We don't do anything special for subdomains.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://monkey.www.bar.com/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://bar.com/path"));

    expected.insert("monkey.www.bar.com");
    expected.insert("bar.com");

    perm_set = new ExtensionPermissionSet(
        empty_perms, explicit_hosts, scriptable_hosts);
    EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
  }

  {
    SCOPED_TRACE("RCDs differ");

    // Now test for RCD uniquing.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.co.uk/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.de/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.ca.us/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.net/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com.my/path"));

    // This is an unknown RCD, which shouldn't be uniqued out.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.xyzzy/path"));
    // But it should only occur once.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.xyzzy/path"));

    expected.insert("www.foo.xyzzy");

    perm_set = new ExtensionPermissionSet(
        empty_perms, explicit_hosts, scriptable_hosts);
    EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
  }

  {
    SCOPED_TRACE("wildcards");

    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://*.google.com/*"));

    expected.insert("*.google.com");

    perm_set = new ExtensionPermissionSet(
        empty_perms, explicit_hosts, scriptable_hosts);
    EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
  }

  {
    SCOPED_TRACE("scriptable hosts");
    explicit_hosts.ClearPatterns();
    scriptable_hosts.ClearPatterns();
    expected.clear();

    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://*.google.com/*"));
    scriptable_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://*.example.com/*"));

    expected.insert("*.google.com");
    expected.insert("*.example.com");

    perm_set = new ExtensionPermissionSet(
        empty_perms, explicit_hosts, scriptable_hosts);
    EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
  }

  {
    // We don't display warnings for file URLs because they are off by default.
    SCOPED_TRACE("file urls");
    explicit_hosts.ClearPatterns();
    scriptable_hosts.ClearPatterns();
    expected.clear();

    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_FILE, "file:///*"));

    perm_set = new ExtensionPermissionSet(
        empty_perms, explicit_hosts, scriptable_hosts);
    EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
  }
}

TEST(ExtensionPermissionsTest, GetDistinctHostsForDisplay_ComIsBestRcd) {
  scoped_refptr<ExtensionPermissionSet> perm_set;
  ExtensionAPIPermissionSet empty_perms;
  URLPatternSet explicit_hosts;
  URLPatternSet scriptable_hosts;
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.ca/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.org/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.co.uk/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.net/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.jp/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com/path"));

  std::set<std::string> expected;
  expected.insert("www.foo.com");
  perm_set = new ExtensionPermissionSet(
      empty_perms, explicit_hosts, scriptable_hosts);
  EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
}

TEST(ExtensionPermissionsTest, GetDistinctHostsForDisplay_NetIs2ndBestRcd) {
  scoped_refptr<ExtensionPermissionSet> perm_set;
  ExtensionAPIPermissionSet empty_perms;
  URLPatternSet explicit_hosts;
  URLPatternSet scriptable_hosts;
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.ca/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.org/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.co.uk/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.net/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.jp/path"));
  // No http://www.foo.com/path

  std::set<std::string> expected;
  expected.insert("www.foo.net");
  perm_set = new ExtensionPermissionSet(
      empty_perms, explicit_hosts, scriptable_hosts);
  EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
}

TEST(ExtensionPermissionsTest,
     GetDistinctHostsForDisplay_OrgIs3rdBestRcd) {
  scoped_refptr<ExtensionPermissionSet> perm_set;
  ExtensionAPIPermissionSet empty_perms;
  URLPatternSet explicit_hosts;
  URLPatternSet scriptable_hosts;
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.ca/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.org/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.co.uk/path"));
  // No http://www.foo.net/path
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.jp/path"));
  // No http://www.foo.com/path

  std::set<std::string> expected;
  expected.insert("www.foo.org");
  perm_set = new ExtensionPermissionSet(
      empty_perms, explicit_hosts, scriptable_hosts);
  EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
}

TEST(ExtensionPermissionsTest,
     GetDistinctHostsForDisplay_FirstInListIs4thBestRcd) {
  scoped_refptr<ExtensionPermissionSet> perm_set;
  ExtensionAPIPermissionSet empty_perms;
  URLPatternSet explicit_hosts;
  URLPatternSet scriptable_hosts;
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.ca/path"));
  // No http://www.foo.org/path
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.co.uk/path"));
  // No http://www.foo.net/path
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.jp/path"));
  // No http://www.foo.com/path

  std::set<std::string> expected;
  expected.insert("www.foo.ca");
  perm_set = new ExtensionPermissionSet(
      empty_perms, explicit_hosts, scriptable_hosts);
  EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
}

TEST(ExtensionPermissionsTest, HasLessHostPrivilegesThan) {
  URLPatternSet elist1;
  URLPatternSet elist2;
  URLPatternSet slist1;
  URLPatternSet slist2;
  scoped_refptr<ExtensionPermissionSet> set1;
  scoped_refptr<ExtensionPermissionSet> set2;
  ExtensionAPIPermissionSet empty_perms;
  elist1.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"));
  elist1.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com/path"));

  // Test that the host order does not matter.
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com/path"));
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"));

  set1 = new ExtensionPermissionSet(empty_perms, elist1, slist1);
  set2 = new ExtensionPermissionSet(empty_perms, elist2, slist2);

  EXPECT_FALSE(set1->HasLessHostPrivilegesThan(set2.get()));
  EXPECT_FALSE(set2->HasLessHostPrivilegesThan(set1.get()));

  // Test that paths are ignored.
  elist2.ClearPatterns();
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com/*"));
  set2 = new ExtensionPermissionSet(empty_perms, elist2, slist2);
  EXPECT_FALSE(set1->HasLessHostPrivilegesThan(set2.get()));
  EXPECT_FALSE(set2->HasLessHostPrivilegesThan(set1.get()));

  // Test that RCDs are ignored.
  elist2.ClearPatterns();
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com.hk/*"));
  set2 = new ExtensionPermissionSet(empty_perms, elist2, slist2);
  EXPECT_FALSE(set1->HasLessHostPrivilegesThan(set2.get()));
  EXPECT_FALSE(set2->HasLessHostPrivilegesThan(set1.get()));

  // Test that subdomain wildcards are handled properly.
  elist2.ClearPatterns();
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://*.google.com.hk/*"));
  set2 = new ExtensionPermissionSet(empty_perms, elist2, slist2);
  EXPECT_TRUE(set1->HasLessHostPrivilegesThan(set2.get()));
  //TODO(jstritar): Does not match subdomains properly. http://crbug.com/65337
  //EXPECT_FALSE(set2->HasLessHostPrivilegesThan(set1.get()));

  // Test that different domains count as different hosts.
  elist2.ClearPatterns();
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com/path"));
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.example.org/path"));
  set2 = new ExtensionPermissionSet(empty_perms, elist2, slist2);
  EXPECT_TRUE(set1->HasLessHostPrivilegesThan(set2.get()));
  EXPECT_FALSE(set2->HasLessHostPrivilegesThan(set1.get()));

  // Test that different subdomains count as different hosts.
  elist2.ClearPatterns();
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://mail.google.com/*"));
  set2 = new ExtensionPermissionSet(empty_perms, elist2, slist2);
  EXPECT_TRUE(set1->HasLessHostPrivilegesThan(set2.get()));
  EXPECT_TRUE(set2->HasLessHostPrivilegesThan(set1.get()));
}

TEST(ExtensionPermissionsTest, GetAPIsAsStrings) {
  ExtensionAPIPermissionSet apis;
  URLPatternSet empty_set;

  apis.insert(ExtensionAPIPermission::kProxy);
  apis.insert(ExtensionAPIPermission::kBackground);
  apis.insert(ExtensionAPIPermission::kNotification);
  apis.insert(ExtensionAPIPermission::kTab);

  scoped_refptr<ExtensionPermissionSet> perm_set = new ExtensionPermissionSet(
      apis, empty_set, empty_set);
  std::set<std::string> api_names = perm_set->GetAPIsAsStrings();

  // The result is correct if it has the same number of elements
  // and we can convert it back to the id set.
  EXPECT_EQ(4u, api_names.size());
  EXPECT_EQ(apis,
            ExtensionPermissionsInfo::GetInstance()->GetAllByName(api_names));
}

TEST(ExtensionPermissionsTest, IsEmpty) {
  ExtensionAPIPermissionSet empty_apis;
  URLPatternSet empty_extent;

  scoped_refptr<ExtensionPermissionSet> empty = new ExtensionPermissionSet();
  EXPECT_TRUE(empty->IsEmpty());
  scoped_refptr<ExtensionPermissionSet> perm_set;

  perm_set = new ExtensionPermissionSet(empty_apis, empty_extent, empty_extent);
  EXPECT_TRUE(perm_set->IsEmpty());

  ExtensionAPIPermissionSet non_empty_apis;
  non_empty_apis.insert(ExtensionAPIPermission::kBackground);
  perm_set = new ExtensionPermissionSet(
      non_empty_apis, empty_extent, empty_extent);
  EXPECT_FALSE(perm_set->IsEmpty());

  // Try non standard host
  URLPatternSet non_empty_extent;
  AddPattern(&non_empty_extent, "http://www.google.com/*");

  perm_set = new ExtensionPermissionSet(
      empty_apis, non_empty_extent, empty_extent);
  EXPECT_FALSE(perm_set->IsEmpty());

  perm_set = new ExtensionPermissionSet(
      empty_apis, empty_extent, non_empty_extent);
  EXPECT_FALSE(perm_set->IsEmpty());
}
