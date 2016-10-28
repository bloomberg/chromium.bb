// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/model_type_registry.h"

#include <utility>

#include "base/deferred_sequenced_task_runner.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "components/sync/engine/activation_context.h"
#include "components/sync/engine/fake_model_type_processor.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/model_neutral_mutable_entry.h"
#include "components/sync/syncable/syncable_model_neutral_write_transaction.h"
#include "components/sync/syncable/test_user_share.h"
#include "components/sync/test/engine/fake_model_worker.h"
#include "components/sync/test/engine/mock_nudge_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class ModelTypeRegistryTest : public ::testing::Test {
 public:
  ModelTypeRegistryTest();
  void SetUp() override;
  void TearDown() override;

  ModelTypeRegistry* registry();

  static sync_pb::ModelTypeState MakeInitialModelTypeState(ModelType type) {
    sync_pb::ModelTypeState state;
    state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(type));
    return state;
  }

  static std::unique_ptr<ActivationContext> MakeActivationContext(
      const sync_pb::ModelTypeState& model_type_state) {
    auto context = base::MakeUnique<ActivationContext>();
    context->model_type_state = model_type_state;
    context->type_processor = base::MakeUnique<FakeModelTypeProcessor>();
    return context;
  }

  void MarkInitialSyncEndedForDirectoryType(ModelType type) {
    syncable::ModelNeutralWriteTransaction trans(FROM_HERE, syncable::SYNCER,
                                                 directory());
    syncable::ModelNeutralMutableEntry entry(
        &trans, syncable::CREATE_NEW_TYPE_ROOT, type);
    ASSERT_TRUE(entry.good());
    entry.PutServerIsDir(true);
    entry.PutUniqueServerTag(ModelTypeToRootTag(type));
    directory()->MarkInitialSyncEndedForType(&trans, type);
  }

  bool migration_attempted() { return migration_attempted_; }

 private:
  bool MigrateDirectory(ModelType type,
                        UserShare* user_share,
                        ModelTypeWorker* worker) {
    migration_attempted_ = true;
    return true;
  }

  syncable::Directory* directory();

  base::MessageLoop message_loop_;

  TestUserShare test_user_share_;
  std::vector<scoped_refptr<ModelSafeWorker>> workers_;
  std::unique_ptr<ModelTypeRegistry> registry_;
  MockNudgeHandler mock_nudge_handler_;
  bool migration_attempted_ = false;
};

ModelTypeRegistryTest::ModelTypeRegistryTest() {}

void ModelTypeRegistryTest::SetUp() {
  test_user_share_.SetUp();
  scoped_refptr<ModelSafeWorker> passive_worker(
      new FakeModelWorker(GROUP_PASSIVE));
  scoped_refptr<ModelSafeWorker> ui_worker(new FakeModelWorker(GROUP_UI));
  scoped_refptr<ModelSafeWorker> db_worker(new FakeModelWorker(GROUP_DB));
  workers_.push_back(passive_worker);
  workers_.push_back(ui_worker);
  workers_.push_back(db_worker);

  registry_ = base::MakeUnique<ModelTypeRegistry>(
      workers_, test_user_share_.user_share(), &mock_nudge_handler_,
      base::Bind(&ModelTypeRegistryTest::MigrateDirectory,
                 base::Unretained(this)));
}

void ModelTypeRegistryTest::TearDown() {
  registry_.reset();
  workers_.clear();
  test_user_share_.TearDown();
}

ModelTypeRegistry* ModelTypeRegistryTest::registry() {
  return registry_.get();
}

syncable::Directory* ModelTypeRegistryTest::directory() {
  return test_user_share_.user_share()->directory.get();
}

