// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_host.h"

#include <cstddef>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/sync/invalidations/invalidator_storage.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/sync_manager_factory.h"
#include "sync/internal_api/public/test/fake_sync_manager.h"
#include "sync/internal_api/public/util/experiments.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/protocol/sync_protocol_error.h"
#include "sync/util/test_unrecoverable_error_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using syncer::FakeSyncManager;
using syncer::SyncManager;
using ::testing::InvokeWithoutArgs;
using ::testing::_;

namespace browser_sync {

namespace {

ACTION_P(Signal, event) {
  event->Signal();
}

void SignalEvent(base::WaitableEvent* event) {
  event->Signal();
}

static void QuitMessageLoop() {
  MessageLoop::current()->Quit();
}

class MockSyncFrontend : public SyncFrontend {
 public:
  virtual ~MockSyncFrontend() {}

  MOCK_METHOD2(OnBackendInitialized,
               void(const syncer::WeakHandle<syncer::JsBackend>&, bool));
  MOCK_METHOD0(OnSyncCycleCompleted, void());
  MOCK_METHOD1(OnConnectionStatusChange,
               void(syncer::ConnectionStatus status));
  MOCK_METHOD0(OnStopSyncingPermanently, void());
  MOCK_METHOD0(OnClearServerDataSucceeded, void());
  MOCK_METHOD0(OnClearServerDataFailed, void());
  MOCK_METHOD2(OnPassphraseRequired,
               void(syncer::PassphraseRequiredReason,
                    const sync_pb::EncryptedData&));
  MOCK_METHOD0(OnPassphraseAccepted, void());
  MOCK_METHOD2(OnEncryptedTypesChanged,
               void(syncer::ModelTypeSet, bool));
  MOCK_METHOD0(OnEncryptionComplete, void());
  MOCK_METHOD1(OnMigrationNeededForTypes, void(syncer::ModelTypeSet));
  MOCK_METHOD1(OnExperimentsChanged,
      void(const syncer::Experiments&));
  MOCK_METHOD1(OnActionableError,
      void(const syncer::SyncProtocolError& sync_error));
  MOCK_METHOD0(OnSyncConfigureRetry, void());
};

class FakeSyncManagerFactory : public syncer::SyncManagerFactory {
 public:
  FakeSyncManagerFactory() : manager_(NULL) {}
  virtual ~FakeSyncManagerFactory() {}

  // Takes ownership of |manager|.
  void SetSyncManager(FakeSyncManager* manager) {
    DCHECK(!manager_.get());
    manager_.reset(manager);
  }

  // Passes ownership of |manager_|.
  // SyncManagerFactory implementation.
  virtual scoped_ptr<SyncManager> CreateSyncManager(std::string name) OVERRIDE {
    DCHECK(manager_.get());
    return manager_.Pass();
  }

 private:
  scoped_ptr<SyncManager> manager_;
};

class SyncBackendHostTest : public testing::Test {
 protected:
  SyncBackendHostTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        io_thread_(BrowserThread::IO),
        fake_manager_(NULL) {}

  virtual ~SyncBackendHostTest() {}

  virtual void SetUp() OVERRIDE {
    io_thread_.StartIOThread();
    profile_.reset(new TestingProfile());
    profile_->CreateRequestContext();
    sync_prefs_.reset(new SyncPrefs(profile_->GetPrefs()));
    invalidator_storage_.reset(new InvalidatorStorage(profile_->GetPrefs()));
    backend_.reset(new SyncBackendHost(
        profile_->GetDebugName(),
        profile_.get(),
        sync_prefs_->AsWeakPtr(),
        invalidator_storage_->AsWeakPtr()));
    credentials_.email = "user@example.com";
    credentials_.sync_token = "sync_token";
    fake_manager_ = new FakeSyncManager();
    fake_sync_manager_factory_.SetSyncManager(fake_manager_);

    // NOTE: We can't include Passwords or Typed URLs due to the Sync Backend
    // Registrar removing them if it can't find their model workers.
    enabled_types_.Put(syncer::BOOKMARKS);
    enabled_types_.Put(syncer::NIGORI);
    enabled_types_.Put(syncer::PREFERENCES);
    enabled_types_.Put(syncer::SESSIONS);
    enabled_types_.Put(syncer::SEARCH_ENGINES);
    enabled_types_.Put(syncer::AUTOFILL);
  }

