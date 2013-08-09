// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "chrome/common/extensions/permissions/socket_permission.h"
#include "extensions/common/error_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using extension_test_util::LoadManifest;

namespace extensions {

namespace {

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

size_t IndexOf(const std::vector<string16>& warnings,
               const std::string& warning) {
  for (size_t i = 0; i < warnings.size(); ++i) {
    if (warnings[i] == ASCIIToUTF16(warning))
      return i;
  }

  return warnings.size();
}

bool Contains(const std::vector<string16>& warnings,
              const std::string& warning) {
  return IndexOf(warnings, warning) != warnings.size();
}

}  // namespace

// Tests GetByID.
TEST(PermissionsTest, GetByID) {
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  APIPermissionSet apis = info->GetAll();
  for (APIPermissionSet::const_iterator i = apis.begin();
       i != apis.end(); ++i) {
    EXPECT_EQ(i->id(), i->info()->id());
  }
}

// Tests that GetByName works with normal permission names and aliases.
TEST(PermissionsTest, GetByName) {
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  EXPECT_EQ(APIPermission::kTab, info->GetByName("tabs")->id());
  EXPECT_EQ(APIPermission::kManagement,
            info->GetByName("management")->id());
  EXPECT_FALSE(info->GetByName("alsdkfjasldkfj"));
}

TEST(PermissionsTest, GetAll) {
  size_t count = 0;
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  APIPermissionSet apis = info->GetAll();
  for (APIPermissionSet::const_iterator api = apis.begin();
       api != apis.end(); ++api) {
    // Make sure only the valid permission IDs get returned.
    EXPECT_NE(APIPermission::kInvalid, api->id());
    EXPECT_NE(APIPermission::kUnknown, api->id());
    count++;
  }
  EXPECT_EQ(count, info->get_permission_count());
}

TEST(PermissionsTest, GetAllByName) {
  std::set<std::string> names;
  names.insert("background");
  names.insert("management");

  // This is an alias of kTab
  names.insert("windows");

  // This unknown name should get dropped.
  names.insert("sdlkfjasdlkfj");

  APIPermissionSet expected;
  expected.insert(APIPermission::kBackground);
  expected.insert(APIPermission::kManagement);
  expected.insert(APIPermission::kTab);

  EXPECT_EQ(expected,
            PermissionsInfo::GetInstance()->GetAllByName(names));
}

// Tests that the aliases are properly mapped.
TEST(PermissionsTest, Aliases) {
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  // tabs: tabs, windows
  std::string tabs_name = "tabs";
  EXPECT_EQ(tabs_name, info->GetByID(APIPermission::kTab)->name());
  EXPECT_EQ(APIPermission::kTab, info->GetByName("tabs")->id());
  EXPECT_EQ(APIPermission::kTab, info->GetByName("windows")->id());

  // unlimitedStorage: unlimitedStorage, unlimited_storage
  std::string storage_name = "unlimitedStorage";
  EXPECT_EQ(storage_name, info->GetByID(
      APIPermission::kUnlimitedStorage)->name());
  EXPECT_EQ(APIPermission::kUnlimitedStorage,
            info->GetByName("unlimitedStorage")->id());
  EXPECT_EQ(APIPermission::kUnlimitedStorage,
            info->GetByName("unlimited_storage")->id());
}

TEST(PermissionsTest, EffectiveHostPermissions) {
  scoped_refptr<Extension> extension;
  scoped_refptr<const PermissionSet> permissions;

  extension = LoadManifest("effective_host_permissions", "empty.json");
  permissions = extension->GetActivePermissions();
  EXPECT_EQ(0u,
            PermissionsData::GetEffectiveHostPermissions(extension.get())
                .patterns().size());
  EXPECT_FALSE(
      permissions->HasEffectiveAccessToURL(GURL("http://www.google.com")));
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

TEST(PermissionsTest, ExplicitAccessToOrigin) {
  APIPermissionSet apis;
  URLPatternSet explicit_hosts;
  URLPatternSet scriptable_hosts;

  AddPattern(&explicit_hosts, "http://*.google.com/*");
  // The explicit host paths should get set to /*.
  AddPattern(&explicit_hosts, "http://www.example.com/a/particular/path/*");

  scoped_refptr<PermissionSet> perm_set = new PermissionSet(
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

TEST(PermissionsTest, CreateUnion) {
  APIPermission* permission = NULL;

  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;

  URLPatternSet explicit_hosts1;
  URLPatternSet explicit_hosts2;
  URLPatternSet expected_explicit_hosts;

  URLPatternSet scriptable_hosts1;
  URLPatternSet scriptable_hosts2;
  URLPatternSet expected_scriptable_hosts;

  URLPatternSet effective_hosts;

  scoped_refptr<PermissionSet> set1;
  scoped_refptr<PermissionSet> set2;
  scoped_refptr<PermissionSet> union_set;

  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(
        base::Value::CreateStringValue("tcp-connect:*.example.com:80"));
    value->Append(base::Value::CreateStringValue("udp-bind::8080"));
    value->Append(base::Value::CreateStringValue("udp-send-to::8888"));
    if (!permission->FromValue(value.get())) {
      NOTREACHED();
    }
  }

  // Union with an empty set.
  apis1.insert(APIPermission::kTab);
  apis1.insert(APIPermission::kBackground);
  apis1.insert(permission->Clone());
  expected_apis.insert(APIPermission::kTab);
  expected_apis.insert(APIPermission::kBackground);
  expected_apis.insert(permission);

  AddPattern(&explicit_hosts1, "http://*.google.com/*");
  AddPattern(&expected_explicit_hosts, "http://*.google.com/*");
  AddPattern(&effective_hosts, "http://*.google.com/*");

  set1 = new PermissionSet(apis1, explicit_hosts1, scriptable_hosts1);
  set2 = new PermissionSet(apis2, explicit_hosts2, scriptable_hosts2);
  union_set = PermissionSet::CreateUnion(set1.get(), set2.get());
  EXPECT_TRUE(set1->Contains(*set2.get()));
  EXPECT_TRUE(set1->Contains(*union_set.get()));
  EXPECT_FALSE(set2->Contains(*set1.get()));
  EXPECT_FALSE(set2->Contains(*union_set.get()));
  EXPECT_TRUE(union_set->Contains(*set1.get()));
  EXPECT_TRUE(union_set->Contains(*set2.get()));

  EXPECT_FALSE(union_set->HasEffectiveFullAccess());
  EXPECT_EQ(expected_apis, union_set->apis());
  EXPECT_EQ(expected_explicit_hosts, union_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, union_set->scriptable_hosts());
  EXPECT_EQ(expected_explicit_hosts, union_set->effective_hosts());

  // Now use a real second set.
  apis2.insert(APIPermission::kTab);
  apis2.insert(APIPermission::kProxy);
  apis2.insert(APIPermission::kClipboardWrite);
  apis2.insert(APIPermission::kPlugin);

  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(
        base::Value::CreateStringValue("tcp-connect:*.example.com:80"));
    value->Append(base::Value::CreateStringValue("udp-send-to::8899"));
    if (!permission->FromValue(value.get())) {
      NOTREACHED();
    }
  }
  apis2.insert(permission);

  expected_apis.insert(APIPermission::kTab);
  expected_apis.insert(APIPermission::kProxy);
  expected_apis.insert(APIPermission::kClipboardWrite);
  expected_apis.insert(APIPermission::kPlugin);

  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(
        base::Value::CreateStringValue("tcp-connect:*.example.com:80"));
    value->Append(base::Value::CreateStringValue("udp-bind::8080"));
    value->Append(base::Value::CreateStringValue("udp-send-to::8888"));
    value->Append(base::Value::CreateStringValue("udp-send-to::8899"));
    if (!permission->FromValue(value.get())) {
      NOTREACHED();
    }
  }
  // Insert a new permission socket permisssion which will replace the old one.
  expected_apis.insert(permission);

  AddPattern(&explicit_hosts2, "http://*.example.com/*");
  AddPattern(&scriptable_hosts2, "http://*.google.com/*");
  AddPattern(&expected_explicit_hosts, "http://*.example.com/*");
  AddPattern(&expected_scriptable_hosts, "http://*.google.com/*");

  URLPatternSet::CreateUnion(
      explicit_hosts2, scriptable_hosts2, &effective_hosts);

  set2 = new PermissionSet(apis2, explicit_hosts2, scriptable_hosts2);
  union_set = PermissionSet::CreateUnion(set1.get(), set2.get());

  EXPECT_FALSE(set1->Contains(*set2.get()));
  EXPECT_FALSE(set1->Contains(*union_set.get()));
  EXPECT_FALSE(set2->Contains(*set1.get()));
  EXPECT_FALSE(set2->Contains(*union_set.get()));
  EXPECT_TRUE(union_set->Contains(*set1.get()));
  EXPECT_TRUE(union_set->Contains(*set2.get()));

  EXPECT_TRUE(union_set->HasEffectiveFullAccess());
  EXPECT_TRUE(union_set->HasEffectiveAccessToAllHosts());
  EXPECT_EQ(expected_apis, union_set->apis());
  EXPECT_EQ(expected_explicit_hosts, union_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, union_set->scriptable_hosts());
  EXPECT_EQ(effective_hosts, union_set->effective_hosts());
}

TEST(PermissionsTest, CreateIntersection) {
  APIPermission* permission = NULL;

  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;

  URLPatternSet explicit_hosts1;
  URLPatternSet explicit_hosts2;
  URLPatternSet expected_explicit_hosts;

  URLPatternSet scriptable_hosts1;
  URLPatternSet scriptable_hosts2;
  URLPatternSet expected_scriptable_hosts;

  URLPatternSet effective_hosts;

  scoped_refptr<PermissionSet> set1;
  scoped_refptr<PermissionSet> set2;
  scoped_refptr<PermissionSet> new_set;

  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

  // Intersection with an empty set.
  apis1.insert(APIPermission::kTab);
  apis1.insert(APIPermission::kBackground);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(
        base::Value::CreateStringValue("tcp-connect:*.example.com:80"));
    value->Append(base::Value::CreateStringValue("udp-bind::8080"));
    value->Append(base::Value::CreateStringValue("udp-send-to::8888"));
    if (!permission->FromValue(value.get())) {
      NOTREACHED();
    }
  }
  apis1.insert(permission);

