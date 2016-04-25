// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_diagnosis_runner.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/crx_file/id_util.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using content::BrowserThread;
using extensions::Extension;

namespace chromeos {

namespace {

const char kOAuthRefreshToken[] = "refresh_token";
const char kOAuthClientId[] = "client_id";
const char kOAuthClientSecret[] = "client_secret";

const base::FilePath::CharType kOAuthFileName[] =
    FILE_PATH_LITERAL("kiosk_auth");

const int kMaxLaunchAttempt = 5;

}  // namespace

StartupAppLauncher::StartupAppLauncher(Profile* profile,
                                       const std::string& app_id,
                                       bool diagnostic_mode,
                                       StartupAppLauncher::Delegate* delegate)
    : profile_(profile),
      app_id_(app_id),
      diagnostic_mode_(diagnostic_mode),
      delegate_(delegate) {
  DCHECK(profile_);
  DCHECK(crx_file::id_util::IdIsValid(app_id_));
  KioskAppManager::Get()->AddObserver(this);
}

StartupAppLauncher::~StartupAppLauncher() {
  KioskAppManager::Get()->RemoveObserver(this);

  // StartupAppLauncher can be deleted at anytime during the launch process
  // through a user bailout shortcut.
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)
      ->RemoveObserver(this);
  extensions::InstallTrackerFactory::GetForBrowserContext(profile_)
      ->RemoveObserver(this);
}

void StartupAppLauncher::Initialize() {
  StartLoadingOAuthFile();
}

void StartupAppLauncher::ContinueWithNetworkReady() {
  if (!network_ready_handled_) {
    network_ready_handled_ = true;
    // The network might not be ready when KioskAppManager tries to update
    // external cache initially. Update the external cache now that the network
    // is ready for sure.
    wait_for_crx_update_ = true;
    KioskAppManager::Get()->UpdateExternalCache();
  }
}

void StartupAppLauncher::StartLoadingOAuthFile() {
  delegate_->OnLoadingOAuthFile();

  KioskOAuthParams* auth_params = new KioskOAuthParams();
  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&StartupAppLauncher::LoadOAuthFileOnBlockingPool,
                 auth_params),
      base::Bind(&StartupAppLauncher::OnOAuthFileLoaded,
                 AsWeakPtr(),
                 base::Owned(auth_params)));
}

// static.
void StartupAppLauncher::LoadOAuthFileOnBlockingPool(
    KioskOAuthParams* auth_params) {
  int error_code = JSONFileValueDeserializer::JSON_NO_ERROR;
  std::string error_msg;
  base::FilePath user_data_dir;
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  base::FilePath auth_file = user_data_dir.Append(kOAuthFileName);
  std::unique_ptr<JSONFileValueDeserializer> deserializer(
      new JSONFileValueDeserializer(user_data_dir.Append(kOAuthFileName)));
  std::unique_ptr<base::Value> value =
      deserializer->Deserialize(&error_code, &error_msg);
  base::DictionaryValue* dict = NULL;
  if (error_code != JSONFileValueDeserializer::JSON_NO_ERROR ||
      !value.get() || !value->GetAsDictionary(&dict)) {
    LOG(WARNING) << "Can't find auth file at " << auth_file.value();
    return;
  }

  dict->GetString(kOAuthRefreshToken, &auth_params->refresh_token);
  dict->GetString(kOAuthClientId, &auth_params->client_id);
  dict->GetString(kOAuthClientSecret, &auth_params->client_secret);
}

void StartupAppLauncher::OnOAuthFileLoaded(KioskOAuthParams* auth_params) {
  auth_params_ = *auth_params;
  // Override chrome client_id and secret that will be used for identity
  // API token minting.
  if (!auth_params_.client_id.empty() && !auth_params_.client_secret.empty()) {
    UserSessionManager::GetInstance()->SetAppModeChromeClientOAuthInfo(
            auth_params_.client_id,
            auth_params_.client_secret);
  }

  // If we are restarting chrome (i.e. on crash), we need to initialize
  // OAuth2TokenService as well.
  InitializeTokenService();
}

void StartupAppLauncher::RestartLauncher() {
  // If the installer is still running in the background, we don't need to
  // restart the launch process. We will just wait until it completes and
  // launches the kiosk app.
  if (extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->pending_extension_manager()
          ->IsIdPending(app_id_)) {
    LOG(WARNING) << "Installer still running";
    return;
  }

  MaybeInitializeNetwork();
}

