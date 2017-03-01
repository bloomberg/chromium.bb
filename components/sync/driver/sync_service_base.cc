// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_base.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/syslog_logging.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/engine_components_factory_impl.h"

namespace syncer {

namespace {

const base::FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

EngineComponentsFactory::Switches EngineSwitchesFromCommandLine() {
  EngineComponentsFactory::Switches factory_switches = {
      EngineComponentsFactory::ENCRYPTION_KEYSTORE,
      EngineComponentsFactory::BACKOFF_NORMAL};

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kSyncShortInitialRetryOverride)) {
    factory_switches.backoff_override =
        EngineComponentsFactory::BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE;
  }
  if (cl->HasSwitch(switches::kSyncEnableGetUpdateAvoidance)) {
    factory_switches.pre_commit_updates_policy =
        EngineComponentsFactory::FORCE_ENABLE_PRE_COMMIT_UPDATE_AVOIDANCE;
  }
  if (cl->HasSwitch(switches::kSyncShortNudgeDelayForTest)) {
    factory_switches.nudge_delay =
        EngineComponentsFactory::NudgeDelay::SHORT_NUDGE_DELAY;
  }
  return factory_switches;
}

}  // namespace

SyncServiceBase::SyncServiceBase(std::unique_ptr<SyncClient> sync_client,
                                 std::unique_ptr<SigninManagerWrapper> signin,
                                 const version_info::Channel& channel,
                                 const base::FilePath& base_directory,
                                 const std::string& debug_identifier)
    : sync_client_(std::move(sync_client)),
      signin_(std::move(signin)),
      channel_(channel),
      base_directory_(base_directory),
      sync_data_folder_(
          base_directory_.Append(base::FilePath(kSyncDataFolderName))),
      debug_identifier_(debug_identifier),
      sync_prefs_(sync_client_->GetPrefService()) {
  ResetCryptoState();
}

SyncServiceBase::~SyncServiceBase() = default;

void SyncServiceBase::AddObserver(SyncServiceObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void SyncServiceBase::RemoveObserver(SyncServiceObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

bool SyncServiceBase::HasObserver(const SyncServiceObserver* observer) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return observers_.HasObserver(observer);
}

void SyncServiceBase::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnStateChanged(this);
  }
}

void SyncServiceBase::InitializeEngine() {
  DCHECK(engine_);

  if (!sync_thread_) {
    sync_thread_ = base::MakeUnique<base::Thread>("Chrome_SyncThread");
    base::Thread::Options options;
    options.timer_slack = base::TIMER_SLACK_MAXIMUM;
    CHECK(sync_thread_->StartWithOptions(options));
  }

  SyncEngine::InitParams params;
  params.sync_task_runner = sync_thread_->task_runner();
  params.host = this;
  params.registrar = base::MakeUnique<SyncBackendRegistrar>(
      debug_identifier_, base::Bind(&SyncClient::CreateModelWorkerForGroup,
                                    base::Unretained(sync_client_.get())));
  params.encryption_observer_proxy = crypto_->GetEncryptionObserverProxy();
  params.extensions_activity = sync_client_->GetExtensionsActivity();
  params.event_handler = GetJsEventHandler();
  params.service_url = sync_service_url();
  params.sync_user_agent = GetLocalDeviceInfoProvider()->GetSyncUserAgent();
  params.http_factory_getter = MakeHttpPostProviderFactoryGetter();
  params.credentials = GetCredentials();
  invalidation::InvalidationService* invalidator =
      sync_client_->GetInvalidationService();
  params.invalidator_client_id =
      invalidator ? invalidator->GetInvalidatorClientId() : "",
  params.sync_manager_factory = base::MakeUnique<SyncManagerFactory>();
  // The first time we start up the engine we want to ensure we have a clean
  // directory, so delete any old one that might be there.
  params.delete_sync_data_folder = !IsFirstSetupComplete();
  params.enable_local_sync_backend = sync_prefs_.IsLocalSyncEnabled();
  params.local_sync_backend_folder = sync_client_->GetLocalSyncBackendFolder();
  params.restored_key_for_bootstrapping =
      sync_prefs_.GetEncryptionBootstrapToken();
  params.restored_keystore_key_for_bootstrapping =
      sync_prefs_.GetKeystoreEncryptionBootstrapToken();
  params.engine_components_factory =
      base::MakeUnique<EngineComponentsFactoryImpl>(
          EngineSwitchesFromCommandLine());
  params.unrecoverable_error_handler = GetUnrecoverableErrorHandler();
  params.report_unrecoverable_error_function =
      base::Bind(ReportUnrecoverableError, channel_);
  params.saved_nigori_state = crypto_->TakeSavedNigoriState();
  sync_prefs_.GetInvalidationVersions(&params.invalidation_versions);

  engine_->Initialize(std::move(params));
}

void SyncServiceBase::ResetCryptoState() {
  crypto_ = base::MakeUnique<SyncServiceCrypto>(
      base::BindRepeating(&SyncServiceBase::NotifyObservers,
                          base::Unretained(this)),
      base::BindRepeating(&SyncService::GetPreferredDataTypes,
                          base::Unretained(this)),
      &sync_prefs_);
}

}  // namespace syncer
