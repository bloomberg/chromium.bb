// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_backend_host_impl.h"

#include <cstddef>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/invalidation/impl/invalidator_storage.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/sync/base/experiments.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/test_unrecoverable_error_handler.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/driver/fake_sync_client.h"
#include "components/sync/driver/sync_frontend.h"
#include "components/sync/engine/cycle/commit_counters.h"
#include "components/sync/engine/cycle/status_counters.h"
#include "components/sync/engine/cycle/update_counters.h"
#include "components/sync/engine/fake_sync_manager.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/engine/net/http_bridge_network_resources.h"
#include "components/sync/engine/net/network_resources.h"
#include "components/sync/engine/passive_model_worker.h"
#include "components/sync/engine/sync_manager_factory.h"
#include "components/sync/test/callback_counter.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google/cacheinvalidation/include/types.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::InvokeWithoutArgs;
using ::testing::StrictMock;
using ::testing::_;

namespace syncer {

namespace {

static const base::FilePath::CharType kTestSyncDir[] =
    FILE_PATH_LITERAL("sync-test");

ACTION_P(Signal, event) {
  event->Signal();
}

void EmptyNetworkTimeUpdate(const base::Time&,
                            const base::TimeDelta&,
                            const base::TimeDelta&) {}

void QuitMessageLoop() {
  base::MessageLoop::current()->QuitWhenIdle();
}

class MockSyncFrontend : public SyncFrontend {
 public:
  virtual ~MockSyncFrontend() {}

  MOCK_METHOD4(OnBackendInitialized,
               void(const WeakHandle<JsBackend>&,
                    const WeakHandle<DataTypeDebugInfoListener>&,
                    const std::string&,
                    bool));
  MOCK_METHOD0(OnSyncCycleCompleted, void());
  MOCK_METHOD1(OnConnectionStatusChange, void(ConnectionStatus status));
  MOCK_METHOD0(OnClearServerDataSucceeded, void());
  MOCK_METHOD0(OnClearServerDataFailed, void());
  MOCK_METHOD2(OnPassphraseRequired,
               void(PassphraseRequiredReason, const sync_pb::EncryptedData&));
  MOCK_METHOD0(OnPassphraseAccepted, void());
  MOCK_METHOD2(OnEncryptedTypesChanged, void(ModelTypeSet, bool));
  MOCK_METHOD0(OnEncryptionComplete, void());
  MOCK_METHOD1(OnMigrationNeededForTypes, void(ModelTypeSet));
  MOCK_METHOD1(OnProtocolEvent, void(const ProtocolEvent&));
  MOCK_METHOD2(OnDirectoryTypeCommitCounterUpdated,
               void(ModelType, const CommitCounters&));
  MOCK_METHOD2(OnDirectoryTypeUpdateCounterUpdated,
               void(ModelType, const UpdateCounters&));
  MOCK_METHOD2(OnDatatypeStatusCounterUpdated,
               void(ModelType, const StatusCounters&));
  MOCK_METHOD1(OnExperimentsChanged, void(const Experiments&));
  MOCK_METHOD1(OnActionableError, void(const SyncProtocolError& sync_error));
  MOCK_METHOD0(OnSyncConfigureRetry, void());
  MOCK_METHOD1(OnLocalSetPassphraseEncryption,
               void(const SyncEncryptionHandler::NigoriState& nigori_state));
};

class FakeSyncManagerFactory : public SyncManagerFactory {
 public:
  explicit FakeSyncManagerFactory(FakeSyncManager** fake_manager)
      : fake_manager_(fake_manager) {
    *fake_manager_ = nullptr;
  }
  ~FakeSyncManagerFactory() override {}

  // SyncManagerFactory implementation.  Called on the sync thread.
  std::unique_ptr<SyncManager> CreateSyncManager(
      const std::string& /* name */) override {
    *fake_manager_ =
        new FakeSyncManager(initial_sync_ended_types_, progress_marker_types_,
                            configure_fail_types_);
    return std::unique_ptr<SyncManager>(*fake_manager_);
  }