// Create some directory update handlers and commit contributors.
//
// We don't get to inspect any of the state we're modifying.  This test is
// useful only for detecting crashes or memory leaks.
TEST_F(ModelTypeRegistryTest, SetEnabledDirectoryTypes_Once) {
  ModelSafeRoutingInfo routing_info;
  routing_info.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info.insert(std::make_pair(BOOKMARKS, GROUP_UI));
  routing_info.insert(std::make_pair(AUTOFILL, GROUP_DB));
  routing_info.insert(std::make_pair(APPS, GROUP_NON_BLOCKING));

  registry()->SetEnabledDirectoryTypes(routing_info);

  UpdateHandlerMap* update_handler_map = registry()->update_handler_map();
  // Apps is non-blocking type, SetEnabledDirectoryTypes shouldn't instantiate
  // update_handler for it.
  EXPECT_TRUE(update_handler_map->find(APPS) == update_handler_map->end());
}

// Try two different routing info settings.
//
// We don't get to inspect any of the state we're modifying.  This test is
// useful only for detecting crashes or memory leaks.
TEST_F(ModelTypeRegistryTest, SetEnabledDirectoryTypes_Repeatedly) {
  ModelSafeRoutingInfo routing_info1;
  routing_info1.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info1.insert(std::make_pair(BOOKMARKS, GROUP_PASSIVE));
  routing_info1.insert(std::make_pair(AUTOFILL, GROUP_PASSIVE));
  routing_info1.insert(std::make_pair(APPS, GROUP_NON_BLOCKING));

  registry()->SetEnabledDirectoryTypes(routing_info1);

  ModelSafeRoutingInfo routing_info2;
  routing_info2.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info2.insert(std::make_pair(BOOKMARKS, GROUP_UI));
  routing_info2.insert(std::make_pair(AUTOFILL, GROUP_DB));
  routing_info2.insert(std::make_pair(APPS, GROUP_NON_BLOCKING));

  registry()->SetEnabledDirectoryTypes(routing_info2);
}

// Test removing all types from the list.
//
// We don't get to inspect any of the state we're modifying.  This test is
// useful only for detecting crashes or memory leaks.
TEST_F(ModelTypeRegistryTest, SetEnabledDirectoryTypes_Clear) {
  ModelSafeRoutingInfo routing_info1;
  routing_info1.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info1.insert(std::make_pair(BOOKMARKS, GROUP_UI));
  routing_info1.insert(std::make_pair(AUTOFILL, GROUP_DB));
  routing_info1.insert(std::make_pair(APPS, GROUP_NON_BLOCKING));

  registry()->SetEnabledDirectoryTypes(routing_info1);

  ModelSafeRoutingInfo routing_info2;
  registry()->SetEnabledDirectoryTypes(routing_info2);
}

// Test disabling then re-enabling some directory types.
//
// We don't get to inspect any of the state we're modifying.  This test is
// useful only for detecting crashes or memory leaks.
TEST_F(ModelTypeRegistryTest, SetEnabledDirectoryTypes_OffAndOn) {
  ModelSafeRoutingInfo routing_info1;
  routing_info1.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info1.insert(std::make_pair(BOOKMARKS, GROUP_UI));
  routing_info1.insert(std::make_pair(AUTOFILL, GROUP_DB));
  routing_info1.insert(std::make_pair(APPS, GROUP_NON_BLOCKING));

  registry()->SetEnabledDirectoryTypes(routing_info1);

  ModelSafeRoutingInfo routing_info2;
  registry()->SetEnabledDirectoryTypes(routing_info2);

  registry()->SetEnabledDirectoryTypes(routing_info1);
}

TEST_F(ModelTypeRegistryTest, NonBlockingTypes) {
  EXPECT_TRUE(registry()->GetEnabledTypes().Empty());

  registry()->ConnectType(
      THEMES, MakeActivationContext(MakeInitialModelTypeState(THEMES)));
  EXPECT_EQ(ModelTypeSet(THEMES), registry()->GetEnabledTypes());

  registry()->ConnectType(
      SESSIONS, MakeActivationContext(MakeInitialModelTypeState(SESSIONS)));
  EXPECT_EQ(ModelTypeSet(THEMES, SESSIONS), registry()->GetEnabledTypes());

  registry()->DisconnectType(THEMES);
  EXPECT_EQ(ModelTypeSet(SESSIONS), registry()->GetEnabledTypes());

  // Allow ModelTypeRegistry destruction to delete the
  // Sessions' ModelTypeSyncWorker.
}