  AddPattern(&explicit_hosts1, "http://*.google.com/*");
  AddPattern(&scriptable_hosts1, "http://www.reddit.com/*");

  set1 = new PermissionSet(apis1, explicit_hosts1, scriptable_hosts1);
  set2 = new PermissionSet(apis2, explicit_hosts2, scriptable_hosts2);
  new_set = PermissionSet::CreateIntersection(set1.get(), set2.get());
  EXPECT_TRUE(set1->Contains(*new_set.get()));
  EXPECT_TRUE(set2->Contains(*new_set.get()));
  EXPECT_TRUE(set1->Contains(*set2.get()));
  EXPECT_FALSE(set2->Contains(*set1.get()));
  EXPECT_FALSE(new_set->Contains(*set1.get()));
  EXPECT_TRUE(new_set->Contains(*set2.get()));

  EXPECT_TRUE(new_set->IsEmpty());
  EXPECT_FALSE(new_set->HasEffectiveFullAccess());
  EXPECT_EQ(expected_apis, new_set->apis());
  EXPECT_EQ(expected_explicit_hosts, new_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, new_set->scriptable_hosts());
  EXPECT_EQ(expected_explicit_hosts, new_set->effective_hosts());

  // Now use a real second set.
  apis2.insert(APIPermission::kTab);
  apis2.insert(APIPermission::kProxy);
  apis2.insert(APIPermission::kClipboardWrite);
  apis2.insert(APIPermission::kPlugin);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(base::Value::CreateStringValue("udp-bind::8080"));
    value->Append(base::Value::CreateStringValue("udp-send-to::8888"));
    value->Append(base::Value::CreateStringValue("udp-send-to::8899"));
    if (!permission->FromValue(value.get())) {
      NOTREACHED();
    }
  }
  apis2.insert(permission);

  expected_apis.insert(APIPermission::kTab);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(base::Value::CreateStringValue("udp-bind::8080"));
    value->Append(base::Value::CreateStringValue("udp-send-to::8888"));
    if (!permission->FromValue(value.get())) {
      NOTREACHED();
    }
  }
  expected_apis.insert(permission);

  AddPattern(&explicit_hosts2, "http://*.example.com/*");
  AddPattern(&explicit_hosts2, "http://*.google.com/*");
  AddPattern(&scriptable_hosts2, "http://*.google.com/*");
  AddPattern(&expected_explicit_hosts, "http://*.google.com/*");

  effective_hosts.ClearPatterns();
  AddPattern(&effective_hosts, "http://*.google.com/*");

  set2 = new PermissionSet(apis2, explicit_hosts2, scriptable_hosts2);
  new_set = PermissionSet::CreateIntersection(set1.get(), set2.get());

  EXPECT_TRUE(set1->Contains(*new_set.get()));
  EXPECT_TRUE(set2->Contains(*new_set.get()));
  EXPECT_FALSE(set1->Contains(*set2.get()));
  EXPECT_FALSE(set2->Contains(*set1.get()));
  EXPECT_FALSE(new_set->Contains(*set1.get()));
  EXPECT_FALSE(new_set->Contains(*set2.get()));

  EXPECT_FALSE(new_set->HasEffectiveFullAccess());
  EXPECT_FALSE(new_set->HasEffectiveAccessToAllHosts());
  EXPECT_EQ(expected_apis, new_set->apis());
  EXPECT_EQ(expected_explicit_hosts, new_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, new_set->scriptable_hosts());
  EXPECT_EQ(effective_hosts, new_set->effective_hosts());
}

