// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/browser/profile_sync_test_util.h"

#include "components/history/core/browser/history_model_worker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync_driver/glue/browser_thread_model_worker.h"
#include "components/sync_driver/glue/ui_model_worker.h"
#include "components/sync_driver/signin_manager_wrapper.h"
#include "components/sync_driver/sync_prefs.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/internal_api/public/engine/passive_model_worker.h"

using sync_driver::ClearBrowsingDataCallback;

namespace browser_sync {

namespace {

class BundleSyncClient : public sync_driver::FakeSyncClient {
 public:
  BundleSyncClient(
      sync_driver::SyncApiComponentFactory* factory,
      PrefService* pref_service,
      const ClearBrowsingDataCallback& clear_browsing_data_callback,
      sync_sessions::SyncSessionsClient* sync_sessions_client,
      autofill::PersonalDataManager* personal_data_manager,
      const base::Callback<base::WeakPtr<syncer::SyncableService>(
          syncer::ModelType type)>& get_syncable_service_callback,
      const base::Callback<sync_driver::SyncService*(void)>&
          get_sync_service_callback,
      scoped_refptr<base::SingleThreadTaskRunner> db_thread,
      scoped_refptr<base::SingleThreadTaskRunner> file_thread,
      history::HistoryService* history_service);

  ~BundleSyncClient() override;

  PrefService* GetPrefService() override;
  ClearBrowsingDataCallback GetClearBrowsingDataCallback() override;
  sync_sessions::SyncSessionsClient* GetSyncSessionsClient() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override;
  sync_driver::SyncService* GetSyncService() override;
  scoped_refptr<syncer::ModelSafeWorker> CreateModelWorkerForGroup(
      syncer::ModelSafeGroup group,
      syncer::WorkerLoopDestructionObserver* observer) override;
  history::HistoryService* GetHistoryService() override;

