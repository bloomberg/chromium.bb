// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_permissions_api_helpers.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "chrome/common/extensions/url_pattern_set.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::permissions_api::PackPermissionsToValue;
using extensions::permissions_api::UnpackPermissionsFromValue;

namespace {

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

}  // namespace

// Tests that we can convert ExtensionPermissionSets to and from values.
TEST(ExtensionPermissionsAPIHelpers, Pack) {
  ExtensionAPIPermissionSet apis;
  apis.insert(ExtensionAPIPermission::kTab);
  apis.insert(ExtensionAPIPermission::kWebRequest);
  URLPatternSet hosts;
  AddPattern(&hosts, "http://a.com/*");
  AddPattern(&hosts, "http://b.com/*");

  scoped_refptr<ExtensionPermissionSet> permissions =
      new ExtensionPermissionSet(apis, hosts, URLPatternSet());

  // Pack the permission set to value and verify its contents.
  scoped_ptr<DictionaryValue> value(PackPermissionsToValue(permissions));
  ListValue* api_list = NULL;
  ListValue* origin_list = NULL;
  ASSERT_TRUE(value->GetList("permissions", &api_list));
  ASSERT_TRUE(value->GetList("origins", &origin_list));

  ASSERT_EQ(2u, api_list->GetSize());
  ASSERT_EQ(2u, origin_list->GetSize());

  std::string expected_apis[] = { "tabs", "webRequest" };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(expected_apis); ++i) {
    scoped_ptr<Value> value(Value::CreateStringValue(expected_apis[i]));
    ASSERT_NE(api_list->end(), api_list->Find(*value));
  }

  std::string expected_origins[] = { "http://a.com/*", "http://b.com/*" };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(expected_origins); ++i) {
    scoped_ptr<Value> value(Value::CreateStringValue(expected_origins[i]));
    ASSERT_NE(origin_list->end(), origin_list->Find(*value));
  }

  // Unpack the value back to a permission set and make sure its equal to the
  // original one.
  scoped_refptr<ExtensionPermissionSet> from_value;
  bool bad_message = false;
  std::string error;
  ASSERT_TRUE(UnpackPermissionsFromValue(
      value.get(), &from_value, &bad_message, &error));
  ASSERT_FALSE(bad_message);
  ASSERT_TRUE(error.empty());

  ASSERT_EQ(*permissions, *from_value);
}

// Tests various error conditions and edge cases when unpacking values
// into ExtensionPermissionSets.
TEST(ExtensionPermissionsAPIHelpers, Unpack) {
  scoped_ptr<ListValue> apis(new ListValue());
  apis->Append(Value::CreateStringValue("tabs"));
  scoped_ptr<ListValue> origins(new ListValue());
  origins->Append(Value::CreateStringValue("http://a.com/*"));

  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  scoped_refptr<ExtensionPermissionSet> permissions;
  bool bad_message = false;
  std::string error;

  // Origins shouldn't have to be present.
  value->Set("permissions", apis->DeepCopy());
  ASSERT_TRUE(UnpackPermissionsFromValue(
      value.get(), &permissions, &bad_message, &error));
  ASSERT_TRUE(permissions->HasAPIPermission(ExtensionAPIPermission::kTab));
  ASSERT_FALSE(bad_message);
  ASSERT_TRUE(error.empty());

  // The api permissions don't need to be present either.
  value->Clear();
  value->Set("origins", origins->DeepCopy());
  ASSERT_TRUE(UnpackPermissionsFromValue(
      value.get(), &permissions, &bad_message, &error));
  ASSERT_FALSE(bad_message);
  ASSERT_TRUE(error.empty());
  ASSERT_TRUE(permissions->HasExplicitAccessToOrigin(GURL("http://a.com/")));

  // Throw errors for non-string API permissions.
  value->Clear();
  scoped_ptr<ListValue> invalid_apis(apis->DeepCopy());
  invalid_apis->Append(Value::CreateIntegerValue(3));
  value->Set("permissions", invalid_apis->DeepCopy());
  ASSERT_FALSE(UnpackPermissionsFromValue(
      value.get(), &permissions, &bad_message, &error));
  ASSERT_TRUE(bad_message);
  bad_message = false;

  // Throw errors for non-string origins.
  value->Clear();
  scoped_ptr<ListValue> invalid_origins(origins->DeepCopy());
  invalid_origins->Append(Value::CreateIntegerValue(3));
  value->Set("origins", invalid_origins->DeepCopy());
  ASSERT_FALSE(UnpackPermissionsFromValue(
      value.get(), &permissions, &bad_message, &error));
  ASSERT_TRUE(bad_message);
  bad_message = false;

  // Throw errors when "origins" or "permissions" are not list values.
  value->Clear();
  value->Set("origins", Value::CreateIntegerValue(2));
  ASSERT_FALSE(UnpackPermissionsFromValue(
      value.get(), &permissions, &bad_message, &error));
  ASSERT_TRUE(bad_message);
  bad_message = false;

  value->Clear();
  value->Set("permissions", Value::CreateIntegerValue(2));
  ASSERT_FALSE(UnpackPermissionsFromValue(
      value.get(), &permissions, &bad_message, &error));
  ASSERT_TRUE(bad_message);
  bad_message = false;

  // Additional fields should be allowed.
  value->Clear();
  value->Set("origins", origins->DeepCopy());
  value->Set("random", Value::CreateIntegerValue(3));
  ASSERT_TRUE(UnpackPermissionsFromValue(
      value.get(), &permissions, &bad_message, &error));
  ASSERT_FALSE(bad_message);
  ASSERT_TRUE(error.empty());
  ASSERT_TRUE(permissions->HasExplicitAccessToOrigin(GURL("http://a.com/")));

  // Unknown permissions should throw an error but not set the bad_message bit.
  value->Clear();
  invalid_apis.reset(apis->DeepCopy());
  invalid_apis->Append(Value::CreateStringValue("unknown_permission"));
  value->Set("permissions", invalid_apis->DeepCopy());
  ASSERT_FALSE(UnpackPermissionsFromValue(
      value.get(), &permissions, &bad_message, &error));
  ASSERT_FALSE(bad_message);
  ASSERT_FALSE(error.empty());
  ASSERT_EQ(error, "'unknown_permission' is not a recognized permission.");
}