  void set_initial_sync_ended_types(ModelTypeSet types) {
    initial_sync_ended_types_ = types;
  }

  void set_progress_marker_types(ModelTypeSet types) {
    progress_marker_types_ = types;
  }

  void set_configure_fail_types(ModelTypeSet types) {
    configure_fail_types_ = types;
  }

 private:
  ModelTypeSet initial_sync_ended_types_;
  ModelTypeSet progress_marker_types_;
  ModelTypeSet configure_fail_types_;
  FakeSyncManager** fake_manager_;
};

class BackendSyncClient : public FakeSyncClient {
 public:
  scoped_refptr<ModelSafeWorker> CreateModelWorkerForGroup(
      ModelSafeGroup group) override {
    switch (group) {
      case GROUP_PASSIVE:
        return new PassiveModelWorker();
      default:
        return nullptr;
    }
  }
};

class SyncBackendHostTest : public testing::Test {
 protected:
  SyncBackendHostTest()
      : sync_thread_("SyncThreadForTest"), fake_manager_(nullptr) {}

  ~SyncBackendHostTest() override {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());

    sync_prefs_ = base::MakeUnique<SyncPrefs>(&pref_service_);
    sync_thread_.StartAndWaitForTesting();
    backend_ = base::MakeUnique<SyncBackendHostImpl>(
        "dummyDebugName", &sync_client_, nullptr, sync_prefs_->AsWeakPtr(),
        temp_dir_.GetPath().Append(base::FilePath(kTestSyncDir)));
    credentials_.account_id = "user@example.com";
    credentials_.email = "user@example.com";
    credentials_.sync_token = "sync_token";
    credentials_.scope_set.insert(GaiaConstants::kChromeSyncOAuth2Scope);

    fake_manager_factory_ =
        base::MakeUnique<FakeSyncManagerFactory>(&fake_manager_);

    // These types are always implicitly enabled.
    enabled_types_.PutAll(ControlTypes());

    // NOTE: We can't include Passwords or Typed URLs due to the Sync Backend
    // Registrar removing them if it can't find their model workers.
    enabled_types_.Put(BOOKMARKS);
    enabled_types_.Put(PREFERENCES);
    enabled_types_.Put(SESSIONS);
    enabled_types_.Put(SEARCH_ENGINES);
    enabled_types_.Put(AUTOFILL);

    network_resources_ = base::MakeUnique<HttpBridgeNetworkResources>();
  }

  void TearDown() override {
    if (backend_) {
      backend_->StopSyncingForShutdown();
      backend_->Shutdown(STOP_SYNC);
    }
    backend_.reset();
    sync_prefs_.reset();
    // Pump messages posted by the sync thread.
    base::RunLoop().RunUntilIdle();
  }

  // Synchronously initializes the backend.
  void InitializeBackend(bool expect_success) {
    EXPECT_CALL(mock_frontend_, OnBackendInitialized(_, _, _, expect_success))
        .WillOnce(InvokeWithoutArgs(QuitMessageLoop));
    SyncBackendHost::HttpPostProviderFactoryGetter
        http_post_provider_factory_getter =
            base::Bind(&NetworkResources::GetHttpPostProviderFactory,
                       base::Unretained(network_resources_.get()), nullptr,
                       base::Bind(&EmptyNetworkTimeUpdate));
    backend_->Initialize(
        &mock_frontend_, sync_thread_.task_runner(),
        WeakHandle<JsEventHandler>(), GURL(std::string()), std::string(),
        credentials_, true, false, base::FilePath(),
        std::move(fake_manager_factory_),
        MakeWeakHandle(test_unrecoverable_error_handler_.GetWeakPtr()),
        base::Closure(), http_post_provider_factory_getter,
        std::move(saved_nigori_state_));
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
    // |fake_manager_factory_|'s fake_manager() is set on the sync
    // thread, but we can rely on the message loop barriers to
    // guarantee that we see the updated value.
    DCHECK(fake_manager_);
  }