TEST(PermissionsTest, CreateDifference) {
  APIPermission* permission = NULL;

  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;

  URLPatternSet explicit_hosts1;
  URLPatternSet explicit_hosts2;
  URLPatternSet expected_explicit_hosts;

  URLPatternSet scriptable_hosts1;
  URLPatternSet scriptable_hosts2;
  URLPatternSet expected_scriptable_hosts;

  URLPatternSet effective_hosts;

  scoped_refptr<PermissionSet> set1;
  scoped_refptr<PermissionSet> set2;
  scoped_refptr<PermissionSet> new_set;

  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

  // Difference with an empty set.
  apis1.insert(APIPermission::kTab);
  apis1.insert(APIPermission::kBackground);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(
       base::Value::CreateStringValue("tcp-connect:*.example.com:80"));
    value->Append(base::Value::CreateStringValue("udp-bind::8080"));
    value->Append(base::Value::CreateStringValue("udp-send-to::8888"));
    if (!permission->FromValue(value.get())) {
      NOTREACHED();
    }
  }
  apis1.insert(permission);

  AddPattern(&explicit_hosts1, "http://*.google.com/*");
  AddPattern(&scriptable_hosts1, "http://www.reddit.com/*");

  set1 = new PermissionSet(apis1, explicit_hosts1, scriptable_hosts1);
  set2 = new PermissionSet(apis2, explicit_hosts2, scriptable_hosts2);
  new_set = PermissionSet::CreateDifference(set1.get(), set2.get());
  EXPECT_EQ(*set1.get(), *new_set.get());

  // Now use a real second set.
  apis2.insert(APIPermission::kTab);
  apis2.insert(APIPermission::kProxy);
  apis2.insert(APIPermission::kClipboardWrite);
  apis2.insert(APIPermission::kPlugin);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(
        base::Value::CreateStringValue("tcp-connect:*.example.com:80"));
    value->Append(base::Value::CreateStringValue("udp-send-to::8899"));
    if (!permission->FromValue(value.get())) {
      NOTREACHED();
    }
  }
  apis2.insert(permission);

  expected_apis.insert(APIPermission::kBackground);
  permission = permission_info->CreateAPIPermission();
  {
    scoped_ptr<base::ListValue> value(new base::ListValue());
    value->Append(base::Value::CreateStringValue("udp-bind::8080"));
    value->Append(base::Value::CreateStringValue("udp-send-to::8888"));
    if (!permission->FromValue(value.get())) {
      NOTREACHED();
    }
  }
  expected_apis.insert(permission);

  AddPattern(&explicit_hosts2, "http://*.example.com/*");
  AddPattern(&explicit_hosts2, "http://*.google.com/*");
  AddPattern(&scriptable_hosts2, "http://*.google.com/*");
  AddPattern(&expected_scriptable_hosts, "http://www.reddit.com/*");

  effective_hosts.ClearPatterns();
  AddPattern(&effective_hosts, "http://www.reddit.com/*");

  set2 = new PermissionSet(apis2, explicit_hosts2, scriptable_hosts2);
  new_set = PermissionSet::CreateDifference(set1.get(), set2.get());

  EXPECT_TRUE(set1->Contains(*new_set.get()));
  EXPECT_FALSE(set2->Contains(*new_set.get()));

  EXPECT_FALSE(new_set->HasEffectiveFullAccess());
  EXPECT_FALSE(new_set->HasEffectiveAccessToAllHosts());
  EXPECT_EQ(expected_apis, new_set->apis());
  EXPECT_EQ(expected_explicit_hosts, new_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, new_set->scriptable_hosts());
  EXPECT_EQ(effective_hosts, new_set->effective_hosts());

  // |set3| = |set1| - |set2| --> |set3| intersect |set2| == empty_set
  set1 = PermissionSet::CreateIntersection(new_set.get(), set2.get());
  EXPECT_TRUE(set1->IsEmpty());
}

