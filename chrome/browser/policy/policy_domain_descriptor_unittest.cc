// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_domain_descriptor.h"

#include "base/values.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_schema.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(PolicyDomainDescriptor, FilterBundle) {
  scoped_refptr<PolicyDomainDescriptor> descriptor =
      new PolicyDomainDescriptor(POLICY_DOMAIN_EXTENSIONS);
  EXPECT_EQ(POLICY_DOMAIN_EXTENSIONS, descriptor->domain());
  EXPECT_TRUE(descriptor->components().empty());

  std::string error;
  scoped_ptr<PolicySchema> schema = PolicySchema::Parse(
      "{"
      "  \"$schema\":\"http://json-schema.org/draft-03/schema#\","
      "  \"type\":\"object\","
      "  \"properties\": {"
      "    \"Array\": {"
      "      \"type\": \"array\","
      "      \"items\": { \"type\": \"string\" }"
      "    },"
      "    \"Boolean\": { \"type\": \"boolean\" },"
      "    \"Integer\": { \"type\": \"integer\" },"
      "    \"Null\": { \"type\": \"null\" },"
      "    \"Number\": { \"type\": \"number\" },"
      "    \"Object\": {"
      "      \"type\": \"object\","
      "      \"properties\": {"
      "        \"a\": { \"type\": \"string\" },"
      "        \"b\": { \"type\": \"integer\" }"
      "      }"
      "    },"
      "    \"String\": { \"type\": \"string\" }"
      "  }"
      "}", &error);
  ASSERT_TRUE(schema) << error;

  descriptor->RegisterComponent("abc", schema.Pass());

  EXPECT_EQ(1u, descriptor->components().size());
  EXPECT_EQ(1u, descriptor->components().count("abc"));

  PolicyBundle bundle;
  descriptor->FilterBundle(&bundle);
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(bundle.Equals(empty_bundle));

  // Other namespaces aren't filtered.
  PolicyBundle expected_bundle;
  PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, "");
  expected_bundle.Get(chrome_ns).Set("ChromePolicy",
                                     POLICY_LEVEL_MANDATORY,
                                     POLICY_SCOPE_USER,
                                     base::Value::CreateStringValue("value"));
  bundle.CopyFrom(expected_bundle);
  // Unknown components of the domain are filtered out.
  PolicyNamespace another_extension_ns(POLICY_DOMAIN_EXTENSIONS, "xyz");
  bundle.Get(another_extension_ns).Set(
      "AnotherExtensionPolicy",
      POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER,
      base::Value::CreateStringValue("value"));
  descriptor->FilterBundle(&bundle);
  EXPECT_TRUE(bundle.Equals(expected_bundle));

  PolicyNamespace extension_ns(POLICY_DOMAIN_EXTENSIONS, "abc");
  PolicyMap& map = expected_bundle.Get(extension_ns);
  base::ListValue list;
  list.AppendString("a");
  list.AppendString("b");
  map.Set("Array", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          list.DeepCopy());
  map.Set("Boolean", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          base::Value::CreateBooleanValue(true));
  map.Set("Integer", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          base::Value::CreateIntegerValue(1));
  map.Set("Null", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          base::Value::CreateNullValue());
  map.Set("Number", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          base::Value::CreateDoubleValue(1.2));
  base::DictionaryValue dict;
  dict.SetString("a", "b");
  dict.SetInteger("b", 2);
  map.Set("Object", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, dict.DeepCopy());
  map.Set("String", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          base::Value::CreateStringValue("value"));

  bundle.MergeFrom(expected_bundle);
  bundle.Get(extension_ns).Set("Unexpected",
                               POLICY_LEVEL_MANDATORY,
                               POLICY_SCOPE_USER,
                               base::Value::CreateStringValue("to-be-removed"));

  descriptor->FilterBundle(&bundle);
  EXPECT_TRUE(bundle.Equals(expected_bundle));

  // Mismatched types are also removed.
  bundle.Clear();
  PolicyMap& badmap = bundle.Get(extension_ns);
  badmap.Set("Array", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false));
  badmap.Set("Boolean", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateIntegerValue(0));
  badmap.Set("Integer", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false));
  badmap.Set("Null", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false));
  badmap.Set("Number", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false));
  badmap.Set("Object", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false));
  badmap.Set("String", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false));

  descriptor->FilterBundle(&bundle);
  EXPECT_TRUE(bundle.Equals(empty_bundle));
}

}  // namespace policy
