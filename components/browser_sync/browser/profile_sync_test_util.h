// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_TEST_UTIL_H_
#define COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_TEST_UTIL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/time/time.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_driver/fake_sync_client.h"
#include "components/sync_driver/sync_api_component_factory_mock.h"
#include "components/sync_sessions/fake_sync_sessions_client.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"

namespace base {
class Time;
class TimeDelta;
}

namespace net {
class URLRequestContextGetter;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace browser_sync {

// An empty syncer::NetworkTimeUpdateCallback. Used in various tests to
// instantiate ProfileSyncService.
void EmptyNetworkTimeUpdate(const base::Time&,
                            const base::TimeDelta&,
                            const base::TimeDelta&);

// Call this to register preferences needed for ProfileSyncService creation.
void RegisterPrefsForProfileSyncService(
    user_prefs::PrefRegistrySyncable* registry);

// Aggregate this class to get all necessary support for creating a
// ProfileSyncService in tests.
class ProfileSyncServiceBundle {
 public:
#if defined(OS_CHROMEOS)
  typedef FakeSigninManagerBase FakeSigninManagerType;
#else
  typedef FakeSigninManager FakeSigninManagerType;
#endif

  ProfileSyncServiceBundle();
  ~ProfileSyncServiceBundle();

  // Builders

  // Builds a child of FakeSyncClient which overrides some of the client's
  // accessors to return objects from the bundle.
  class SyncClientBuilder {
   public:
    // Construct the builder and associate with the |bundle| to source objects
    // from.
    explicit SyncClientBuilder(ProfileSyncServiceBundle* bundle);

    ~SyncClientBuilder();

    // Set the clear browsing data callback for the client to return.
    void SetClearBrowsingDataCallback(
        sync_driver::ClearBrowsingDataCallback clear_browsing_data_callback);

    scoped_ptr<sync_driver::FakeSyncClient> Build();

   private:
    // Associated bundle to source objects from.
    ProfileSyncServiceBundle* const bundle_;

    sync_driver::ClearBrowsingDataCallback clear_browsing_data_callback_;

    DISALLOW_COPY_AND_ASSIGN(SyncClientBuilder);
  };

  // Creates an InitParams instance with the specified |start_behavior| and
  // |sync_client|, and fills the rest with dummy values and objects owned by
  // the bundle.
  ProfileSyncService::InitParams CreateBasicInitParams(
      browser_sync::ProfileSyncServiceStartBehavior start_behavior,
      scoped_ptr<sync_driver::SyncClient> sync_client);

  // Accessors

  net::URLRequestContextGetter* url_request_context() {
    return url_request_context_.get();
  }

  syncable_prefs::TestingPrefServiceSyncable* pref_service() {
    return &pref_service_;
  }

  FakeProfileOAuth2TokenService* auth_service() { return &auth_service_; }

  FakeSigninManagerType* signin_manager() { return &signin_manager_; }

  AccountTrackerService* account_tracker() { return &account_tracker_; }

  SyncApiComponentFactoryMock* component_factory() {
    return &component_factory_;
  }

  sync_sessions::FakeSyncSessionsClient* sync_sessions_client() {
    return &sync_sessions_client_;
  }

 private:
  base::SequencedWorkerPoolOwner worker_pool_owner_;
  syncable_prefs::TestingPrefServiceSyncable pref_service_;
  TestSigninClient signin_client_;
  AccountTrackerService account_tracker_;
  FakeSigninManagerType signin_manager_;
  FakeProfileOAuth2TokenService auth_service_;
  SyncApiComponentFactoryMock component_factory_;
  sync_sessions::FakeSyncSessionsClient sync_sessions_client_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceBundle);
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_TEST_UTIL_H_