TEST(PermissionsTest, HasLessPrivilegesThan) {
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
    { "platformapp1", false },  // host permissions for platform apps
    { "platformapp2", true },  // API permissions for platform apps
    { "media_galleries1", true },  // read|all -> copyTo|all
    { "media_galleries2", true },  // read|all -> read|copyTo|all
    { "media_galleries3", true },  // all -> copyTo|all
    { "media_galleries4", false },  // read|all -> all
    { "media_galleries5", false },  // read|copyTo|all -> read|all
    { "media_galleries6", false },  // read|all -> read|all
  };

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

    scoped_refptr<const PermissionSet> old_p(
        old_extension->GetActivePermissions());
    scoped_refptr<const PermissionSet> new_p(
        new_extension->GetActivePermissions());
    Manifest::Type extension_type = old_extension->GetType();

    EXPECT_EQ(kTests[i].expect_increase,
              old_p->HasLessPrivilegesThan(new_p.get(), extension_type))
        << kTests[i].base_name;
  }
}

TEST(PermissionsTest, PermissionMessages) {
  // Ensure that all permissions that needs to show install UI actually have
  // strings associated with them.
  APIPermissionSet skip;

  // These are considered "nuisance" or "trivial" permissions that don't need
  // a prompt.
  skip.insert(APIPermission::kActiveTab);
  skip.insert(APIPermission::kAdView);
  skip.insert(APIPermission::kAlarms);
  skip.insert(APIPermission::kAppCurrentWindowInternal);
  skip.insert(APIPermission::kAppRuntime);
  skip.insert(APIPermission::kAppWindow);
  skip.insert(APIPermission::kAudio);
  skip.insert(APIPermission::kBrowsingData);
  skip.insert(APIPermission::kContextMenus);
  skip.insert(APIPermission::kDiagnostics);
  skip.insert(APIPermission::kDownloadsShelf);
  skip.insert(APIPermission::kFontSettings);
  skip.insert(APIPermission::kFullscreen);
  skip.insert(APIPermission::kIdle);
  skip.insert(APIPermission::kLogPrivate);
  skip.insert(APIPermission::kNotification);
  skip.insert(APIPermission::kPointerLock);
  skip.insert(APIPermission::kPower);
  skip.insert(APIPermission::kPushMessaging);
  skip.insert(APIPermission::kScreensaver);
  skip.insert(APIPermission::kSessionRestore);
  skip.insert(APIPermission::kStorage);
  skip.insert(APIPermission::kSystemCpu);
  skip.insert(APIPermission::kSystemDisplay);
  skip.insert(APIPermission::kSystemMemory);
  skip.insert(APIPermission::kSystemStorage);
  skip.insert(APIPermission::kTts);
  skip.insert(APIPermission::kUnlimitedStorage);
  skip.insert(APIPermission::kWebView);

  // TODO(erikkay) add a string for this permission.
  skip.insert(APIPermission::kBackground);

  skip.insert(APIPermission::kClipboardWrite);

  // The cookie permission does nothing unless you have associated host
  // permissions.
  skip.insert(APIPermission::kCookie);

  // These are warned as part of host permission checks.
  skip.insert(APIPermission::kDeclarativeContent);
  skip.insert(APIPermission::kDeclarativeWebRequest);
  skip.insert(APIPermission::kPageCapture);
  skip.insert(APIPermission::kProxy);
  skip.insert(APIPermission::kTabCapture);
  skip.insert(APIPermission::kWebRequest);
  skip.insert(APIPermission::kWebRequestBlocking);

  // This permission requires explicit user action (context menu handler)
  // so we won't prompt for it for now.
  skip.insert(APIPermission::kFileBrowserHandler);

  // These permissions require explicit user action (configuration dialog)
  // so we don't prompt for them at install time.
  skip.insert(APIPermission::kMediaGalleries);

  // If you've turned on the experimental command-line flag, we don't need
  // to warn you further.
  skip.insert(APIPermission::kExperimental);

  // The Identity API has its own server-driven permission prompts.
  skip.insert(APIPermission::kIdentity);

  // These are private.
  skip.insert(APIPermission::kAutoTestPrivate);
  skip.insert(APIPermission::kBookmarkManagerPrivate);
  skip.insert(APIPermission::kChromeosInfoPrivate);
  skip.insert(APIPermission::kCloudPrintPrivate);
  skip.insert(APIPermission::kCommandLinePrivate);
  skip.insert(APIPermission::kDeveloperPrivate);
  skip.insert(APIPermission::kDial);
  skip.insert(APIPermission::kDownloadsInternal);
  skip.insert(APIPermission::kEchoPrivate);
  skip.insert(APIPermission::kEnterprisePlatformKeysPrivate);
  skip.insert(APIPermission::kFeedbackPrivate);
  skip.insert(APIPermission::kFileBrowserHandlerInternal);
  skip.insert(APIPermission::kFileBrowserPrivate);
  skip.insert(APIPermission::kIdentityPrivate);
  skip.insert(APIPermission::kInfobars);
  skip.insert(APIPermission::kInputMethodPrivate);
  skip.insert(APIPermission::kMediaGalleriesPrivate);
  skip.insert(APIPermission::kMediaPlayerPrivate);
  skip.insert(APIPermission::kMetricsPrivate);
  skip.insert(APIPermission::kPreferencesPrivate);
  skip.insert(APIPermission::kRecoveryPrivate);
  skip.insert(APIPermission::kRtcPrivate);
  skip.insert(APIPermission::kStreamsPrivate);
  skip.insert(APIPermission::kSystemPrivate);
  skip.insert(APIPermission::kTerminalPrivate);
  skip.insert(APIPermission::kWallpaperPrivate);
  skip.insert(APIPermission::kWebRequestInternal);
  skip.insert(APIPermission::kWebstorePrivate);

  // Warned as part of host permissions.
  skip.insert(APIPermission::kDevtools);

  // Platform apps.
  skip.insert(APIPermission::kBluetooth);
  skip.insert(APIPermission::kFileSystem);
  skip.insert(APIPermission::kFileSystemRetainEntries);
  skip.insert(APIPermission::kSocket);
  skip.insert(APIPermission::kUsbDevice);

  PermissionsInfo* info = PermissionsInfo::GetInstance();
  APIPermissionSet permissions = info->GetAll();
  for (APIPermissionSet::const_iterator i = permissions.begin();
       i != permissions.end(); ++i) {
    const APIPermissionInfo* permission_info = i->info();
    EXPECT_TRUE(permission_info != NULL);

    // Always skip permissions that cannot be in the manifest.
    scoped_ptr<const APIPermission> permission(
        permission_info->CreateAPIPermission());
    if (permission->ManifestEntryForbidden())
      continue;

    if (skip.count(i->id())) {
      EXPECT_EQ(PermissionMessage::kNone, permission_info->message_id())
          << "unexpected message_id for " << permission_info->name();
    } else {
      EXPECT_NE(PermissionMessage::kNone, permission_info->message_id())
          << "missing message_id for " << permission_info->name();
    }
  }
}

