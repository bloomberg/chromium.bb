// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/schema_registry.h"

#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;
using ::testing::_;

namespace policy {

namespace {

const char kTestSchema[] =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"string\": { \"type\": \"string\" },"
    "    \"integer\": { \"type\": \"integer\" },"
    "    \"boolean\": { \"type\": \"boolean\" },"
    "    \"null\": { \"type\": \"null\" },"
    "    \"double\": { \"type\": \"number\" },"
    "    \"list\": {"
    "      \"type\": \"array\","
    "      \"items\": { \"type\": \"string\" }"
    "    },"
    "    \"object\": {"
    "      \"type\": \"object\","
    "      \"properties\": {"
    "        \"a\": { \"type\": \"string\" },"
    "        \"b\": { \"type\": \"integer\" }"
    "      }"
    "    }"
    "  }"
    "}";

class MockSchemaRegistryObserver : public SchemaRegistry::Observer {
 public:
  MockSchemaRegistryObserver() {}
  virtual ~MockSchemaRegistryObserver() {}

  MOCK_METHOD2(OnSchemaRegistryUpdated,
               void(const scoped_refptr<SchemaMap>&, bool));
};

}  // namespace

TEST(SchemaRegistryTest, Notifications) {
  std::string error;
  Schema schema = Schema::Parse(kTestSchema, &error);
  ASSERT_TRUE(schema.valid()) << error;

  MockSchemaRegistryObserver observer;
  SchemaRegistry registry;
  registry.AddObserver(&observer);

  ASSERT_TRUE(registry.schema_map());
  EXPECT_FALSE(registry.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc")));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, true));
  registry.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"),
                             schema);
  Mock::VerifyAndClearExpectations(&observer);

  // Re-register also triggers notifications, because the Schema might have
  // changed.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, true));
  registry.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"),
                             schema);
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_TRUE(registry.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc")));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, false));
  registry.UnregisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"));
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_FALSE(registry.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc")));

  // Registering multiple components at once issues only one notification.
  ComponentMap components;
  components["abc"] = schema;
  components["def"] = schema;
  components["xyz"] = schema;
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, true));
  registry.RegisterComponents(POLICY_DOMAIN_EXTENSIONS, components);
  Mock::VerifyAndClearExpectations(&observer);

  registry.RemoveObserver(&observer);
}

TEST(SchemaRegistryTest, Combined) {
  std::string error;
  Schema schema = Schema::Parse(kTestSchema, &error);
  ASSERT_TRUE(schema.valid()) << error;

  MockSchemaRegistryObserver observer;
  SchemaRegistry registry1;
  SchemaRegistry registry2;
  CombinedSchemaRegistry combined;
  combined.AddObserver(&observer);

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, _)).Times(0);
  registry1.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"),
                              schema);
  Mock::VerifyAndClearExpectations(&observer);

  // Starting to track a registry issues notifications when it comes with new
  // schemas.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, true));
  combined.Track(&registry1);
  Mock::VerifyAndClearExpectations(&observer);

  // Adding a new empty registry does not trigger notifications.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, _)).Times(0);
  combined.Track(&registry2);
  Mock::VerifyAndClearExpectations(&observer);

  // Adding the same component to the combined registry itself triggers
  // notifications.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, true));
  combined.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"),
                             schema);
  Mock::VerifyAndClearExpectations(&observer);

  // Adding components to the sub-registries triggers notifications.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, true));
  registry2.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "def"),
                              schema);
  Mock::VerifyAndClearExpectations(&observer);

  // If the same component is published in 2 sub-registries then the combined
  // registry publishes one of them.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, true));
  registry1.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "def"),
                              schema);
  Mock::VerifyAndClearExpectations(&observer);

  ASSERT_EQ(1u, combined.schema_map()->GetDomains().size());
  ASSERT_TRUE(combined.schema_map()->GetComponents(POLICY_DOMAIN_EXTENSIONS));
  ASSERT_EQ(
      2u,
      combined.schema_map()->GetComponents(POLICY_DOMAIN_EXTENSIONS)->size());
  EXPECT_TRUE(combined.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc")));
  EXPECT_TRUE(combined.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "def")));
  EXPECT_FALSE(combined.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "xyz")));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, false));
  registry1.UnregisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"));
  Mock::VerifyAndClearExpectations(&observer);
  // Still registered at the combined registry.
  EXPECT_TRUE(combined.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc")));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, false));
  combined.UnregisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"));
  Mock::VerifyAndClearExpectations(&observer);
  // Now it's gone.
  EXPECT_FALSE(combined.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc")));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, false));
  registry1.UnregisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "def"));
  Mock::VerifyAndClearExpectations(&observer);
  // Still registered at registry2.
  EXPECT_TRUE(combined.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "def")));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, false));
  registry2.UnregisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "def"));
  Mock::VerifyAndClearExpectations(&observer);
  // Now it's gone.
  EXPECT_FALSE(combined.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "def")));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, true)).Times(2);
  registry1.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_CHROME, ""),
                              schema);
  registry2.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "hij"),
                              schema);
  Mock::VerifyAndClearExpectations(&observer);

  // Untracking |registry1| doesn't trigger an update nofitication, because it
  // doesn't contain any components.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, _)).Times(0);
  combined.Untrack(&registry1);
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_, false));
  combined.Untrack(&registry2);
  Mock::VerifyAndClearExpectations(&observer);

  combined.RemoveObserver(&observer);
}

}  // namespace policy