  // Synchronously configures the backend's datatypes.
  ModelTypeSet ConfigureDataTypes(ModelTypeSet types_to_add,
                                  ModelTypeSet types_to_remove,
                                  ModelTypeSet types_to_unapply) {
    BackendDataTypeConfigurer::DataTypeConfigStateMap config_state_map;
    BackendDataTypeConfigurer::SetDataTypesState(
        BackendDataTypeConfigurer::CONFIGURE_ACTIVE, types_to_add,
        &config_state_map);
    BackendDataTypeConfigurer::SetDataTypesState(
        BackendDataTypeConfigurer::DISABLED, types_to_remove,
        &config_state_map);
    BackendDataTypeConfigurer::SetDataTypesState(
        BackendDataTypeConfigurer::UNREADY, types_to_unapply,
        &config_state_map);

    types_to_add.PutAll(ControlTypes());
    ModelTypeSet ready_types = backend_->ConfigureDataTypes(
        CONFIGURE_REASON_RECONFIGURATION, config_state_map,
        base::Bind(&SyncBackendHostTest::DownloadReady, base::Unretained(this)),
        base::Bind(&SyncBackendHostTest::OnDownloadRetry,
                   base::Unretained(this)));
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
    return ready_types;
  }

 protected:
  void DownloadReady(ModelTypeSet succeeded_types, ModelTypeSet failed_types) {
    base::MessageLoop::current()->QuitWhenIdle();
  }

  void OnDownloadRetry() { NOTIMPLEMENTED(); }