TEST(PermissionsTest, GetWarningMessages_ManyHosts) {
  scoped_refptr<Extension> extension;

  extension = LoadManifest("permissions", "many-hosts.json");
  std::vector<string16> warnings =
      PermissionsData::GetPermissionMessageStrings(extension.get());
  ASSERT_EQ(1u, warnings.size());
  EXPECT_EQ("Access your data on encrypted.google.com and www.google.com",
            UTF16ToUTF8(warnings[0]));
}

TEST(PermissionsTest, GetWarningMessages_Plugins) {
  scoped_refptr<Extension> extension;
  scoped_refptr<PermissionSet> permissions;

  extension = LoadManifest("permissions", "plugins.json");
  std::vector<string16> warnings =
      PermissionsData::GetPermissionMessageStrings(extension.get());
// We don't parse the plugins key on Chrome OS, so it should not ask for any
  // permissions.
#if defined(OS_CHROMEOS)
  ASSERT_EQ(0u, warnings.size());
#else
  ASSERT_EQ(1u, warnings.size());
  EXPECT_EQ("Access all data on your computer and the websites you visit",
            UTF16ToUTF8(warnings[0]));
#endif
}

TEST(PermissionsTest, GetWarningMessages_AudioVideo) {
  // Both audio and video present.
  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "audio-video.json");
  PermissionSet* set =
      const_cast<PermissionSet*>(
          extension->GetActivePermissions().get());
  std::vector<string16> warnings =
      set->GetWarningMessages(extension->GetType());
  EXPECT_FALSE(Contains(warnings, "Use your microphone"));
  EXPECT_FALSE(Contains(warnings, "Use your camera"));
  EXPECT_TRUE(Contains(warnings, "Use your microphone and camera"));
  size_t combined_index = IndexOf(warnings, "Use your microphone and camera");
  size_t combined_size = warnings.size();

  // Just audio present.
  set->apis_.erase(APIPermission::kVideoCapture);
  warnings = set->GetWarningMessages(extension->GetType());
  EXPECT_EQ(combined_size, warnings.size());
  EXPECT_EQ(combined_index, IndexOf(warnings, "Use your microphone"));
  EXPECT_FALSE(Contains(warnings, "Use your camera"));
  EXPECT_FALSE(Contains(warnings, "Use your microphone and camera"));

  // Just video present.
  set->apis_.erase(APIPermission::kAudioCapture);
  set->apis_.insert(APIPermission::kVideoCapture);
  warnings = set->GetWarningMessages(extension->GetType());
  EXPECT_EQ(combined_size, warnings.size());
  EXPECT_FALSE(Contains(warnings, "Use your microphone"));
  EXPECT_FALSE(Contains(warnings, "Use your microphone and camera"));
  EXPECT_TRUE(Contains(warnings, "Use your camera"));
}