 private:
  PrefService* const pref_service_;
  const ClearBrowsingDataCallback clear_browsing_data_callback_;
  sync_sessions::SyncSessionsClient* const sync_sessions_client_;
  autofill::PersonalDataManager* const personal_data_manager_;
  const base::Callback<base::WeakPtr<syncer::SyncableService>(
      syncer::ModelType type)>
      get_syncable_service_callback_;
  const base::Callback<sync_driver::SyncService*(void)>
      get_sync_service_callback_;
  // These task runners, if not null, are used in CreateModelWorkerForGroup.
  const scoped_refptr<base::SingleThreadTaskRunner> db_thread_;
  const scoped_refptr<base::SingleThreadTaskRunner> file_thread_;
  history::HistoryService* history_service_;
};

BundleSyncClient::BundleSyncClient(
    sync_driver::SyncApiComponentFactory* factory,
    PrefService* pref_service,
    const ClearBrowsingDataCallback& clear_browsing_data_callback,
    sync_sessions::SyncSessionsClient* sync_sessions_client,
    autofill::PersonalDataManager* personal_data_manager,
    const base::Callback<base::WeakPtr<syncer::SyncableService>(
        syncer::ModelType type)>& get_syncable_service_callback,
    const base::Callback<sync_driver::SyncService*(void)>&
        get_sync_service_callback,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread,
    history::HistoryService* history_service)
    : sync_driver::FakeSyncClient(factory),
      pref_service_(pref_service),
      clear_browsing_data_callback_(clear_browsing_data_callback),
      sync_sessions_client_(sync_sessions_client),
      personal_data_manager_(personal_data_manager),
      get_syncable_service_callback_(get_syncable_service_callback),
      get_sync_service_callback_(get_sync_service_callback),
      db_thread_(db_thread),
      file_thread_(file_thread),
      history_service_(history_service) {
  DCHECK_EQ(!!db_thread_, !!file_thread_);
}

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

autofill::PersonalDataManager* BundleSyncClient::GetPersonalDataManager() {
  return personal_data_manager_;
}

base::WeakPtr<syncer::SyncableService>
BundleSyncClient::GetSyncableServiceForType(syncer::ModelType type) {
  if (get_syncable_service_callback_.is_null())
    return sync_driver::FakeSyncClient::GetSyncableServiceForType(type);
  return get_syncable_service_callback_.Run(type);
}

sync_driver::SyncService* BundleSyncClient::GetSyncService() {
  if (get_sync_service_callback_.is_null())
    return sync_driver::FakeSyncClient::GetSyncService();
  return get_sync_service_callback_.Run();
}

scoped_refptr<syncer::ModelSafeWorker>
BundleSyncClient::CreateModelWorkerForGroup(
    syncer::ModelSafeGroup group,
    syncer::WorkerLoopDestructionObserver* observer) {
  if (!db_thread_)
    return FakeSyncClient::CreateModelWorkerForGroup(group, observer);
  DCHECK(file_thread_) << "DB thread was specified but FILE thread was not.";
  switch (group) {
    case syncer::GROUP_DB:
      return new BrowserThreadModelWorker(db_thread_, syncer::GROUP_DB,
                                          observer);
    case syncer::GROUP_FILE:
      return new BrowserThreadModelWorker(file_thread_, syncer::GROUP_FILE,
                                          observer);
    case syncer::GROUP_UI:
      return new UIModelWorker(base::ThreadTaskRunnerHandle::Get(), observer);
    case syncer::GROUP_PASSIVE:
      return new syncer::PassiveModelWorker(observer);
    case syncer::GROUP_HISTORY: {
      history::HistoryService* history_service = GetHistoryService();
      if (!history_service)
        return nullptr;
      return new HistoryModelWorker(history_service->AsWeakPtr(),
                                    base::ThreadTaskRunnerHandle::Get(),
                                    observer);
    }
    default:
      return nullptr;
  }
}

history::HistoryService* BundleSyncClient::GetHistoryService() {
  if (history_service_)
    return history_service_;
  return FakeSyncClient::GetHistoryService();
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

void ProfileSyncServiceBundle::SyncClientBuilder::SetPersonalDataManager(
    autofill::PersonalDataManager* personal_data_manager) {
  personal_data_manager_ = personal_data_manager;
}

// The client will call this callback to produce the service.
void ProfileSyncServiceBundle::SyncClientBuilder::SetSyncableServiceCallback(
    const base::Callback<base::WeakPtr<syncer::SyncableService>(
        syncer::ModelType type)>& get_syncable_service_callback) {
  get_syncable_service_callback_ = get_syncable_service_callback;
}

// The client will call this callback to produce the service.
void ProfileSyncServiceBundle::SyncClientBuilder::SetSyncServiceCallback(
    const base::Callback<sync_driver::SyncService*(void)>&
        get_sync_service_callback) {
  get_sync_service_callback_ = get_sync_service_callback;
}

void ProfileSyncServiceBundle::SyncClientBuilder::SetHistoryService(
    history::HistoryService* history_service) {
  history_service_ = history_service;
}

scoped_ptr<sync_driver::FakeSyncClient>
ProfileSyncServiceBundle::SyncClientBuilder::Build() {
  return make_scoped_ptr(new BundleSyncClient(
      bundle_->component_factory(), bundle_->pref_service(),
      clear_browsing_data_callback_, bundle_->sync_sessions_client(),
      personal_data_manager_, get_syncable_service_callback_,
      get_sync_service_callback_,
      activate_model_creation_ ? bundle_->db_thread() : nullptr,
      activate_model_creation_ ? bundle_->file_thread() : nullptr,
      history_service_));
}

struct ProfileSyncServiceBundle::ThreadProvider {
 public:
  ThreadProvider() : worker_pool_owner_(2, "sync test worker pool") {
    DCHECK(base::MessageLoop::current())
        << "The test including the bundle also needs to include a MessageLoop";
  }
  ~ThreadProvider() = default;

  base::SequencedWorkerPoolOwner worker_pool_owner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadProvider);
};

ProfileSyncServiceBundle::ProfileSyncServiceBundle()
    : ProfileSyncServiceBundle(make_scoped_ptr(new ThreadProvider),
                               nullptr,
                               nullptr,
                               nullptr) {}

ProfileSyncServiceBundle::ProfileSyncServiceBundle(
    const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& file_thread,
    base::SequencedWorkerPool* worker_pool)
    : ProfileSyncServiceBundle(nullptr, db_thread, file_thread, worker_pool) {}

ProfileSyncServiceBundle::~ProfileSyncServiceBundle() {}

ProfileSyncService::InitParams ProfileSyncServiceBundle::CreateBasicInitParams(
    browser_sync::ProfileSyncServiceStartBehavior start_behavior,
    scoped_ptr<sync_driver::SyncClient> sync_client) {
  DCHECK(worker_pool_);

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
  init_params.db_thread = db_thread_;
  init_params.file_thread = file_thread_;
  init_params.blocking_pool = worker_pool_;

  return init_params;
}

ProfileSyncServiceBundle::ProfileSyncServiceBundle(
    scoped_ptr<ThreadProvider> thread_provider,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread,
    base::SequencedWorkerPool* worker_pool)
    : thread_provider_(std::move(thread_provider)),
      db_thread_(thread_provider_ ? base::ThreadTaskRunnerHandle::Get()
                                  : db_thread),
      file_thread_(thread_provider_ ? base::ThreadTaskRunnerHandle::Get()
                                    : file_thread),
      worker_pool_(thread_provider_
                       ? thread_provider_->worker_pool_owner_.pool().get()
                       : worker_pool),
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

}  // namespace browser_sync
