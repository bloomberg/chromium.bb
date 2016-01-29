// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/browser/profile_sync_test_util.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync_driver/signin_manager_wrapper.h"
#include "components/sync_driver/sync_prefs.h"
#include "net/url_request/url_request_test_util.h"

using sync_driver::ClearBrowsingDataCallback;

namespace browser_sync {

namespace {

class BundleSyncClient : public sync_driver::FakeSyncClient {
 public:
  BundleSyncClient(sync_driver::SyncApiComponentFactory* factory,
                   PrefService* pref_service,
                   ClearBrowsingDataCallback clear_browsing_data_callback,
                   sync_sessions::SyncSessionsClient* sync_sessions_client);

  ~BundleSyncClient() override;

  PrefService* GetPrefService() override;
  ClearBrowsingDataCallback GetClearBrowsingDataCallback() override;
  sync_sessions::SyncSessionsClient* GetSyncSessionsClient() override;

 private:
  PrefService* const pref_service_;
  const ClearBrowsingDataCallback clear_browsing_data_callback_;
  sync_sessions::SyncSessionsClient* const sync_sessions_client_;
};

BundleSyncClient::BundleSyncClient(
    sync_driver::SyncApiComponentFactory* factory,
    PrefService* pref_service,
    ClearBrowsingDataCallback clear_browsing_data_callback,
    sync_sessions::SyncSessionsClient* sync_sessions_client)
    : sync_driver::FakeSyncClient(factory),
      pref_service_(pref_service),
      clear_browsing_data_callback_(clear_browsing_data_callback),
      sync_sessions_client_(sync_sessions_client) {}

BundleSyncClient::~BundleSyncClient() = default;

PrefService* BundleSyncClient::GetPrefService() {
  return pref_service_;
}

ClearBrowsingDataCallback BundleSyncClient::GetClearBrowsingDataCallback() {
  return clear_browsing_data_callback_;
}

sync_sessions::SyncSessionsClient* BundleSyncClient::GetSyncSessionsClient() {
  return sync_sessions_client_;
}

}  // namespace

void EmptyNetworkTimeUpdate(const base::Time&,
                            const base::TimeDelta&,
                            const base::TimeDelta&) {}

void RegisterPrefsForProfileSyncService(
    user_prefs::PrefRegistrySyncable* registry) {
  sync_driver::SyncPrefs::RegisterProfilePrefs(registry);
  AccountTrackerService::RegisterPrefs(registry);
  SigninManagerBase::RegisterProfilePrefs(registry);
  SigninManagerBase::RegisterPrefs(registry);
}

ProfileSyncServiceBundle::SyncClientBuilder::~SyncClientBuilder() = default;

ProfileSyncServiceBundle::SyncClientBuilder::SyncClientBuilder(
    ProfileSyncServiceBundle* bundle)
    : bundle_(bundle) {}

void ProfileSyncServiceBundle::SyncClientBuilder::SetClearBrowsingDataCallback(
    ClearBrowsingDataCallback clear_browsing_data_callback) {
  clear_browsing_data_callback_ = clear_browsing_data_callback;
}

scoped_ptr<sync_driver::FakeSyncClient>
ProfileSyncServiceBundle::SyncClientBuilder::Build() {
  return make_scoped_ptr(new BundleSyncClient(
      bundle_->component_factory(), bundle_->pref_service(),
      clear_browsing_data_callback_, bundle_->sync_sessions_client()));
}

ProfileSyncServiceBundle::ProfileSyncServiceBundle()
    : worker_pool_owner_(2, "sync test worker pool"),
      signin_client_(&pref_service_),
#if defined(OS_CHROMEOS)
      signin_manager_(&signin_client_, &account_tracker_),
#else
      signin_manager_(&signin_client_,
                      &auth_service_,
                      &account_tracker_,
                      nullptr),
#endif
      url_request_context_(new net::TestURLRequestContextGetter(
          base::ThreadTaskRunnerHandle::Get())) {
  browser_sync::RegisterPrefsForProfileSyncService(pref_service_.registry());
  auth_service_.set_auto_post_fetch_response_on_message_loop(true);
  account_tracker_.Initialize(&signin_client_);
  signin_manager_.Initialize(&pref_service_);
}

ProfileSyncServiceBundle::~ProfileSyncServiceBundle() {}

ProfileSyncService::InitParams ProfileSyncServiceBundle::CreateBasicInitParams(
    browser_sync::ProfileSyncServiceStartBehavior start_behavior,
    scoped_ptr<sync_driver::SyncClient> sync_client) {
  ProfileSyncService::InitParams init_params;

  init_params.start_behavior = start_behavior;
  init_params.sync_client = std::move(sync_client);
  init_params.signin_wrapper =
      make_scoped_ptr(new SigninManagerWrapper(signin_manager()));
  init_params.oauth2_token_service = auth_service();
  init_params.network_time_update_callback =
      base::Bind(&EmptyNetworkTimeUpdate);
  init_params.base_directory = base::FilePath(FILE_PATH_LITERAL("dummyPath"));
  init_params.url_request_context = url_request_context();
  init_params.debug_identifier = "dummyDebugName";
  init_params.channel = version_info::Channel::UNKNOWN;
  init_params.db_thread = base::ThreadTaskRunnerHandle::Get();
  init_params.file_thread = base::ThreadTaskRunnerHandle::Get();
  init_params.blocking_pool = worker_pool_owner_.pool().get();

  return init_params;
}

}  // namespace browser_sync