TEST(PermissionsTest, GetWarningMessages_Serial) {
  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "serial.json");

  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_TRUE(extension->HasAPIPermission(APIPermission::kSerial));
  std::vector<string16> warnings =
      PermissionsData::GetPermissionMessageStrings(extension.get());
  EXPECT_TRUE(
      Contains(warnings, "Use serial devices attached to your computer"));
  ASSERT_EQ(1u, warnings.size());
}

TEST(PermissionsTest, GetWarningMessages_Socket_AnyHost) {
  ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_DEV);

  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "socket_any_host.json");
  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_TRUE(extension->HasAPIPermission(APIPermission::kSocket));
  std::vector<string16> warnings =
      PermissionsData::GetPermissionMessageStrings(extension.get());
  EXPECT_EQ(1u, warnings.size());
  EXPECT_TRUE(Contains(warnings, "Exchange data with any computer "
                                 "on the local network or internet"));
}

TEST(PermissionsTest, GetWarningMessages_Socket_OneDomainTwoHostnames) {
  ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_DEV);

  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "socket_one_domain_two_hostnames.json");
  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_TRUE(extension->HasAPIPermission(APIPermission::kSocket));
  std::vector<string16> warnings =
      PermissionsData::GetPermissionMessageStrings(extension.get());

  // Verify the warnings, including support for unicode characters, the fact
  // that domain host warnings come before specific host warnings, and the fact
  // that domains and hostnames are in alphabetical order regardless of the
  // order in the manifest file.
  EXPECT_EQ(2u, warnings.size());
  if (warnings.size() > 0)
    EXPECT_EQ(warnings[0],
              UTF8ToUTF16("Exchange data with any computer in the domain "
                          "example.org"));
  if (warnings.size() > 1)
    EXPECT_EQ(warnings[1],
              UTF8ToUTF16("Exchange data with the computers named: "
                          "b\xC3\xA5r.example.com foo.example.com"));
                          // "\xC3\xA5" = UTF-8 for lowercase A with ring above
}

TEST(PermissionsTest, GetWarningMessages_Socket_TwoDomainsOneHostname) {
  ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_DEV);

  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "socket_two_domains_one_hostname.json");
  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_TRUE(extension->HasAPIPermission(APIPermission::kSocket));
  std::vector<string16> warnings =
      PermissionsData::GetPermissionMessageStrings(extension.get());

  // Verify the warnings, including the fact that domain host warnings come
  // before specific host warnings and the fact that domains and hostnames are
  // in alphabetical order regardless of the order in the manifest file.
  EXPECT_EQ(2u, warnings.size());
  if (warnings.size() > 0)
    EXPECT_EQ(warnings[0],
              UTF8ToUTF16("Exchange data with any computer in the domains: "
                           "example.com foo.example.org"));
  if (warnings.size() > 1)
    EXPECT_EQ(warnings[1],
              UTF8ToUTF16("Exchange data with the computer named "
                           "bar.example.org"));
}

