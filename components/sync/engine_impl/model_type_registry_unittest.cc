// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/model_type_registry.h"

#include <utility>

#include "base/deferred_sequenced_task_runner.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/test/gtest_util.h"
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
  void SetUp() override {
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

  void TearDown() override {
    registry_.reset();
    workers_.clear();
    test_user_share_.TearDown();
  }

  ModelTypeRegistry* registry() { return registry_.get(); }

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

  void SetDummyProgressMarkerForType(ModelType type) {
    sync_pb::DataTypeProgressMarker progress_marker;
    progress_marker.set_token("dummy");
    directory()->SetDownloadProgress(type, progress_marker);
  }

  bool migration_attempted() { return migration_attempted_; }

  syncable::MetahandleSet metahandles_to_purge() {
    return directory()->kernel()->metahandles_to_purge;
  }

 private:
  bool MigrateDirectory(ModelType type,
                        UserShare* user_share,
                        ModelTypeWorker* worker) {
    migration_attempted_ = true;
    return true;
  }

  syncable::Directory* directory() {
    return test_user_share_.user_share()->directory.get();
  }

  base::MessageLoop message_loop_;

  TestUserShare test_user_share_;
  std::vector<scoped_refptr<ModelSafeWorker>> workers_;
  std::unique_ptr<ModelTypeRegistry> registry_;
  MockNudgeHandler mock_nudge_handler_;
  bool migration_attempted_ = false;
};

// Tests operations with directory types.
// Registering/unregistering type should affect enabled types and handlers map.
// Registering/unregistering type twice should trigger DCHECK.
// Registering type with unknown ModelSafeGroup should trigger DCHECK.
TEST_F(ModelTypeRegistryTest, DirectoryTypes) {
  UpdateHandlerMap* update_handler_map = registry()->update_handler_map();
  EXPECT_TRUE(registry()->GetEnabledTypes().Empty());

  registry()->RegisterDirectoryType(AUTOFILL, GROUP_DB);
  EXPECT_EQ(ModelTypeSet(AUTOFILL), registry()->GetEnabledTypes());

  registry()->RegisterDirectoryType(BOOKMARKS, GROUP_UI);
  EXPECT_EQ(ModelTypeSet(AUTOFILL, BOOKMARKS), registry()->GetEnabledTypes());

  // Try registering already registered type.
  EXPECT_DCHECK_DEATH(registry()->RegisterDirectoryType(BOOKMARKS, GROUP_UI));

  EXPECT_TRUE(update_handler_map->find(AUTOFILL) != update_handler_map->end());
  EXPECT_TRUE(update_handler_map->find(BOOKMARKS) != update_handler_map->end());

  registry()->UnregisterDirectoryType(AUTOFILL);
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), registry()->GetEnabledTypes());
  EXPECT_TRUE(update_handler_map->find(AUTOFILL) == update_handler_map->end());
  EXPECT_TRUE(update_handler_map->find(BOOKMARKS) != update_handler_map->end());

  // Try unregistering already unregistered type.
  EXPECT_DCHECK_DEATH(registry()->UnregisterDirectoryType(AUTOFILL));

  // Try registering type with unknown worker.
  EXPECT_DCHECK_DEATH(
      registry()->RegisterDirectoryType(SESSIONS, GROUP_HISTORY));
}

TEST_F(ModelTypeRegistryTest, NonBlockingTypes) {
  EXPECT_TRUE(registry()->GetEnabledTypes().Empty());

  registry()->ConnectNonBlockingType(
      THEMES, MakeActivationContext(MakeInitialModelTypeState(THEMES)));
  EXPECT_EQ(ModelTypeSet(THEMES), registry()->GetEnabledTypes());

  registry()->ConnectNonBlockingType(
      SESSIONS, MakeActivationContext(MakeInitialModelTypeState(SESSIONS)));
  EXPECT_EQ(ModelTypeSet(THEMES, SESSIONS), registry()->GetEnabledTypes());

  registry()->DisconnectNonBlockingType(THEMES);
  EXPECT_EQ(ModelTypeSet(SESSIONS), registry()->GetEnabledTypes());

  // Allow ModelTypeRegistry destruction to delete the
  // Sessions' ModelTypeSyncWorker.
}