TEST_F(ModelTypeRegistryTest, NonBlockingTypesWithDirectoryTypes) {
  ModelSafeRoutingInfo routing_info1;
  routing_info1.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info1.insert(std::make_pair(BOOKMARKS, GROUP_UI));
  routing_info1.insert(std::make_pair(AUTOFILL, GROUP_DB));
  routing_info1.insert(std::make_pair(THEMES, GROUP_NON_BLOCKING));
  routing_info1.insert(std::make_pair(SESSIONS, GROUP_NON_BLOCKING));

  ModelTypeSet directory_types(NIGORI, BOOKMARKS, AUTOFILL);

  ModelTypeSet current_types;
  EXPECT_TRUE(registry()->GetEnabledTypes().Empty());

  // Add the themes non-blocking type.
  registry()->ConnectType(
      THEMES, MakeActivationContext(MakeInitialModelTypeState(THEMES)));
  current_types.Put(THEMES);
  EXPECT_EQ(current_types, registry()->GetEnabledTypes());

  // Add some directory types.
  registry()->SetEnabledDirectoryTypes(routing_info1);
  current_types.PutAll(directory_types);
  EXPECT_EQ(current_types, registry()->GetEnabledTypes());

  // Add sessions non-blocking type.
  registry()->ConnectType(
      SESSIONS, MakeActivationContext(MakeInitialModelTypeState(SESSIONS)));
  current_types.Put(SESSIONS);
  EXPECT_EQ(current_types, registry()->GetEnabledTypes());

  // Remove themes non-blocking type.
  registry()->DisconnectType(THEMES);
  current_types.Remove(THEMES);
  EXPECT_EQ(current_types, registry()->GetEnabledTypes());

  // Clear all directory types.
  ModelSafeRoutingInfo routing_info2;
  registry()->SetEnabledDirectoryTypes(routing_info2);
  current_types.RemoveAll(directory_types);
  EXPECT_EQ(current_types, registry()->GetEnabledTypes());
}

// Tests correct result returned from GetInitialSyncEndedTypes.
TEST_F(ModelTypeRegistryTest, GetInitialSyncEndedTypes) {
  ModelSafeRoutingInfo routing_info;
  // Add two directory and two non-blocking types.
  routing_info.insert(std::make_pair(AUTOFILL, GROUP_PASSIVE));
  routing_info.insert(std::make_pair(BOOKMARKS, GROUP_PASSIVE));
  routing_info.insert(std::make_pair(THEMES, GROUP_NON_BLOCKING));
  routing_info.insert(std::make_pair(SESSIONS, GROUP_NON_BLOCKING));
  registry()->SetEnabledDirectoryTypes(routing_info);

  // Only Autofill and Themes types finished initial sync.
  MarkInitialSyncEndedForDirectoryType(AUTOFILL);

  sync_pb::ModelTypeState model_type_state = MakeInitialModelTypeState(THEMES);
  model_type_state.set_initial_sync_done(true);
  registry()->ConnectType(THEMES, MakeActivationContext(model_type_state));

  EXPECT_EQ(ModelTypeSet(AUTOFILL, THEMES),
            registry()->GetInitialSyncEndedTypes());
}

TEST_F(ModelTypeRegistryTest, UssMigrationAttempted) {
  EXPECT_FALSE(migration_attempted());

  MarkInitialSyncEndedForDirectoryType(THEMES);
  registry()->ConnectType(
      THEMES, MakeActivationContext(MakeInitialModelTypeState(THEMES)));

  EXPECT_TRUE(migration_attempted());
}

}  // namespace syncer