TEST(PermissionsTest, GetWarningMessages_PlatformApppHosts) {
  scoped_refptr<Extension> extension;

  extension = LoadManifest("permissions", "platform_app_hosts.json");
  EXPECT_TRUE(extension->is_platform_app());
  std::vector<string16> warnings =
      PermissionsData::GetPermissionMessageStrings(extension.get());
  ASSERT_EQ(0u, warnings.size());

  extension = LoadManifest("permissions", "platform_app_all_urls.json");
  EXPECT_TRUE(extension->is_platform_app());
  warnings = PermissionsData::GetPermissionMessageStrings(extension.get());
  ASSERT_EQ(0u, warnings.size());
}

TEST(PermissionsTest, GetDistinctHostsForDisplay) {
  scoped_refptr<PermissionSet> perm_set;
  APIPermissionSet empty_perms;
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
    perm_set = new PermissionSet(
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
    perm_set = new PermissionSet(
        empty_perms, explicit_hosts, scriptable_hosts);
    EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
  }

  {
    SCOPED_TRACE("schemes differ");

    // Add a pattern that differs only by scheme. This should be filtered out.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTPS, "https://www.bar.com/path"));
    perm_set = new PermissionSet(
        empty_perms, explicit_hosts, scriptable_hosts);
    EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
  }

  {
    SCOPED_TRACE("paths differ");

    // Add some dupes by path.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.bar.com/pathypath"));
    perm_set = new PermissionSet(
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

    perm_set = new PermissionSet(
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

    perm_set = new PermissionSet(
        empty_perms, explicit_hosts, scriptable_hosts);
    EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
  }

  {
    SCOPED_TRACE("wildcards");

    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://*.google.com/*"));

    expected.insert("*.google.com");

    perm_set = new PermissionSet(
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

    perm_set = new PermissionSet(
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

    perm_set = new PermissionSet(
        empty_perms, explicit_hosts, scriptable_hosts);
    EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
  }
}

TEST(PermissionsTest, GetDistinctHostsForDisplay_ComIsBestRcd) {
  scoped_refptr<PermissionSet> perm_set;
  APIPermissionSet empty_perms;
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
  perm_set = new PermissionSet(
      empty_perms, explicit_hosts, scriptable_hosts);
  EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
}

TEST(PermissionsTest, GetDistinctHostsForDisplay_NetIs2ndBestRcd) {
  scoped_refptr<PermissionSet> perm_set;
  APIPermissionSet empty_perms;
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
  perm_set = new PermissionSet(
      empty_perms, explicit_hosts, scriptable_hosts);
  EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
}

TEST(PermissionsTest,
     GetDistinctHostsForDisplay_OrgIs3rdBestRcd) {
  scoped_refptr<PermissionSet> perm_set;
  APIPermissionSet empty_perms;
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
  perm_set = new PermissionSet(
      empty_perms, explicit_hosts, scriptable_hosts);
  EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
}

TEST(PermissionsTest,
     GetDistinctHostsForDisplay_FirstInListIs4thBestRcd) {
  scoped_refptr<PermissionSet> perm_set;
  APIPermissionSet empty_perms;
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
  perm_set = new PermissionSet(
      empty_perms, explicit_hosts, scriptable_hosts);
  EXPECT_EQ(expected, perm_set->GetDistinctHostsForDisplay());
}

TEST(PermissionsTest, HasLessHostPrivilegesThan) {
  Manifest::Type extension_type = Manifest::TYPE_EXTENSION;
  URLPatternSet elist1;
  URLPatternSet elist2;
  URLPatternSet slist1;
  URLPatternSet slist2;
  scoped_refptr<PermissionSet> set1;
  scoped_refptr<PermissionSet> set2;
  APIPermissionSet empty_perms;
  elist1.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"));
  elist1.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com/path"));

  // Test that the host order does not matter.
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com/path"));
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"));

  set1 = new PermissionSet(empty_perms, elist1, slist1);
  set2 = new PermissionSet(empty_perms, elist2, slist2);

  EXPECT_FALSE(set1->HasLessHostPrivilegesThan(set2.get(), extension_type));
  EXPECT_FALSE(set2->HasLessHostPrivilegesThan(set1.get(), extension_type));

  // Test that paths are ignored.
  elist2.ClearPatterns();
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com/*"));
  set2 = new PermissionSet(empty_perms, elist2, slist2);
  EXPECT_FALSE(set1->HasLessHostPrivilegesThan(set2.get(), extension_type));
  EXPECT_FALSE(set2->HasLessHostPrivilegesThan(set1.get(), extension_type));

  // Test that RCDs are ignored.
  elist2.ClearPatterns();
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com.hk/*"));
  set2 = new PermissionSet(empty_perms, elist2, slist2);
  EXPECT_FALSE(set1->HasLessHostPrivilegesThan(set2.get(), extension_type));
  EXPECT_FALSE(set2->HasLessHostPrivilegesThan(set1.get(), extension_type));

  // Test that subdomain wildcards are handled properly.
  elist2.ClearPatterns();
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://*.google.com.hk/*"));
  set2 = new PermissionSet(empty_perms, elist2, slist2);
  EXPECT_TRUE(set1->HasLessHostPrivilegesThan(set2.get(), extension_type));
  // TODO(jstritar): Does not match subdomains properly. http://crbug.com/65337
  // EXPECT_FALSE(set2->HasLessHostPrivilegesThan(set1.get()));

  // Test that different domains count as different hosts.
  elist2.ClearPatterns();
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com/path"));
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.example.org/path"));
  set2 = new PermissionSet(empty_perms, elist2, slist2);
  EXPECT_TRUE(set1->HasLessHostPrivilegesThan(set2.get(), extension_type));
  EXPECT_FALSE(set2->HasLessHostPrivilegesThan(set1.get(), extension_type));

  // Test that different subdomains count as different hosts.
  elist2.ClearPatterns();
  elist2.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://mail.google.com/*"));
  set2 = new PermissionSet(empty_perms, elist2, slist2);
  EXPECT_TRUE(set1->HasLessHostPrivilegesThan(set2.get(), extension_type));
  EXPECT_TRUE(set2->HasLessHostPrivilegesThan(set1.get(), extension_type));

  // Test that platform apps do not have host permissions increases.
  extension_type = Manifest::TYPE_PLATFORM_APP;
  EXPECT_FALSE(set1->HasLessHostPrivilegesThan(set2.get(), extension_type));
  EXPECT_FALSE(set2->HasLessHostPrivilegesThan(set1.get(), extension_type));
}

TEST(PermissionsTest, GetAPIsAsStrings) {
  APIPermissionSet apis;
  URLPatternSet empty_set;

  apis.insert(APIPermission::kProxy);
  apis.insert(APIPermission::kBackground);
  apis.insert(APIPermission::kNotification);
  apis.insert(APIPermission::kTab);

  scoped_refptr<PermissionSet> perm_set = new PermissionSet(
      apis, empty_set, empty_set);
  std::set<std::string> api_names = perm_set->GetAPIsAsStrings();

  // The result is correct if it has the same number of elements
  // and we can convert it back to the id set.
  EXPECT_EQ(4u, api_names.size());
  EXPECT_EQ(apis,
            PermissionsInfo::GetInstance()->GetAllByName(api_names));
}

TEST(PermissionsTest, IsEmpty) {
  APIPermissionSet empty_apis;
  URLPatternSet empty_extent;

  scoped_refptr<PermissionSet> empty = new PermissionSet();
  EXPECT_TRUE(empty->IsEmpty());
  scoped_refptr<PermissionSet> perm_set;

  perm_set = new PermissionSet(empty_apis, empty_extent, empty_extent);
  EXPECT_TRUE(perm_set->IsEmpty());

  APIPermissionSet non_empty_apis;
  non_empty_apis.insert(APIPermission::kBackground);
  perm_set = new PermissionSet(
      non_empty_apis, empty_extent, empty_extent);
  EXPECT_FALSE(perm_set->IsEmpty());

  // Try non standard host
  URLPatternSet non_empty_extent;
  AddPattern(&non_empty_extent, "http://www.google.com/*");

  perm_set = new PermissionSet(
      empty_apis, non_empty_extent, empty_extent);
  EXPECT_FALSE(perm_set->IsEmpty());

  perm_set = new PermissionSet(
      empty_apis, empty_extent, non_empty_extent);
  EXPECT_FALSE(perm_set->IsEmpty());
}

TEST(PermissionsTest, ImpliedPermissions) {
  URLPatternSet empty_extent;
  APIPermissionSet apis;
  apis.insert(APIPermission::kWebRequest);
  apis.insert(APIPermission::kFileBrowserHandler);
  EXPECT_EQ(2U, apis.size());

  scoped_refptr<PermissionSet> perm_set;
  perm_set = new PermissionSet(apis, empty_extent, empty_extent);
  EXPECT_EQ(4U, perm_set->apis().size());
}

TEST(PermissionsTest, SyncFileSystemPermission) {
  scoped_refptr<Extension> extension = LoadManifest(
      "permissions", "sync_file_system.json");
  APIPermissionSet apis;
  apis.insert(APIPermission::kSyncFileSystem);
  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_TRUE(extension->HasAPIPermission(APIPermission::kSyncFileSystem));
  std::vector<string16> warnings =
      PermissionsData::GetPermissionMessageStrings(extension.get());
  EXPECT_TRUE(Contains(warnings, "Store data in your Google Drive account"));
  ASSERT_EQ(1u, warnings.size());
}

// Make sure that we don't crash when we're trying to show the permissions
// even though chrome://thumb (and everything that's not chrome://favicon with
// a chrome:// scheme) is not a valid permission.
// More details here: crbug/246314.
TEST(PermissionsTest, ChromeURLs) {
  URLPatternSet allowed_hosts;
  allowed_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "http://www.google.com/"));
  allowed_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "chrome://favicon/"));
  allowed_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "chrome://thumb/"));
  scoped_refptr<PermissionSet> permissions(
      new PermissionSet(APIPermissionSet(), allowed_hosts, URLPatternSet()));
  permissions->GetPermissionMessages(Manifest::TYPE_EXTENSION);
}

}  // namespace extensions