  virtual void TearDown() OVERRIDE {
    backend_->StopSyncingForShutdown();
    backend_->Shutdown(false);
    backend_.reset();
    sync_prefs_.reset();
    invalidator_storage_.reset();
    profile_.reset();
    // Pump messages posted by the sync core thread (which may end up
    // posting on the IO thread).
    ui_loop_.RunAllPending();
    io_thread_.Stop();
    // Pump any messages posted by the IO thread.
    ui_loop_.RunAllPending();
  }

  // Synchronously initializes the backend.
  void InitializeBackend(syncer::ModelTypeSet enabled_types) {
    EXPECT_CALL(mock_frontend_, OnBackendInitialized(_, true)).
        WillOnce(InvokeWithoutArgs(QuitMessageLoop));
    backend_->Initialize(&mock_frontend_,
                         syncer::WeakHandle<syncer::JsEventHandler>(),
                         GURL(""),
                         enabled_types,
                         credentials_,
                         true,
                         &fake_sync_manager_factory_,
                         &handler_,
                         NULL);
    ui_loop_.PostDelayedTask(
        FROM_HERE,
        ui_loop_.QuitClosure(),
        base::TimeDelta::FromMilliseconds(TestTimeouts::action_timeout_ms()));
    ui_loop_.Run();
  }

  // Synchronously configures the backend's datatypes.
  void ConfigureDataTypes(syncer::ModelTypeSet types_to_add,
                          syncer::ModelTypeSet types_to_remove,
                          BackendDataTypeConfigurer::NigoriState nigori_state) {
    backend_->ConfigureDataTypes(
        syncer::CONFIGURE_REASON_RECONFIGURATION,
        types_to_add,
        types_to_remove,
        nigori_state,
        base::Bind(&SyncBackendHostTest::DownloadReady,
                   base::Unretained(this)),
        base::Bind(&SyncBackendHostTest::OnDownloadRetry,
                   base::Unretained(this)));
    ui_loop_.PostDelayedTask(
        FROM_HERE,
        ui_loop_.QuitClosure(),
        base::TimeDelta::FromMilliseconds(TestTimeouts::action_timeout_ms()));
    ui_loop_.Run();
  }

 protected:
  void DownloadReady(syncer::ModelTypeSet types) {
    MessageLoop::current()->Quit();
  }

  void OnDownloadRetry() {
    NOTIMPLEMENTED();
  }

  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  MockSyncFrontend mock_frontend_;
  syncer::SyncCredentials credentials_;
  syncer::TestUnrecoverableErrorHandler handler_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<SyncPrefs> sync_prefs_;
  scoped_ptr<InvalidatorStorage> invalidator_storage_;
  scoped_ptr<SyncBackendHost> backend_;
  FakeSyncManagerFactory fake_sync_manager_factory_;
  FakeSyncManager* fake_manager_;
  syncer::ModelTypeSet enabled_types_;
};

// Test basic initialization with no initial types (first time initialization).
// Only the nigori should be configured.
TEST_F(SyncBackendHostTest, InitShutdown) {
  InitializeBackend(syncer::ModelTypeSet());
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Equals(
      syncer::ModelTypeSet(syncer::NIGORI)));
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(
      syncer::ModelTypeSet(syncer::NIGORI)));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      syncer::ModelTypeSet(syncer::NIGORI)).Empty());
}

// Test first time sync scenario. All types should be properly configured.
TEST_F(SyncBackendHostTest, FirstTimeSync) {
  InitializeBackend(syncer::ModelTypeSet());
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Equals(
      syncer::ModelTypeSet(syncer::NIGORI)));
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(
      syncer::ModelTypeSet(syncer::NIGORI)));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      syncer::ModelTypeSet(syncer::NIGORI)).Empty());

  ConfigureDataTypes(enabled_types_,
                     Difference(syncer::ModelTypeSet::All(),
                                enabled_types_),
                     BackendDataTypeConfigurer::WITH_NIGORI);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().HasAll(
      enabled_types_));
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(enabled_types_));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Empty());
}

// Test the restart after setting up sync scenario. No enabled types should be
// downloaded or cleaned.
TEST_F(SyncBackendHostTest, Restart) {
  sync_prefs_->SetSyncSetupCompleted();
  syncer::ModelTypeSet all_but_nigori = enabled_types_;
  fake_manager_->set_progress_marker_types(
      enabled_types_);
  fake_manager_->set_initial_sync_ended_types(enabled_types_);
  InitializeBackend(enabled_types_);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(enabled_types_));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Empty());

  ConfigureDataTypes(enabled_types_,
                     Difference(syncer::ModelTypeSet::All(),
                                enabled_types_),
                     BackendDataTypeConfigurer::WITH_NIGORI);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(enabled_types_));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Empty());
}

