// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_host.h"

#include <cstddef>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/invalidations/invalidator_storage.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/util/experiments.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/protocol/sync_protocol_error.h"
#include "sync/util/test_unrecoverable_error_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace browser_sync {

namespace {

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
               void(syncable::ModelTypeSet, bool));
  MOCK_METHOD0(OnEncryptionComplete, void());
  MOCK_METHOD1(OnMigrationNeededForTypes, void(syncable::ModelTypeSet));
  MOCK_METHOD1(OnExperimentsChanged,
      void(const syncer::Experiments&));
  MOCK_METHOD1(OnActionableError,
      void(const syncer::SyncProtocolError& sync_error));
  MOCK_METHOD0(OnSyncConfigureRetry, void());
};

}  // namespace

class SyncBackendHostTest : public testing::Test {
 protected:
  SyncBackendHostTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        io_thread_(BrowserThread::IO) {}

  virtual ~SyncBackendHostTest() {}

  virtual void SetUp() {
    io_thread_.StartIOThread();
  }

  virtual void TearDown() {
    // Pump messages posted by the sync core thread (which may end up
    // posting on the IO thread).
    ui_loop_.RunAllPending();
    io_thread_.Stop();
    // Pump any messages posted by the IO thread.
    ui_loop_.RunAllPending();
  }

 private:
  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
};

TEST_F(SyncBackendHostTest, InitShutdown) {
  std::string k_mock_url = "http://www.example.com";
  net::FakeURLFetcherFactory test_factory_;
  test_factory_.SetFakeResponse(k_mock_url + "/time?command=get_time", "",
      false);

  TestingProfile profile;
  profile.CreateRequestContext();

  SyncPrefs sync_prefs(profile.GetPrefs());
  InvalidatorStorage invalidator_storage(profile.GetPrefs());
  SyncBackendHost backend(profile.GetDebugName(),
                          &profile, sync_prefs.AsWeakPtr(),
                          invalidator_storage.AsWeakPtr());

  MockSyncFrontend mock_frontend;
  syncer::SyncCredentials credentials;
  credentials.email = "user@example.com";
  credentials.sync_token = "sync_token";
  syncer::TestUnrecoverableErrorHandler handler;
  backend.Initialize(&mock_frontend,
                     syncer::WeakHandle<syncer::JsEventHandler>(),
                     GURL(k_mock_url),
                     syncable::ModelTypeSet(),
                     credentials,
                     true,
                     &handler,
                     NULL);
  backend.StopSyncingForShutdown();
  backend.Shutdown(false);
}

// TODO(akalin): Write more SyncBackendHost unit tests.

}  // namespace browser_sync
