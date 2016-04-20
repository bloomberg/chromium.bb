// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema_registry.h"

#include <memory>

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

  MOCK_METHOD1(OnSchemaRegistryUpdated, void(bool));
  MOCK_METHOD0(OnSchemaRegistryReady, void());
};

bool SchemaMapEquals(const scoped_refptr<SchemaMap>& schema_map1,
                     const scoped_refptr<SchemaMap>& schema_map2) {
  PolicyNamespaceList added;
  PolicyNamespaceList removed;
  schema_map1->GetChanges(schema_map2, &removed, &added);
  return added.empty() && removed.empty();
}

}  // namespace

TEST(SchemaRegistryTest, Notifications) {
  std::string error;
  Schema schema = Schema::Parse(kTestSchema, &error);
  ASSERT_TRUE(schema.valid()) << error;

  MockSchemaRegistryObserver observer;
  SchemaRegistry registry;
  registry.AddObserver(&observer);

  ASSERT_TRUE(registry.schema_map().get());
  EXPECT_FALSE(registry.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc")));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(true));
  registry.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"),
                             schema);
  Mock::VerifyAndClearExpectations(&observer);

  // Re-register also triggers notifications, because the Schema might have
  // changed.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(true));
  registry.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"),
                             schema);
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_TRUE(registry.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc")));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(false));
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
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(true));
  registry.RegisterComponents(POLICY_DOMAIN_EXTENSIONS, components);
  Mock::VerifyAndClearExpectations(&observer);

  registry.RemoveObserver(&observer);
}

TEST(SchemaRegistryTest, IsReady) {
  SchemaRegistry registry;
  MockSchemaRegistryObserver observer;
  registry.AddObserver(&observer);

  EXPECT_FALSE(registry.IsReady());
#if defined(ENABLE_EXTENSIONS)
  EXPECT_CALL(observer, OnSchemaRegistryReady()).Times(0);
  registry.SetReady(POLICY_DOMAIN_EXTENSIONS);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_FALSE(registry.IsReady());
#endif
  EXPECT_CALL(observer, OnSchemaRegistryReady());
  registry.SetReady(POLICY_DOMAIN_CHROME);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(registry.IsReady());
  EXPECT_CALL(observer, OnSchemaRegistryReady()).Times(0);
  registry.SetReady(POLICY_DOMAIN_CHROME);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(registry.IsReady());

  CombinedSchemaRegistry combined;
  EXPECT_TRUE(combined.IsReady());

  registry.RemoveObserver(&observer);
}

TEST(SchemaRegistryTest, Combined) {
  std::string error;
  Schema schema = Schema::Parse(kTestSchema, &error);
  ASSERT_TRUE(schema.valid()) << error;

  MockSchemaRegistryObserver observer;
  std::unique_ptr<SchemaRegistry> registry1(new SchemaRegistry);
  std::unique_ptr<SchemaRegistry> registry2(new SchemaRegistry);
  CombinedSchemaRegistry combined;
  combined.AddObserver(&observer);

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_)).Times(0);
  registry1->RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"),
                               schema);
  Mock::VerifyAndClearExpectations(&observer);

  // Starting to track a registry issues notifications when it comes with new
  // schemas.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(true));
  combined.Track(registry1.get());
  Mock::VerifyAndClearExpectations(&observer);

  // Adding a new empty registry does not trigger notifications.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_)).Times(0);
  combined.Track(registry2.get());
  Mock::VerifyAndClearExpectations(&observer);

  // Adding the same component to the combined registry itself triggers
  // notifications.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(true));
  combined.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"),
                             schema);
  Mock::VerifyAndClearExpectations(&observer);

  // Adding components to the sub-registries triggers notifications.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(true));
  registry2->RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "def"),
                               schema);
  Mock::VerifyAndClearExpectations(&observer);

  // If the same component is published in 2 sub-registries then the combined
  // registry publishes one of them.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(true));
  registry1->RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "def"),
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

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(false));
  registry1->UnregisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"));
  Mock::VerifyAndClearExpectations(&observer);
  // Still registered at the combined registry.
  EXPECT_TRUE(combined.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc")));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(false));
  combined.UnregisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"));
  Mock::VerifyAndClearExpectations(&observer);
  // Now it's gone.
  EXPECT_FALSE(combined.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc")));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(false));
  registry1->UnregisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "def"));
  Mock::VerifyAndClearExpectations(&observer);
  // Still registered at registry2.
  EXPECT_TRUE(combined.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "def")));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(false));
  registry2->UnregisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "def"));
  Mock::VerifyAndClearExpectations(&observer);
  // Now it's gone.
  EXPECT_FALSE(combined.schema_map()->GetSchema(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "def")));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(true)).Times(2);
  registry1->RegisterComponent(PolicyNamespace(POLICY_DOMAIN_CHROME, ""),
                               schema);
  registry2->RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "hij"),
                               schema);
  Mock::VerifyAndClearExpectations(&observer);

  // Untracking |registry1| doesn't trigger an update notification, because it
  // doesn't contain any components.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(_)).Times(0);
  registry1.reset();
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(false));
  registry2.reset();
  Mock::VerifyAndClearExpectations(&observer);

  combined.RemoveObserver(&observer);
}

TEST(SchemaRegistryTest, ForwardingSchemaRegistry) {
  std::unique_ptr<SchemaRegistry> registry(new SchemaRegistry);
  ForwardingSchemaRegistry forwarding(registry.get());
  MockSchemaRegistryObserver observer;
  forwarding.AddObserver(&observer);

  EXPECT_FALSE(registry->IsReady());
  EXPECT_FALSE(forwarding.IsReady());
  // They always have the same SchemaMap.
  EXPECT_TRUE(SchemaMapEquals(registry->schema_map(), forwarding.schema_map()));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(true));
  registry->RegisterComponent(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"),
                              Schema());
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(SchemaMapEquals(registry->schema_map(), forwarding.schema_map()));

  EXPECT_CALL(observer, OnSchemaRegistryUpdated(false));
  registry->UnregisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "abc"));
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_TRUE(SchemaMapEquals(registry->schema_map(), forwarding.schema_map()));

  // No notifications expected for these calls.
  EXPECT_FALSE(registry->IsReady());
  EXPECT_FALSE(forwarding.IsReady());

  registry->SetReady(POLICY_DOMAIN_EXTENSIONS);
  EXPECT_FALSE(registry->IsReady());
  EXPECT_FALSE(forwarding.IsReady());

  registry->SetReady(POLICY_DOMAIN_CHROME);
  EXPECT_TRUE(registry->IsReady());
  // The ForwardingSchemaRegistry becomes ready independently of the wrapped
  // registry.
  EXPECT_FALSE(forwarding.IsReady());

  EXPECT_TRUE(SchemaMapEquals(registry->schema_map(), forwarding.schema_map()));
  Mock::VerifyAndClearExpectations(&observer);

  forwarding.SetReady(POLICY_DOMAIN_EXTENSIONS);
  EXPECT_FALSE(forwarding.IsReady());
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnSchemaRegistryReady());
  forwarding.SetReady(POLICY_DOMAIN_CHROME);
  EXPECT_TRUE(forwarding.IsReady());
  Mock::VerifyAndClearExpectations(&observer);

  // Keep the same SchemaMap when the original registry is gone.
  // No notifications are expected in this case either.
  scoped_refptr<SchemaMap> schema_map = registry->schema_map();
  registry.reset();
  EXPECT_TRUE(SchemaMapEquals(schema_map, forwarding.schema_map()));
  Mock::VerifyAndClearExpectations(&observer);

  forwarding.RemoveObserver(&observer);
}

}  // namespace policy