// Test a sync restart scenario where some types had never finished configuring.
// The partial types should be purged, then reconfigured properly.
TEST_F(SyncBackendHostTest, PartialTypes) {
  sync_prefs_->SetSyncSetupCompleted();
  // Set sync manager behavior before passing it down. All types have progress
  // markers, but nigori and bookmarks are missing initial sync ended.
  syncer::ModelTypeSet partial_types(syncer::NIGORI, syncer::BOOKMARKS);
  syncer::ModelTypeSet full_types =
      Difference(enabled_types_, partial_types);
  fake_manager_->set_progress_marker_types(enabled_types_);
  fake_manager_->set_initial_sync_ended_types(full_types);

  // All partial types should have been purged with nothing downloaded as part
  // of bringing up the backend.
  InitializeBackend(enabled_types_);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(
      full_types));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Equals(partial_types));

  // Now do the actual configuration, which should download and apply both
  // nigori and bookmarks.
  ConfigureDataTypes(enabled_types_,
                     Difference(syncer::ModelTypeSet::All(),
                                enabled_types_),
                     BackendDataTypeConfigurer::WITH_NIGORI);
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Equals(
      partial_types));
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(enabled_types_));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Empty());
}

// Test the behavior when we lose the sync db. Although we already have types
// enabled, we should re-download all of them because we lost their data.
TEST_F(SyncBackendHostTest, LostDB) {
  sync_prefs_->SetSyncSetupCompleted();
  // Don't set any progress marker or initial_sync_ended types before
  // initializing. Initialization should not affect the datatypes.
  InitializeBackend(enabled_types_);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Empty());
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Equals(enabled_types_));

  // The actual configuration should redownload and apply all the enabled types.
  ConfigureDataTypes(enabled_types_,
                     Difference(syncer::ModelTypeSet::All(),
                                enabled_types_),
                     BackendDataTypeConfigurer::WITH_NIGORI);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().HasAll(
      enabled_types_));
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(enabled_types_));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Empty());
}

TEST_F(SyncBackendHostTest, DisableTypes) {
  // Simulate first time sync.
  InitializeBackend(syncer::ModelTypeSet());
  fake_manager_->GetAndResetCleanedTypes();
  ConfigureDataTypes(enabled_types_,
                     Difference(syncer::ModelTypeSet::All(),
                                enabled_types_),
                     BackendDataTypeConfigurer::WITH_NIGORI);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Equals(
      enabled_types_));
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(enabled_types_));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Empty());

  // Then disable two datatypes.
  syncer::ModelTypeSet disabled_types(syncer::BOOKMARKS,
                                      syncer::SEARCH_ENGINES);
  syncer::ModelTypeSet old_types = enabled_types_;
  enabled_types_.RemoveAll(disabled_types);
  ConfigureDataTypes(enabled_types_,
                     Difference(syncer::ModelTypeSet::All(),
                                enabled_types_),
                     BackendDataTypeConfigurer::WITH_NIGORI);

  // Only those datatypes disabled should be cleaned. Nothing should be
  // downloaded.
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           old_types).Equals(disabled_types));
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(enabled_types_));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Empty());
}

TEST_F(SyncBackendHostTest, AddTypes) {
  // Simulate first time sync.
  InitializeBackend(syncer::ModelTypeSet());
  fake_manager_->GetAndResetCleanedTypes();
  ConfigureDataTypes(enabled_types_,
                     Difference(syncer::ModelTypeSet::All(),
                                enabled_types_),
                     BackendDataTypeConfigurer::WITH_NIGORI);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Equals(
      enabled_types_));
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(enabled_types_));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Empty());

  // Then add two datatypes.
  syncer::ModelTypeSet new_types(syncer::EXTENSIONS,
                                 syncer::APPS);
  enabled_types_.PutAll(new_types);
  ConfigureDataTypes(enabled_types_,
                     Difference(syncer::ModelTypeSet::All(),
                                enabled_types_),
                     BackendDataTypeConfigurer::WITH_NIGORI);

  // Only those datatypes added should be downloaded (plus nigori). Nothing
  // should be cleaned aside from the disabled types.
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Equals(
      Union(new_types, syncer::ModelTypeSet(syncer::NIGORI))));
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(enabled_types_));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Empty());
}

