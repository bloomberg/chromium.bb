// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_domain_descriptor.h"

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/external_data_manager.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class PolicyDomainDescriptorTest : public testing::Test {
 protected:
  scoped_ptr<ExternalDataFetcher> CreateExternalDataFetcher() const;
};

scoped_ptr<ExternalDataFetcher>
    PolicyDomainDescriptorTest::CreateExternalDataFetcher() const {
  return make_scoped_ptr(
      new ExternalDataFetcher(base::WeakPtr<ExternalDataManager>(),
                              std::string()));
}

TEST_F(PolicyDomainDescriptorTest, FilterBundle) {
  scoped_refptr<PolicyDomainDescriptor> descriptor =
      new PolicyDomainDescriptor(POLICY_DOMAIN_EXTENSIONS);
  EXPECT_EQ(POLICY_DOMAIN_EXTENSIONS, descriptor->domain());
  EXPECT_TRUE(descriptor->components().empty());

  std::string error;
  scoped_ptr<SchemaOwner> schema = SchemaOwner::Parse(
      "{"
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
                                     base::Value::CreateStringValue("value"),
                                     NULL);
  bundle.CopyFrom(expected_bundle);
  // Unknown components of the domain are filtered out.
  PolicyNamespace another_extension_ns(POLICY_DOMAIN_EXTENSIONS, "xyz");
  bundle.Get(another_extension_ns).Set(
      "AnotherExtensionPolicy",
      POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER,
      base::Value::CreateStringValue("value"),
      NULL);
  descriptor->FilterBundle(&bundle);
  EXPECT_TRUE(bundle.Equals(expected_bundle));

  PolicyNamespace extension_ns(POLICY_DOMAIN_EXTENSIONS, "abc");
  PolicyMap& map = expected_bundle.Get(extension_ns);
  base::ListValue list;
  list.AppendString("a");
  list.AppendString("b");
  map.Set("Array", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          list.DeepCopy(), NULL);
  map.Set("Boolean", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          base::Value::CreateBooleanValue(true), NULL);
  map.Set("Integer", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          base::Value::CreateIntegerValue(1), NULL);
  map.Set("Null", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          base::Value::CreateNullValue(), NULL);
  map.Set("Number", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          base::Value::CreateDoubleValue(1.2), NULL);
  base::DictionaryValue dict;
  dict.SetString("a", "b");
  dict.SetInteger("b", 2);
  map.Set("Object", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          dict.DeepCopy(), NULL);
  map.Set("String", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          base::Value::CreateStringValue("value"), NULL);

  bundle.MergeFrom(expected_bundle);
  bundle.Get(extension_ns).Set("Unexpected",
                               POLICY_LEVEL_MANDATORY,
                               POLICY_SCOPE_USER,
                               base::Value::CreateStringValue("to-be-removed"),
                               NULL);

  descriptor->FilterBundle(&bundle);
  EXPECT_TRUE(bundle.Equals(expected_bundle));

  // Mismatched types are also removed.
  bundle.Clear();
  PolicyMap& badmap = bundle.Get(extension_ns);
  badmap.Set("Array", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false), NULL);
  badmap.Set("Boolean", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateIntegerValue(0), NULL);
  badmap.Set("Integer", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false), NULL);
  badmap.Set("Null", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false), NULL);
  badmap.Set("Number", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false), NULL);
  badmap.Set("Object", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false), NULL);
  badmap.Set("String", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             NULL, CreateExternalDataFetcher().release());

  descriptor->FilterBundle(&bundle);
  EXPECT_TRUE(bundle.Equals(empty_bundle));
}

TEST_F(PolicyDomainDescriptorTest, LegacyComponents) {
  scoped_refptr<PolicyDomainDescriptor> descriptor =
      new PolicyDomainDescriptor(POLICY_DOMAIN_EXTENSIONS);
  EXPECT_EQ(POLICY_DOMAIN_EXTENSIONS, descriptor->domain());
  EXPECT_TRUE(descriptor->components().empty());

  std::string error;
  scoped_ptr<SchemaOwner> schema = SchemaOwner::Parse(
      "{"
      "  \"type\":\"object\","
      "  \"properties\": {"
      "    \"String\": { \"type\": \"string\" }"
      "  }"
      "}", &error);
  ASSERT_TRUE(schema) << error;

  descriptor->RegisterComponent("with-schema", schema.Pass());
  descriptor->RegisterComponent("without-schema", scoped_ptr<SchemaOwner>());

  EXPECT_EQ(2u, descriptor->components().size());

  // |bundle| contains policies loaded by a policy provider.
  PolicyBundle bundle;

  // Known components with schemas are filtered.
  PolicyNamespace extension_ns(POLICY_DOMAIN_EXTENSIONS, "with-schema");
  bundle.Get(extension_ns).Set("String",
                               POLICY_LEVEL_MANDATORY,
                               POLICY_SCOPE_USER,
                               base::Value::CreateStringValue("value 1"),
                               NULL);

  // Known components without a schema are not filtered.
  PolicyNamespace without_schema_ns(POLICY_DOMAIN_EXTENSIONS, "without-schema");
  bundle.Get(without_schema_ns).Set("Schemaless",
                                    POLICY_LEVEL_MANDATORY,
                                    POLICY_SCOPE_USER,
                                    base::Value::CreateStringValue("value 2"),
                                    NULL);

  // Other namespaces aren't filtered.
  PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, "");
  bundle.Get(chrome_ns).Set("ChromePolicy",
                            POLICY_LEVEL_MANDATORY,
                            POLICY_SCOPE_USER,
                            base::Value::CreateStringValue("value 3"),
                            NULL);

  PolicyBundle expected_bundle;
  expected_bundle.MergeFrom(bundle);

  // Unknown policies of known components with a schema are removed.
  bundle.Get(extension_ns).Set("Surprise",
                               POLICY_LEVEL_MANDATORY,
                               POLICY_SCOPE_USER,
                               base::Value::CreateStringValue("value 4"),
                               NULL);

  // Unknown components are removed.
  PolicyNamespace unknown_ns(POLICY_DOMAIN_EXTENSIONS, "unknown");
  bundle.Get(unknown_ns).Set("Surprise",
                             POLICY_LEVEL_MANDATORY,
                             POLICY_SCOPE_USER,
                             base::Value::CreateStringValue("value 5"),
                             NULL);

  descriptor->FilterBundle(&bundle);
  EXPECT_TRUE(bundle.Equals(expected_bundle));
}

}  // namespace policy