void StartupAppLauncher::MaybeInitializeNetwork() {
  network_ready_handled_ = false;

  const Extension* extension = GetPrimaryAppExtension();
  bool crx_cached = KioskAppManager::Get()->HasCachedCrx(app_id_);
  const bool requires_network =
      (!extension && !crx_cached) ||
      (extension &&
       !extensions::OfflineEnabledInfo::IsOfflineEnabled(extension));

  if (requires_network) {
    delegate_->InitializeNetwork();
    return;
  }

  // Update the offline enabled crx cache if the network is ready;
  // or just install the app.
  if (delegate_->IsNetworkReady())
    ContinueWithNetworkReady();
  else
    BeginInstall();
}

void StartupAppLauncher::InitializeTokenService() {
  delegate_->OnInitializingTokenService();

  ProfileOAuth2TokenService* profile_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  const std::string primary_account_id =
      signin_manager->GetAuthenticatedAccountId();
  if (profile_token_service->RefreshTokenIsAvailable(primary_account_id) ||
      auth_params_.refresh_token.empty()) {
    MaybeInitializeNetwork();
  } else {
    // Pass oauth2 refresh token from the auth file.
    // TODO(zelidrag): We should probably remove this option after M27.
    // TODO(fgorski): This can go when we have persistence implemented on PO2TS.
    // Unless the code is no longer needed.
    // TODO(rogerta): Now that this CL implements token persistence in PO2TS, is
    // this code still needed?  See above two TODOs.
    //
    // ProfileOAuth2TokenService triggers either OnRefreshTokenAvailable or
    // OnRefreshTokensLoaded. Given that we want to handle exactly one event,
    // whichever comes first, both handlers call RemoveObserver on PO2TS.
    // Handling any of the two events is the only way to resume the execution
    // and enable Cleanup method to be called, self-invoking a destructor.
    profile_token_service->AddObserver(this);

    profile_token_service->UpdateCredentials(
        primary_account_id,
        auth_params_.refresh_token);
  }
}

void StartupAppLauncher::OnRefreshTokenAvailable(
    const std::string& account_id) {
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)
      ->RemoveObserver(this);
  MaybeInitializeNetwork();
}

void StartupAppLauncher::OnRefreshTokensLoaded() {
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)
      ->RemoveObserver(this);
  MaybeInitializeNetwork();
}

void StartupAppLauncher::MaybeLaunchApp() {
  // Check if the app is offline enabled.
  const Extension* extension = GetPrimaryAppExtension();
  DCHECK(extension);
  const bool offline_enabled =
      extensions::OfflineEnabledInfo::IsOfflineEnabled(extension);
  if (offline_enabled || delegate_->IsNetworkReady()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&StartupAppLauncher::OnReadyToLaunch, AsWeakPtr()));
  } else {
    ++launch_attempt_;
    if (launch_attempt_ < kMaxLaunchAttempt) {
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&StartupAppLauncher::MaybeInitializeNetwork, AsWeakPtr()));
      return;
    }
    OnLaunchFailure(KioskAppLaunchError::UNABLE_TO_LAUNCH);
  }
}

void StartupAppLauncher::MaybeCheckExtensionUpdate() {
  extensions::ExtensionUpdater* updater =
      extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->updater();
  if (!delegate_->IsNetworkReady() || !updater ||
      PrimaryAppHasPendingUpdate()) {
    MaybeLaunchApp();
    return;
  }

  extension_update_found_ = false;
  registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND,
                 content::NotificationService::AllSources());

  // Enforce an immediate version update check for all extensions before
  // launching the primary app. After the chromeos is updated, the shared
  // module(e.g. ARC runtime) may need to be updated to a newer version
  // compatible with the new chromeos. See crbug.com/555083.
  extensions::ExtensionUpdater::CheckParams params;
  params.install_immediately = true;
  params.callback = base::Bind(
      &StartupAppLauncher::OnExtensionUpdateCheckFinished, AsWeakPtr());
  updater->CheckNow(params);
}

void StartupAppLauncher::OnExtensionUpdateCheckFinished() {
  if (extension_update_found_) {
    // Reload the primary app to make sure any reference to the previous version
    // of the shared module, extension, etc will be cleaned up andthe new
    // version will be loaded.
    extensions::ExtensionSystem::Get(profile_)
            ->extension_service()
            ->ReloadExtension(app_id_);
    extension_update_found_ = false;
  }
  registrar_.Remove(this, extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND,
                    content::NotificationService::AllSources());

  MaybeLaunchApp();
}

