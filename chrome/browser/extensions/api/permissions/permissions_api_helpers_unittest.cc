// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"
#include "chrome/common/extensions/api/permissions.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "extensions/common/url_pattern_set.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::permissions_api_helpers::PackPermissionSet;
using extensions::permissions_api_helpers::UnpackPermissionSet;
using extensions::api::permissions::Permissions;
using extensions::APIPermission;
using extensions::APIPermissionSet;
using extensions::PermissionSet;
using extensions::URLPatternSet;

namespace {

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

}  // namespace

// Tests that we can convert PermissionSets to and from values.
TEST(ExtensionPermissionsAPIHelpers, Pack) {
  APIPermissionSet apis;
  apis.insert(APIPermission::kTab);
  apis.insert(APIPermission::kWebRequest);
  // Note: kWebRequest implies also kWebRequestInternal.
  URLPatternSet hosts;
  AddPattern(&hosts, "http://a.com/*");
  AddPattern(&hosts, "http://b.com/*");

  scoped_refptr<PermissionSet> permission_set =
      new PermissionSet(apis, hosts, URLPatternSet());

  // Pack the permission set to value and verify its contents.
  scoped_ptr<Permissions> permissions(PackPermissionSet(permission_set));
  scoped_ptr<DictionaryValue> value(permissions->ToValue());
  ListValue* api_list = NULL;
  ListValue* origin_list = NULL;
  EXPECT_TRUE(value->GetList("permissions", &api_list));
  EXPECT_TRUE(value->GetList("origins", &origin_list));

  EXPECT_EQ(3u, api_list->GetSize());
  EXPECT_EQ(2u, origin_list->GetSize());

  std::string expected_apis[] = { "tabs", "webRequest" };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(expected_apis); ++i) {
    scoped_ptr<Value> value(Value::CreateStringValue(expected_apis[i]));
    EXPECT_NE(api_list->end(), api_list->Find(*value));
  }

  std::string expected_origins[] = { "http://a.com/*", "http://b.com/*" };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(expected_origins); ++i) {
    scoped_ptr<Value> value(Value::CreateStringValue(expected_origins[i]));
    EXPECT_NE(origin_list->end(), origin_list->Find(*value));
  }

  // Unpack the value back to a permission set and make sure its equal to the
  // original one.
  scoped_refptr<PermissionSet> from_value;
  std::string error;
  Permissions permissions_object;
  EXPECT_TRUE(Permissions::Populate(*value, &permissions_object));
  from_value = UnpackPermissionSet(permissions_object, true, &error);
  EXPECT_TRUE(error.empty());

  EXPECT_EQ(*permission_set, *from_value);
}

// Tests various error conditions and edge cases when unpacking values
// into PermissionSets.
TEST(ExtensionPermissionsAPIHelpers, Unpack) {
  scoped_ptr<ListValue> apis(new ListValue());
  apis->Append(Value::CreateStringValue("tabs"));
  scoped_ptr<ListValue> origins(new ListValue());
  origins->Append(Value::CreateStringValue("http://a.com/*"));

  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  scoped_refptr<PermissionSet> permissions;
  std::string error;

  // Origins shouldn't have to be present.
  {
    Permissions permissions_object;
    value->Set("permissions", apis->DeepCopy());
    EXPECT_TRUE(Permissions::Populate(*value, &permissions_object));
    permissions = UnpackPermissionSet(permissions_object, true, &error);
    EXPECT_TRUE(permissions->HasAPIPermission(APIPermission::kTab));
    EXPECT_TRUE(permissions);
    EXPECT_TRUE(error.empty());
  }

  // The api permissions don't need to be present either.
  {
    Permissions permissions_object;
    value->Clear();
    value->Set("origins", origins->DeepCopy());
    EXPECT_TRUE(Permissions::Populate(*value, &permissions_object));
    permissions = UnpackPermissionSet(permissions_object, true, &error);
    EXPECT_TRUE(permissions);
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(permissions->HasExplicitAccessToOrigin(GURL("http://a.com/")));
  }

  // Throw errors for non-string API permissions.
  {
    Permissions permissions_object;
    value->Clear();
    scoped_ptr<ListValue> invalid_apis(apis->DeepCopy());
    invalid_apis->Append(Value::CreateIntegerValue(3));
    value->Set("permissions", invalid_apis->DeepCopy());
    EXPECT_FALSE(Permissions::Populate(*value, &permissions_object));
  }

  // Throw errors for non-string origins.
  {
    Permissions permissions_object;
    value->Clear();
    scoped_ptr<ListValue> invalid_origins(origins->DeepCopy());
    invalid_origins->Append(Value::CreateIntegerValue(3));
    value->Set("origins", invalid_origins->DeepCopy());
    EXPECT_FALSE(Permissions::Populate(*value, &permissions_object));
  }

  // Throw errors when "origins" or "permissions" are not list values.
  {
    Permissions permissions_object;
    value->Clear();
    value->Set("origins", Value::CreateIntegerValue(2));
    EXPECT_FALSE(Permissions::Populate(*value, &permissions_object));
  }

  {
    Permissions permissions_object;
    value->Clear();
    value->Set("permissions", Value::CreateIntegerValue(2));
    EXPECT_FALSE(Permissions::Populate(*value, &permissions_object));
  }

  // Additional fields should be allowed.
  {
    Permissions permissions_object;
    value->Clear();
    value->Set("origins", origins->DeepCopy());
    value->Set("random", Value::CreateIntegerValue(3));
    EXPECT_TRUE(Permissions::Populate(*value, &permissions_object));
    permissions = UnpackPermissionSet(permissions_object, true, &error);
    EXPECT_TRUE(permissions);
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(permissions->HasExplicitAccessToOrigin(GURL("http://a.com/")));
  }

  // Unknown permissions should throw an error.
  {
    Permissions permissions_object;
    value->Clear();
    scoped_ptr<ListValue> invalid_apis(apis->DeepCopy());
    invalid_apis->Append(Value::CreateStringValue("unknown_permission"));
    value->Set("permissions", invalid_apis->DeepCopy());
    EXPECT_TRUE(Permissions::Populate(*value, &permissions_object));
    permissions = UnpackPermissionSet(permissions_object, true, &error);
    EXPECT_FALSE(permissions);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(error, "'unknown_permission' is not a recognized permission.");
  }
}