  base::MessageLoop message_loop_;
  base::ScopedTempDir temp_dir_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  base::Thread sync_thread_;
  StrictMock<MockSyncFrontend> mock_frontend_;
  SyncCredentials credentials_;
  BackendSyncClient sync_client_;
  TestUnrecoverableErrorHandler test_unrecoverable_error_handler_;
  std::unique_ptr<SyncPrefs> sync_prefs_;
  std::unique_ptr<SyncBackendHostImpl> backend_;
  std::unique_ptr<FakeSyncManagerFactory> fake_manager_factory_;
  FakeSyncManager* fake_manager_;
  ModelTypeSet enabled_types_;
  std::unique_ptr<NetworkResources> network_resources_;
  std::unique_ptr<SyncEncryptionHandler::NigoriState> saved_nigori_state_;
};

// Test basic initialization with no initial types (first time initialization).
// Only the nigori should be configured.
TEST_F(SyncBackendHostTest, InitShutdown) {
  InitializeBackend(true);
  EXPECT_EQ(ControlTypes(), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(ControlTypes(), fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(ControlTypes())
          .Empty());
}

// Test first time sync scenario. All types should be properly configured.

TEST_F(SyncBackendHostTest, FirstTimeSync) {
  InitializeBackend(true);
  EXPECT_EQ(ControlTypes(), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(ControlTypes(), fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(ControlTypes())
          .Empty());

  ModelTypeSet ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().HasAll(
      Difference(enabled_types_, ControlTypes())));
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetEnabledTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// Test the restart after setting up sync scenario. No enabled types should be
// downloaded or cleaned.
TEST_F(SyncBackendHostTest, Restart) {
  sync_prefs_->SetFirstSetupComplete();
  ModelTypeSet all_but_nigori = enabled_types_;
  fake_manager_factory_->set_progress_marker_types(enabled_types_);
  fake_manager_factory_->set_initial_sync_ended_types(enabled_types_);
  InitializeBackend(true);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetCleanedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());

  ModelTypeSet ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());
  EXPECT_EQ(enabled_types_, ready_types);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetCleanedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetEnabledTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// Test a sync restart scenario where some types had never finished configuring.
// The partial types should be purged, then reconfigured properly.
TEST_F(SyncBackendHostTest, PartialTypes) {
  sync_prefs_->SetFirstSetupComplete();
  // Set sync manager behavior before passing it down. All types have progress
  // markers, but nigori and bookmarks are missing initial sync ended.
  ModelTypeSet partial_types(NIGORI, BOOKMARKS);
  ModelTypeSet full_types = Difference(enabled_types_, partial_types);
  fake_manager_factory_->set_progress_marker_types(enabled_types_);
  fake_manager_factory_->set_initial_sync_ended_types(full_types);

  // Bringing up the backend should purge all partial types, then proceed to
  // download the Nigori.
  InitializeBackend(true);
  EXPECT_EQ(ModelTypeSet(NIGORI), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(fake_manager_->GetAndResetCleanedTypes().HasAll(partial_types));
  EXPECT_EQ(Union(full_types, ModelTypeSet(NIGORI)),
            fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(
      Difference(partial_types, ModelTypeSet(NIGORI)),
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_));

  // Now do the actual configuration, which should download and apply bookmarks.
  ModelTypeSet ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());
  EXPECT_EQ(full_types, ready_types);
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetCleanedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(partial_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetEnabledTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// Test the behavior when we lose the sync db. Although we already have types
// enabled, we should re-download all of them because we lost their data.
TEST_F(SyncBackendHostTest, LostDB) {
  sync_prefs_->SetFirstSetupComplete();
  // Initialization should fetch the Nigori node.  Everything else should be
  // left untouched.
  InitializeBackend(true);
  EXPECT_EQ(ModelTypeSet(ControlTypes()),
            fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(ModelTypeSet(ControlTypes()),
            fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(
      Difference(enabled_types_, ControlTypes()),
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_));

  // The database was empty, so any cleaning is entirely optional.  We want to
  // reset this value before running the next part of the test, though.
  fake_manager_->GetAndResetCleanedTypes();

  // The actual configuration should redownload and apply all the enabled types.
  ModelTypeSet ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().HasAll(
      Difference(enabled_types_, ControlTypes())));
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetCleanedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetEnabledTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

TEST_F(SyncBackendHostTest, DisableTypes) {
  // Simulate first time sync.
  InitializeBackend(true);
  fake_manager_->GetAndResetCleanedTypes();
  ModelTypeSet ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetCleanedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());

  // Then disable two datatypes.
  ModelTypeSet disabled_types(BOOKMARKS, SEARCH_ENGINES);
  ModelTypeSet old_types = enabled_types_;
  enabled_types_.RemoveAll(disabled_types);
  ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());

  // Only those datatypes disabled should be cleaned. Nothing should be
  // downloaded.
  EXPECT_EQ(enabled_types_, ready_types);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_EQ(disabled_types,
            Intersection(fake_manager_->GetAndResetCleanedTypes(), old_types));
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetEnabledTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

TEST_F(SyncBackendHostTest, AddTypes) {
  // Simulate first time sync.
  InitializeBackend(true);
  fake_manager_->GetAndResetCleanedTypes();
  ModelTypeSet ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetCleanedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());

  // Then add two datatypes.
  ModelTypeSet new_types(EXTENSIONS, APPS);
  enabled_types_.PutAll(new_types);
  ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());

  // Only those datatypes added should be downloaded (plus nigori). Nothing
  // should be cleaned aside from the disabled types.
  new_types.Put(NIGORI);
  EXPECT_EQ(Difference(enabled_types_, new_types), ready_types);
  EXPECT_EQ(new_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetCleanedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetEnabledTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// And and disable in the same configuration.
TEST_F(SyncBackendHostTest, AddDisableTypes) {
  // Simulate first time sync.
  InitializeBackend(true);
  fake_manager_->GetAndResetCleanedTypes();
  ModelTypeSet ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());
  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetCleanedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());

  // Then add two datatypes.
  ModelTypeSet old_types = enabled_types_;
  ModelTypeSet disabled_types(BOOKMARKS, SEARCH_ENGINES);
  ModelTypeSet new_types(EXTENSIONS, APPS);
  enabled_types_.PutAll(new_types);
  enabled_types_.RemoveAll(disabled_types);
  ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());

  // Only those datatypes added should be downloaded (plus nigori). Nothing
  // should be cleaned aside from the disabled types.
  new_types.Put(NIGORI);
  EXPECT_EQ(Difference(enabled_types_, new_types), ready_types);
  EXPECT_EQ(new_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(disabled_types,
            Intersection(fake_manager_->GetAndResetCleanedTypes(), old_types));
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetEnabledTypes());
  EXPECT_EQ(disabled_types,
            fake_manager_->GetTypesWithEmptyProgressMarkerToken(old_types));
}

// Test restarting the browser to newly supported datatypes. The new datatypes
// should be downloaded on the configuration after backend initialization.
TEST_F(SyncBackendHostTest, NewlySupportedTypes) {
  sync_prefs_->SetFirstSetupComplete();
  // Set sync manager behavior before passing it down. All types have progress
  // markers and initial sync ended except the new types.
  ModelTypeSet old_types = enabled_types_;
  fake_manager_factory_->set_progress_marker_types(old_types);
  fake_manager_factory_->set_initial_sync_ended_types(old_types);
  ModelTypeSet new_types(APP_SETTINGS, EXTENSION_SETTINGS);
  enabled_types_.PutAll(new_types);

  // Does nothing.
  InitializeBackend(true);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(), old_types)
                  .Empty());
  EXPECT_EQ(old_types, fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(new_types, fake_manager_->GetTypesWithEmptyProgressMarkerToken(
                           enabled_types_));

  // Downloads and applies the new types (plus nigori).
  ModelTypeSet ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());

  new_types.Put(NIGORI);
  EXPECT_EQ(Difference(old_types, ModelTypeSet(NIGORI)), ready_types);
  EXPECT_EQ(new_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetCleanedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetEnabledTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// Test the newly supported types scenario, but with the presence of partial
// types as well. Both partial and newly supported types should be downloaded
// the configuration.
TEST_F(SyncBackendHostTest, NewlySupportedTypesWithPartialTypes) {
  sync_prefs_->SetFirstSetupComplete();
  // Set sync manager behavior before passing it down. All types have progress
  // markers and initial sync ended except the new types.
  ModelTypeSet old_types = enabled_types_;
  ModelTypeSet partial_types(NIGORI, BOOKMARKS);
  ModelTypeSet full_types = Difference(enabled_types_, partial_types);
  fake_manager_factory_->set_progress_marker_types(old_types);
  fake_manager_factory_->set_initial_sync_ended_types(full_types);
  ModelTypeSet new_types(APP_SETTINGS, EXTENSION_SETTINGS);
  enabled_types_.PutAll(new_types);

  // Purge the partial types.  The nigori will be among the purged types, but
  // the syncer will re-download it by the time the initialization is complete.
  InitializeBackend(true);
  EXPECT_EQ(ModelTypeSet(NIGORI), fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(fake_manager_->GetAndResetCleanedTypes().HasAll(partial_types));
  EXPECT_EQ(Union(full_types, ModelTypeSet(NIGORI)),
            fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(
      Union(new_types, Difference(partial_types, ModelTypeSet(NIGORI))),
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_));

  // Downloads and applies the new types and partial types (which includes
  // nigori anyways).
  ModelTypeSet ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());
  EXPECT_EQ(full_types, ready_types);
  EXPECT_EQ(Union(new_types, partial_types),
            fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_TRUE(
      Intersection(fake_manager_->GetAndResetCleanedTypes(), enabled_types_)
          .Empty());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->GetAndResetEnabledTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// Verify that downloading control types only downloads those types that do
// not have initial sync ended set.
TEST_F(SyncBackendHostTest, DownloadControlTypes) {
  sync_prefs_->SetFirstSetupComplete();
  // Set sync manager behavior before passing it down. Experiments and device
  // info are new types without progress markers or initial sync ended, while
  // all other types have been fully downloaded and applied.
  ModelTypeSet new_types(EXPERIMENTS, NIGORI);
  ModelTypeSet old_types = Difference(enabled_types_, new_types);
  fake_manager_factory_->set_progress_marker_types(old_types);
  fake_manager_factory_->set_initial_sync_ended_types(old_types);

  // Bringing up the backend should download the new types without downloading
  // any old types.
  InitializeBackend(true);
  EXPECT_EQ(new_types, fake_manager_->GetAndResetDownloadedTypes());
  EXPECT_EQ(Difference(ModelTypeSet::All(), enabled_types_),
            fake_manager_->GetAndResetCleanedTypes());
  EXPECT_EQ(enabled_types_, fake_manager_->InitialSyncEndedTypes());
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(enabled_types_)
          .Empty());
}

// Fail to download control types.  It's believed that there is a server bug
// which can allow this to happen (crbug.com/164288).  The sync backend host
// should detect this condition and fail to initialize the backend.
//
// The failure is "silent" in the sense that the GetUpdates request appears to
// be successful, but it returned no results.  This means that the usual
// download retry logic will not be invoked.
TEST_F(SyncBackendHostTest, SilentlyFailToDownloadControlTypes) {
  fake_manager_factory_->set_configure_fail_types(ModelTypeSet::All());
  InitializeBackend(false);
}

// Test that local refresh requests are delivered to sync.
TEST_F(SyncBackendHostTest, ForwardLocalRefreshRequest) {
  InitializeBackend(true);

  ModelTypeSet set1 = ModelTypeSet::All();
  backend_->TriggerRefresh(set1);
  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(set1, fake_manager_->GetLastRefreshRequestTypes());

  ModelTypeSet set2 = ModelTypeSet(SESSIONS);
  backend_->TriggerRefresh(set2);
  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(set2, fake_manager_->GetLastRefreshRequestTypes());
}

// Test that configuration on signin sends the proper GU source.
TEST_F(SyncBackendHostTest, DownloadControlTypesNewClient) {
  InitializeBackend(true);
  EXPECT_EQ(CONFIGURE_REASON_NEW_CLIENT,
            fake_manager_->GetAndResetConfigureReason());
}

// Test that configuration on restart sends the proper GU source.
TEST_F(SyncBackendHostTest, DownloadControlTypesRestart) {
  sync_prefs_->SetFirstSetupComplete();
  fake_manager_factory_->set_progress_marker_types(enabled_types_);
  fake_manager_factory_->set_initial_sync_ended_types(enabled_types_);
  InitializeBackend(true);
  EXPECT_EQ(CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE,
            fake_manager_->GetAndResetConfigureReason());
}

// It is SyncBackendHostCore responsibility to cleanup Sync Data folder if sync
// setup hasn't been completed. This test ensures that cleanup happens.
TEST_F(SyncBackendHostTest, TestStartupWithOldSyncData) {
  const char* nonsense = "slon";
  base::FilePath temp_directory =
      temp_dir_.GetPath().Append(base::FilePath(kTestSyncDir));
  base::FilePath sync_file = temp_directory.AppendASCII("SyncData.sqlite3");
  ASSERT_TRUE(base::CreateDirectory(temp_directory));
  ASSERT_NE(-1, base::WriteFile(sync_file, nonsense, strlen(nonsense)));

  InitializeBackend(true);

  EXPECT_FALSE(base::PathExists(sync_file));
}

// If bookmarks encounter an error that results in disabling without purging
// (such as when the type is unready), and then is explicitly disabled, the
// SyncBackendHost needs to tell the manager to purge the type, even though
// it's already disabled (crbug.com/386778).
TEST_F(SyncBackendHostTest, DisableThenPurgeType) {
  ModelTypeSet error_types(BOOKMARKS);

  InitializeBackend(true);

  // First enable the types.
  ModelTypeSet ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());

  // Nigori is always downloaded so won't be ready.
  EXPECT_EQ(Difference(ControlTypes(), ModelTypeSet(NIGORI)), ready_types);

  // Then mark the error types as unready (disables without purging).
  ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      error_types);
  EXPECT_EQ(Difference(enabled_types_, error_types), ready_types);
  EXPECT_TRUE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(error_types).Empty());

  // Lastly explicitly disable the error types, which should result in a purge.
  enabled_types_.RemoveAll(error_types);
  ready_types = ConfigureDataTypes(
      enabled_types_, Difference(ModelTypeSet::All(), enabled_types_),
      ModelTypeSet());
  EXPECT_EQ(Difference(enabled_types_, error_types), ready_types);
  EXPECT_FALSE(
      fake_manager_->GetTypesWithEmptyProgressMarkerToken(error_types).Empty());
}