void StartupAppLauncher::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK(type == extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND);
  typedef const std::pair<std::string, Version> UpdateDetails;
  const std::string& id = content::Details<UpdateDetails>(details)->first;
  const Version& version = content::Details<UpdateDetails>(details)->second;
  VLOG(1) << "Found extension update id=" << id
      << " version=" << version.GetString();
  extension_update_found_ = true;
}

void StartupAppLauncher::OnFinishCrxInstall(const std::string& extension_id,
                                            bool success) {
  // Wait for pending updates or dependent extensions to download.
  if (extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->pending_extension_manager()
          ->HasPendingExtensions()) {
    return;
  }

  extensions::InstallTracker* tracker =
      extensions::InstallTrackerFactory::GetForBrowserContext(profile_);
  tracker->RemoveObserver(this);
  if (delegate_->IsShowingNetworkConfigScreen()) {
    LOG(WARNING) << "Showing network config screen";
    return;
  }

  if (DidPrimaryOrSecondaryAppFailedToInstall(success, extension_id)) {
    OnLaunchFailure(KioskAppLaunchError::UNABLE_TO_INSTALL);
    return;
  }

  if (GetPrimaryAppExtension()) {
    if (!secondary_apps_installed_)
      MaybeInstallSecondaryApps();
    else
      MaybeCheckExtensionUpdate();
  }
}

void StartupAppLauncher::OnKioskExtensionLoadedInCache(
    const std::string& app_id) {
  OnKioskAppDataLoadStatusChanged(app_id);
}

void StartupAppLauncher::OnKioskExtensionDownloadFailed(
    const std::string& app_id) {
  OnKioskAppDataLoadStatusChanged(app_id);
}

void StartupAppLauncher::OnKioskAppDataLoadStatusChanged(
    const std::string& app_id) {
  if (app_id != app_id_ || !wait_for_crx_update_)
    return;

  wait_for_crx_update_ = false;
  if (KioskAppManager::Get()->HasCachedCrx(app_id_))
    BeginInstall();
  else
    OnLaunchFailure(KioskAppLaunchError::UNABLE_TO_DOWNLOAD);
}

bool StartupAppLauncher::IsAnySecondaryAppPending() const {
  const extensions::Extension* extension = GetPrimaryAppExtension();
  DCHECK(extension);
  extensions::KioskModeInfo* info = extensions::KioskModeInfo::Get(extension);
  for (const auto& id : info->secondary_app_ids) {
    if (extensions::ExtensionSystem::Get(profile_)
            ->extension_service()
            ->pending_extension_manager()
            ->IsIdPending(id)) {
      return true;
    }
  }
  return false;
}

bool StartupAppLauncher::AreSecondaryAppsInstalled() const {
  const extensions::Extension* extension = GetPrimaryAppExtension();
  DCHECK(extension);
  extensions::KioskModeInfo* info = extensions::KioskModeInfo::Get(extension);
  for (const auto& id : info->secondary_app_ids) {
    if (!extensions::ExtensionSystem::Get(profile_)
             ->extension_service()
             ->GetInstalledExtension(id)) {
      return false;
    }
  }
  return true;
}

bool StartupAppLauncher::HasSecondaryApps() const {
  const extensions::Extension* extension = GetPrimaryAppExtension();
  DCHECK(extension);
  return extensions::KioskModeInfo::HasSecondaryApps(extension);
}

bool StartupAppLauncher::PrimaryAppHasPendingUpdate() const {
  return !!extensions::ExtensionSystem::Get(profile_)
               ->extension_service()
               ->GetPendingExtensionUpdate(app_id_);
}

bool StartupAppLauncher::DidPrimaryOrSecondaryAppFailedToInstall(
    bool success,
    const std::string& id) const {
  if (success)
    return false;

  if (id == app_id_) {
    LOG(ERROR) << "Failed to install crx file of the primary app id=" << id;
    return true;
  }

  const extensions::Extension* extension = GetPrimaryAppExtension();
  if (!extension)
    return false;

  extensions::KioskModeInfo* info = extensions::KioskModeInfo::Get(extension);
  for (const auto& app_id : info->secondary_app_ids) {
    if (app_id == id) {
      LOG(ERROR) << "Failed to install a secondary app id=" << id;
      return true;
    }
  }

  LOG(WARNING) << "Failed to install crx file for an app id=" << id;
  return false;
}