// And and disable in the same configuration.
TEST_F(SyncBackendHostTest, AddDisableTypes) {
  // Simulate first time sync.
  InitializeBackend(syncer::ModelTypeSet());
  fake_manager_->GetAndResetCleanedTypes();
  ConfigureDataTypes(enabled_types_,
                     Difference(syncer::ModelTypeSet::All(),
                                enabled_types_),
                     BackendDataTypeConfigurer::WITH_NIGORI);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Equals(
      enabled_types_));
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(enabled_types_));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Empty());

  // Then add two datatypes.
  syncer::ModelTypeSet old_types = enabled_types_;
  syncer::ModelTypeSet disabled_types(syncer::BOOKMARKS,
                                      syncer::SEARCH_ENGINES);
  syncer::ModelTypeSet new_types(syncer::EXTENSIONS,
                                 syncer::APPS);
  enabled_types_.PutAll(new_types);
  enabled_types_.RemoveAll(disabled_types);
  ConfigureDataTypes(enabled_types_,
                     Difference(syncer::ModelTypeSet::All(),
                                enabled_types_),
                     BackendDataTypeConfigurer::WITH_NIGORI);

  // Only those datatypes added should be downloaded (plus nigori). Nothing
  // should be cleaned aside from the disabled types.
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Equals(
      Union(new_types, syncer::ModelTypeSet(syncer::NIGORI))));
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           old_types).Equals(disabled_types));
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(enabled_types_));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      old_types).Equals(disabled_types));
}

// Test restarting the browser to newly supported datatypes. The new datatypes
// should be downloaded on the configuration after backend initialization.
TEST_F(SyncBackendHostTest, NewlySupportedTypes) {
  sync_prefs_->SetSyncSetupCompleted();
  // Set sync manager behavior before passing it down. All types have progress
  // markers and initial sync ended except the new types.
  syncer::ModelTypeSet old_types = enabled_types_;
  fake_manager_->set_progress_marker_types(old_types);
  fake_manager_->set_initial_sync_ended_types(old_types);
  syncer::ModelTypeSet new_types(syncer::APP_SETTINGS,
                                 syncer::EXTENSION_SETTINGS);
  enabled_types_.PutAll(new_types);

  // Does nothing.
  InitializeBackend(enabled_types_);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(old_types));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Equals(new_types));

  // Downloads and applies the new types.
  ConfigureDataTypes(enabled_types_,
                     Difference(syncer::ModelTypeSet::All(),
                                enabled_types_),
                     BackendDataTypeConfigurer::WITH_NIGORI);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Equals(
      Union(new_types, syncer::ModelTypeSet(syncer::NIGORI))));
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(enabled_types_));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Empty());
}

// Test the newly supported types scenario, but with the presence of partial
// types as well. Both partial and newly supported types should be downloaded
// the configuration.
TEST_F(SyncBackendHostTest, NewlySupportedTypesWithPartialTypes) {
  sync_prefs_->SetSyncSetupCompleted();
  // Set sync manager behavior before passing it down. All types have progress
  // markers and initial sync ended except the new types.
  syncer::ModelTypeSet old_types = enabled_types_;
  syncer::ModelTypeSet partial_types(syncer::NIGORI, syncer::BOOKMARKS);
  syncer::ModelTypeSet full_types =
      Difference(enabled_types_, partial_types);
  fake_manager_->set_progress_marker_types(old_types);
  fake_manager_->set_initial_sync_ended_types(full_types);
  syncer::ModelTypeSet new_types(syncer::APP_SETTINGS,
                                 syncer::EXTENSION_SETTINGS);
  enabled_types_.PutAll(new_types);

  // Purge the partial types.
  InitializeBackend(enabled_types_);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Empty());
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(full_types));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Equals(Union(new_types, partial_types)));

  // Downloads and applies the new types and partial types (which includes
  // nigori anyways).
  ConfigureDataTypes(enabled_types_,
                     Difference(syncer::ModelTypeSet::All(),
                                enabled_types_),
                     BackendDataTypeConfigurer::WITH_NIGORI);
  EXPECT_TRUE(fake_manager_->GetAndResetDownloadedTypes().Equals(
      Union(new_types, partial_types)));
  EXPECT_TRUE(Intersection(fake_manager_->GetAndResetCleanedTypes(),
                           enabled_types_).Empty());
  EXPECT_TRUE(fake_manager_->InitialSyncEndedTypes().Equals(enabled_types_));
  EXPECT_TRUE(fake_manager_->GetTypesWithEmptyProgressMarkerToken(
      enabled_types_).Empty());
}

}  // namespace

}  // namespace browser_sync