// Test that a call to ClearServerData is forwarded to the underlying
// SyncManager.
TEST_F(SyncBackendHostTest, ClearServerDataCallsAreForwarded) {
  InitializeBackend(true);
  CallbackCounter callback_counter;
  backend_->ClearServerData(base::Bind(&CallbackCounter::Callback,
                                       base::Unretained(&callback_counter)));
  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(1, callback_counter.times_called());
}

// Ensure that redundant invalidations are ignored and that the most recent
// set of invalidation version is persisted across restarts.
TEST_F(SyncBackendHostTest, IgnoreOldInvalidations) {
  // Set up some old persisted invalidations.
  std::map<ModelType, int64_t> invalidation_versions;
  invalidation_versions[BOOKMARKS] = 20;
  sync_prefs_->UpdateInvalidationVersions(invalidation_versions);
  InitializeBackend(true);
  EXPECT_EQ(0, fake_manager_->GetInvalidationCount());

  // Receiving an invalidation with an old version should do nothing.
  ObjectIdInvalidationMap invalidation_map;
  std::string notification_type;
  RealModelTypeToNotificationType(BOOKMARKS, &notification_type);
  invalidation_map.Insert(Invalidation::Init(
      invalidation::ObjectId(0, notification_type), 10, "payload"));
  backend_->OnIncomingInvalidation(invalidation_map);
  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(0, fake_manager_->GetInvalidationCount());

  // Invalidations with new versions should be acted upon.
  invalidation_map.Insert(Invalidation::Init(
      invalidation::ObjectId(0, notification_type), 30, "payload"));
  backend_->OnIncomingInvalidation(invalidation_map);
  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(1, fake_manager_->GetInvalidationCount());

  // Invalidation for new data types should be acted on.
  RealModelTypeToNotificationType(SESSIONS, &notification_type);
  invalidation_map.Insert(Invalidation::Init(
      invalidation::ObjectId(0, notification_type), 10, "payload"));
  backend_->OnIncomingInvalidation(invalidation_map);
  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(2, fake_manager_->GetInvalidationCount());

  // But redelivering that same invalidation should be ignored.
  backend_->OnIncomingInvalidation(invalidation_map);
  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(2, fake_manager_->GetInvalidationCount());

  // If an invalidation with an unknown version is received, it should be
  // acted on, but should not affect the persisted versions.
  invalidation_map.Insert(Invalidation::InitUnknownVersion(
      invalidation::ObjectId(0, notification_type)));
  backend_->OnIncomingInvalidation(invalidation_map);
  fake_manager_->WaitForSyncThread();
  EXPECT_EQ(3, fake_manager_->GetInvalidationCount());

  // Verify that the invalidation versions were updated in the prefs.
  invalidation_versions[BOOKMARKS] = 30;
  invalidation_versions[SESSIONS] = 10;
  std::map<ModelType, int64_t> persisted_invalidation_versions;
  sync_prefs_->GetInvalidationVersions(&persisted_invalidation_versions);
  EXPECT_EQ(invalidation_versions.size(),
            persisted_invalidation_versions.size());
  for (auto iter : persisted_invalidation_versions) {
    EXPECT_EQ(invalidation_versions[iter.first], iter.second);
  }
}

// Tests that SyncBackendHostImpl retains ModelTypeConnector after call to
// StopSyncingForShutdown. This is needed for datatype deactivation during
// DataTypeManager shutdown.
TEST_F(SyncBackendHostTest, ModelTypeConnectorValidDuringShutdown) {
  InitializeBackend(true);
  backend_->StopSyncingForShutdown();
  // Verify that call to DeactivateNonBlockingDataType doesn't assert.
  backend_->DeactivateNonBlockingDataType(AUTOFILL);
  backend_->Shutdown(STOP_SYNC);
  backend_.reset();
}

}  // namespace

}  // namespace syncer