const extensions::Extension* StartupAppLauncher::GetPrimaryAppExtension()
    const {
  return extensions::ExtensionSystem::Get(profile_)
      ->extension_service()
      ->GetInstalledExtension(app_id_);
}

void StartupAppLauncher::LaunchApp() {
  if (!ready_to_launch_) {
    NOTREACHED();
    LOG(ERROR) << "LaunchApp() called but launcher is not initialized.";
  }

  const Extension* extension = GetPrimaryAppExtension();
  CHECK(extension);

  if (!extensions::KioskModeInfo::IsKioskEnabled(extension)) {
    OnLaunchFailure(KioskAppLaunchError::NOT_KIOSK_ENABLED);
    return;
  }

  // Always open the app in a window.
  OpenApplication(AppLaunchParams(profile_, extension,
                                  extensions::LAUNCH_CONTAINER_WINDOW,
                                  NEW_WINDOW, extensions::SOURCE_KIOSK));
  KioskAppManager::Get()->InitSession(profile_, app_id_);

  user_manager::UserManager::Get()->SessionStarted();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

  if (diagnostic_mode_)
    KioskDiagnosisRunner::Run(profile_, app_id_);

  OnLaunchSuccess();
}

void StartupAppLauncher::OnLaunchSuccess() {
  delegate_->OnLaunchSucceeded();
}

void StartupAppLauncher::OnLaunchFailure(KioskAppLaunchError::Error error) {
  LOG(ERROR) << "App launch failed, error: " << error;
  DCHECK_NE(KioskAppLaunchError::NONE, error);

  delegate_->OnLaunchFailed(error);
}

void StartupAppLauncher::BeginInstall() {
  extensions::file_util::SetUseSafeInstallation(true);
  KioskAppManager::Get()->InstallFromCache(app_id_);
  if (extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->pending_extension_manager()
          ->IsIdPending(app_id_)) {
    delegate_->OnInstallingApp();
    // Observe the crx installation events.
    extensions::InstallTracker* tracker =
        extensions::InstallTrackerFactory::GetForBrowserContext(profile_);
    tracker->AddObserver(this);
    return;
  }

  if (GetPrimaryAppExtension()) {
    // Install secondary apps.
    MaybeInstallSecondaryApps();
  } else {
    // The extension is skipped for installation due to some error.
    OnLaunchFailure(KioskAppLaunchError::UNABLE_TO_INSTALL);
  }
}

void StartupAppLauncher::MaybeInstallSecondaryApps() {
  if (!AreSecondaryAppsInstalled() && !delegate_->IsNetworkReady()) {
    DelayNetworkCall(
        base::TimeDelta::FromMilliseconds(kDefaultNetworkRetryDelayMS),
        base::Bind(&StartupAppLauncher::MaybeInstallSecondaryApps,
                   AsWeakPtr()));
    return;
  }

  secondary_apps_installed_ = true;
  extensions::KioskModeInfo* info =
      extensions::KioskModeInfo::Get(GetPrimaryAppExtension());
  KioskAppManager::Get()->InstallSecondaryApps(info->secondary_app_ids);
  if (IsAnySecondaryAppPending()) {
    delegate_->OnInstallingApp();
    // Observe the crx installation events.
    extensions::InstallTracker* tracker =
        extensions::InstallTrackerFactory::GetForBrowserContext(profile_);
    tracker->AddObserver(this);
    return;
  }

  if (AreSecondaryAppsInstalled()) {
    // Check extension update before launching the primary kiosk app.
    MaybeCheckExtensionUpdate();
  } else {
    OnLaunchFailure(KioskAppLaunchError::UNABLE_TO_INSTALL);
  }
}

void StartupAppLauncher::OnReadyToLaunch() {
  ready_to_launch_ = true;
  MaybeUpdateAppData();
  delegate_->OnReadyToLaunch();
}

void StartupAppLauncher::MaybeUpdateAppData() {
  // Skip copying meta data from the current installed primary app when
  // there is a pending update.
  if (PrimaryAppHasPendingUpdate())
    return;

  KioskAppManager::Get()->ClearAppData(app_id_);
  KioskAppManager::Get()->UpdateAppDataFromProfile(app_id_, profile_, NULL);
}

}   // namespace chromeos
