// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_

#include <string>

#include "base/message_loop.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/net/fake_network_change_notifier_thread.h"
#include "chrome/test/sync/test_http_bridge_factory.h"

class TestProfileSyncService : public ProfileSyncService {
 public:
  explicit TestProfileSyncService(ProfileSyncFactory* factory,
                                  Profile* profile,
                                  bool bootstrap_sync_authentication,
                                  bool synchronous_backend_initialization)
      : ProfileSyncService(factory, profile,
                           &fake_network_change_notifier_thread_,
                           bootstrap_sync_authentication),
        synchronous_backend_initialization_(
            synchronous_backend_initialization) {
    RegisterPreferences();
    SetSyncSetupCompleted();
  }
  virtual ~TestProfileSyncService() {
  }

  virtual void InitializeBackend(bool delete_sync_data_folder) {
    browser_sync::TestHttpBridgeFactory* factory =
        new browser_sync::TestHttpBridgeFactory();
    browser_sync::TestHttpBridgeFactory* factory2 =
        new browser_sync::TestHttpBridgeFactory();
    backend()->InitializeForTestMode(
        L"testuser", &fake_network_change_notifier_thread_,
        factory, factory2, delete_sync_data_folder,
        browser_sync::kDefaultNotificationMethod);
    // TODO(akalin): Figure out a better way to do this.
    if (synchronous_backend_initialization_) {
      // The SyncBackend posts a task to the current loop when
      // initialization completes.
      MessageLoop::current()->Run();
      // Initialization is synchronous for test mode, so we should be
      // good to go.
      DCHECK(sync_initialized());
    }
  }

  virtual void OnBackendInitialized() {
    ProfileSyncService::OnBackendInitialized();
    // TODO(akalin): Figure out a better way to do this.
    if (synchronous_backend_initialization_) {
      MessageLoop::current()->Quit();
    }
  }

 private:
  // When testing under ChromiumOS, this method must not return an empty
  // value value in order for the profile sync service to start.
  virtual std::string GetLsidForAuthBootstraping() {
    return "foo";
  }

  bool synchronous_backend_initialization_;
  chrome_common_net::FakeNetworkChangeNotifierThread
      fake_network_change_notifier_thread_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