TEST_F(ModelTypeRegistryTest, NonBlockingTypesWithDirectoryTypes) {
  ModelTypeSet directory_types(NIGORI, BOOKMARKS, AUTOFILL);

  ModelTypeSet current_types;
  EXPECT_TRUE(registry()->GetEnabledTypes().Empty());

  // Add the themes non-blocking type.
  registry()->ConnectNonBlockingType(
      THEMES, MakeActivationContext(MakeInitialModelTypeState(THEMES)));
  current_types.Put(THEMES);
  EXPECT_EQ(current_types, registry()->GetEnabledTypes());

  // Add some directory types.
  for (auto it = directory_types.First(); it.Good(); it.Inc())
    registry()->RegisterDirectoryType(it.Get(), GROUP_PASSIVE);
  current_types.PutAll(directory_types);
  EXPECT_EQ(current_types, registry()->GetEnabledTypes());

  // Add sessions non-blocking type.
  registry()->ConnectNonBlockingType(
      SESSIONS, MakeActivationContext(MakeInitialModelTypeState(SESSIONS)));
  current_types.Put(SESSIONS);
  EXPECT_EQ(current_types, registry()->GetEnabledTypes());

  // Remove themes non-blocking type.
  registry()->DisconnectNonBlockingType(THEMES);
  current_types.Remove(THEMES);
  EXPECT_EQ(current_types, registry()->GetEnabledTypes());

  // Clear all directory types.
  for (auto it = directory_types.First(); it.Good(); it.Inc())
    registry()->UnregisterDirectoryType(it.Get());
  current_types.RemoveAll(directory_types);
  EXPECT_EQ(current_types, registry()->GetEnabledTypes());
}

// Tests correct result returned from GetInitialSyncEndedTypes.
TEST_F(ModelTypeRegistryTest, GetInitialSyncEndedTypes) {
  // Add two directory types.
  registry()->RegisterDirectoryType(AUTOFILL, GROUP_PASSIVE);
  registry()->RegisterDirectoryType(BOOKMARKS, GROUP_PASSIVE);

  // Only Autofill and Themes types finished initial sync.
  MarkInitialSyncEndedForDirectoryType(AUTOFILL);

  // Add two non-blocking type.
  sync_pb::ModelTypeState model_type_state = MakeInitialModelTypeState(THEMES);
  model_type_state.set_initial_sync_done(true);
  registry()->ConnectNonBlockingType(THEMES,
                                     MakeActivationContext(model_type_state));

  registry()->ConnectNonBlockingType(
      SESSIONS, MakeActivationContext(MakeInitialModelTypeState(SESSIONS)));

  EXPECT_EQ(ModelTypeSet(AUTOFILL, THEMES),
            registry()->GetInitialSyncEndedTypes());
}

// Tests that when directory data is present for type ConnectNonBlockingType
// triggers USS migration and purges old directory data.
TEST_F(ModelTypeRegistryTest, UssMigration) {
  EXPECT_FALSE(migration_attempted());

  MarkInitialSyncEndedForDirectoryType(THEMES);
  // Purge only proceeds in the presence of a progress marker for the type(s)
  // being purged.
  SetDummyProgressMarkerForType(THEMES);
  EXPECT_EQ(0u, metahandles_to_purge().size());
  registry()->ConnectNonBlockingType(
      THEMES, MakeActivationContext(MakeInitialModelTypeState(THEMES)));

  EXPECT_TRUE(migration_attempted());
  EXPECT_NE(0u, metahandles_to_purge().size());
}

}  // namespace syncer
