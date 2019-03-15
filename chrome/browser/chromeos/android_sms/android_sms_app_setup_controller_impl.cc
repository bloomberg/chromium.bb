// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_app_setup_controller_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace chromeos {

namespace android_sms {

namespace {

const char kDefaultToPersistCookieName[] = "default_to_persist";
const char kMigrationCookieName[] = "cros_migrated_to";
const char kDefaultToPersistCookieValue[] = "true";

}  // namespace

AndroidSmsAppSetupControllerImpl::PwaDelegate::PwaDelegate() = default;

AndroidSmsAppSetupControllerImpl::PwaDelegate::~PwaDelegate() = default;

const extensions::Extension*
AndroidSmsAppSetupControllerImpl::PwaDelegate::GetPwaForUrl(
    const GURL& install_url,
    Profile* profile) {
  // PWA windowing is disabled for some browser tests.
  if (!base::FeatureList::IsEnabled(features::kDesktopPWAWindowing))
    return nullptr;

  return extensions::util::GetInstalledPwaForUrl(profile, install_url);
}

network::mojom::CookieManager*
AndroidSmsAppSetupControllerImpl::PwaDelegate::GetCookieManager(
    const GURL& app_url,
    Profile* profile) {
  return content::BrowserContext::GetStoragePartitionForSite(profile, app_url)
      ->GetCookieManagerForBrowserProcess();
}

AndroidSmsAppSetupControllerImpl::AndroidSmsAppSetupControllerImpl(
    Profile* profile,
    web_app::PendingAppManager* pending_app_manager,
    HostContentSettingsMap* host_content_settings_map)
    : profile_(profile),
      pending_app_manager_(pending_app_manager),
      host_content_settings_map_(host_content_settings_map),
      pwa_delegate_(std::make_unique<PwaDelegate>()),
      weak_ptr_factory_(this) {}

AndroidSmsAppSetupControllerImpl::~AndroidSmsAppSetupControllerImpl() = default;

void AndroidSmsAppSetupControllerImpl::SetUpApp(const GURL& app_url,
                                                const GURL& install_url,
                                                SuccessCallback callback) {
  PA_LOG(VERBOSE) << "AndroidSmsAppSetupControllerImpl::SetUpApp(): Setting "
                  << "DefaultToPersist cookie at " << app_url << " before PWA "
                  << "installation.";
  pwa_delegate_->GetCookieManager(app_url, profile_)
      ->SetCanonicalCookie(
          *net::CanonicalCookie::CreateSanitizedCookie(
              app_url, kDefaultToPersistCookieName,
              kDefaultToPersistCookieValue, std::string() /* domain */,
              std::string() /* path */, base::Time::Now() /* creation_time */,
              base::Time() /* expiration_time */,
              base::Time::Now() /* last_access_time */,
              !net::IsLocalhost(app_url) /* secure */, false /* http_only */,
              net::CookieSameSite::STRICT_MODE, net::COOKIE_PRIORITY_DEFAULT),
          "https", false /* modify_http_only */,
          base::BindOnce(&AndroidSmsAppSetupControllerImpl::
                             OnSetRememberDeviceByDefaultCookieResult,
                         weak_ptr_factory_.GetWeakPtr(), app_url, install_url,
                         std::move(callback)));
}

const extensions::Extension* AndroidSmsAppSetupControllerImpl::GetPwa(
    const GURL& install_url) {
  return pwa_delegate_->GetPwaForUrl(install_url, profile_);
}

void AndroidSmsAppSetupControllerImpl::DeleteRememberDeviceByDefaultCookie(
    const GURL& app_url,
    SuccessCallback callback) {
  PA_LOG(INFO) << "AndroidSmsAppSetupControllerImpl::"
               << "DeleteRememberDeviceByDefaultCookie(): Deleting "
               << "DefaultToPersist cookie at " << app_url << ".";
  network::mojom::CookieDeletionFilterPtr filter(
      network::mojom::CookieDeletionFilter::New());
  filter->url = app_url;
  filter->cookie_name = kDefaultToPersistCookieName;
  pwa_delegate_->GetCookieManager(app_url, profile_)
      ->DeleteCookies(
          std::move(filter),
          base::BindOnce(&AndroidSmsAppSetupControllerImpl::
                             OnDeleteRememberDeviceByDefaultCookieResult,
                         weak_ptr_factory_.GetWeakPtr(), app_url,
                         std::move(callback)));
}

void AndroidSmsAppSetupControllerImpl::RemoveApp(
    const GURL& app_url,
    const GURL& install_url,
    const GURL& migrated_to_app_url,
    SuccessCallback callback) {
  // If there is no app installed at |url|, there is nothing more to do.
  if (!pwa_delegate_->GetPwaForUrl(install_url, profile_)) {
    PA_LOG(VERBOSE) << "AndroidSmsAppSetupControllerImpl::RemoveApp(): No app "
                    << "is installed at " << install_url
                    << "; skipping removal "
                    << "process.";
    std::move(callback).Run(true /* success */);
    return;
  }

  PA_LOG(INFO) << "AndroidSmsAppSetupControllerImpl::RemoveApp(): "
               << "Uninstalling app at " << install_url << ".";
  // UninstallApps() takes a base::RepeatedCallback, but |callback| is a
  // base::OnceCallback; thus, |callback| cannot be included in the closure
  // because it has move-only semantics. Assign this uninstall attempt an ID
  // associated with |callback| so that it can be retrieved in
  // OnAppUninstallResult().
  auto id = base::UnguessableToken::Create();
  uninstall_id_to_callback_map_.emplace(id, std::move(callback));
  pending_app_manager_->UninstallApps(
      std::vector<GURL>{install_url},
      base::BindRepeating(
          &AndroidSmsAppSetupControllerImpl::OnAppUninstallResult,
          weak_ptr_factory_.GetWeakPtr(), id, app_url, migrated_to_app_url));
}

void AndroidSmsAppSetupControllerImpl::OnSetRememberDeviceByDefaultCookieResult(
    const GURL& app_url,
    const GURL& install_url,
    SuccessCallback callback,
    bool succeeded) {
  if (!succeeded) {
    PA_LOG(WARNING)
        << "AndroidSmsAppSetupControllerImpl::"
        << "OnSetRememberDeviceByDefaultCookieResult(): Failed to set "
        << "DefaultToPersist cookie at " << app_url << ". Proceeding "
        << "to remove migration cookie.";
  }

  // Delete migration cookie in case it was set by a previous RemoveApp call.
  network::mojom::CookieDeletionFilterPtr filter(
      network::mojom::CookieDeletionFilter::New());
  filter->url = app_url;
  filter->cookie_name = kMigrationCookieName;
  pwa_delegate_->GetCookieManager(app_url, profile_)
      ->DeleteCookies(
          std::move(filter),
          base::BindOnce(
              &AndroidSmsAppSetupControllerImpl::OnDeleteMigrationCookieResult,
              weak_ptr_factory_.GetWeakPtr(), app_url, install_url,
              std::move(callback)));
}

void AndroidSmsAppSetupControllerImpl::OnDeleteMigrationCookieResult(
    const GURL& app_url,
    const GURL& install_url,
    SuccessCallback callback,
    uint32_t num_deleted) {
  // If the app is already installed at |url|, there is nothing more to do.
  if (pwa_delegate_->GetPwaForUrl(install_url, profile_)) {
    PA_LOG(VERBOSE) << "AndroidSmsAppSetupControllerImpl::"
                    << "OnDeleteMigrationCookieResult():"
                    << "App is already installed at " << install_url
                    << "; skipping setup process.";
    std::move(callback).Run(true /* success */);
    return;
  }

  web_app::PendingAppManager::AppInfo info(install_url,
                                           web_app::LaunchContainer::kWindow,
                                           web_app::InstallSource::kInternal);
  info.override_previous_user_uninstall = true;
  // The ServiceWorker does not load in time for the installability check, so
  // bypass it as a workaround.
  info.bypass_service_worker_check = true;
  info.require_manifest = true;

  PA_LOG(VERBOSE) << "AndroidSmsAppSetupControllerImpl::OnSetCookieResult(): "
                  << "Installing PWA for " << install_url << ".";
  pending_app_manager_->Install(
      std::move(info),
      base::BindOnce(&AndroidSmsAppSetupControllerImpl::OnAppInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     app_url));
}

void AndroidSmsAppSetupControllerImpl::OnAppInstallResult(
    SuccessCallback callback,
    const GURL& app_url,
    const GURL& install_url,
    web_app::InstallResultCode code) {
  UMA_HISTOGRAM_ENUMERATION("AndroidSms.PWAInstallationResult", code);

  if (code != web_app::InstallResultCode::kSuccess) {
    PA_LOG(WARNING)
        << "AndroidSmsAppSetupControllerImpl::OnAppInstallResult(): "
        << "PWA for " << install_url << " failed to install. "
        << "InstallResultCode: " << static_cast<int>(code);
    std::move(callback).Run(false /* success */);
    return;
  }

  PA_LOG(INFO) << "AndroidSmsAppSetupControllerImpl::OnAppInstallResult(): "
               << "PWA for " << install_url << " was installed successfully.";

  // Grant notification permission for the PWA.
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      app_url, GURL() /* top_level_url */,
      ContentSettingsType::CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      content_settings::ResourceIdentifier(),
      std::make_unique<base::Value>(ContentSetting::CONTENT_SETTING_ALLOW));

  std::move(callback).Run(true /* success */);
}

void AndroidSmsAppSetupControllerImpl::OnAppUninstallResult(
    const base::UnguessableToken& id,
    const GURL& app_url,
    const GURL& migrated_to_app_url,
    const GURL& install_url,
    bool succeeded) {
  UMA_HISTOGRAM_BOOLEAN("AndroidSms.PWAUninstallationResult", succeeded);

  // OnAppUninstallResult() should only be called once per ID, so the uninstall
  // callback is always expected to exist in the map.
  SuccessCallback callback = std::move(uninstall_id_to_callback_map_[id]);
  CHECK(callback);
  uninstall_id_to_callback_map_.erase(id);

  if (!succeeded) {
    PA_LOG(ERROR)
        << "AndroidSmsAppSetupControllerImpl::OnAppUninstallResult(): "
        << "PWA for " << install_url << " failed to uninstall.";
    std::move(callback).Run(false /* success */);
    return;
  }

  // Set migration cookie on the client for which the PWA was just uninstalled.
  // The client checks for this cookie to redirect users to the new domain. This
  // prevents unwanted connection stealing between old and new clients should
  // the user try to open old client.
  pwa_delegate_->GetCookieManager(app_url, profile_)
      ->SetCanonicalCookie(
          *net::CanonicalCookie::CreateSanitizedCookie(
              app_url, kMigrationCookieName, migrated_to_app_url.GetContent(),
              std::string() /* domain */, std::string() /* path */,
              base::Time::Now() /* creation_time */,
              base::Time() /* expiration_time */,
              base::Time::Now() /* last_access_time */,
              !net::IsLocalhost(app_url) /* secure */, false /* http_only */,
              net::CookieSameSite::STRICT_MODE, net::COOKIE_PRIORITY_DEFAULT),
          "https", false /* modify_http_only */,
          base::BindOnce(
              &AndroidSmsAppSetupControllerImpl::OnSetMigrationCookieResult,
              weak_ptr_factory_.GetWeakPtr(), app_url, std::move(callback)));
}

void AndroidSmsAppSetupControllerImpl::OnSetMigrationCookieResult(
    const GURL& app_url,
    SuccessCallback callback,
    bool succeeded) {
  if (!succeeded) {
    PA_LOG(ERROR)
        << "AndroidSmsAppSetupControllerImpl::OnSetMigrationCookieResult(): "
        << "Failed to set migration cookie for " << app_url << ". Proceeding "
        << "to remove DefaultToPersist cookie.";
  }

  DeleteRememberDeviceByDefaultCookie(app_url, std::move(callback));
}

void AndroidSmsAppSetupControllerImpl::
    OnDeleteRememberDeviceByDefaultCookieResult(const GURL& app_url,
                                                SuccessCallback callback,
                                                uint32_t num_deleted) {
  if (num_deleted != 1u) {
    PA_LOG(WARNING) << "AndroidSmsAppSetupControllerImpl::"
                    << "OnDeleteRememberDeviceByDefaultCookieResult(): "
                    << "Tried to delete a single cookie at " << app_url
                    << ", but " << num_deleted << " cookies were deleted.";
  }

  // Even if an unexpected number of cookies was deleted, consider this a
  // success. If SetUpApp() failed to install a cookie earlier, the setup
  // process is still considered a success, so failing to delete a cookie should
  // also be considered a success.
  std::move(callback).Run(true /* success */);
}

void AndroidSmsAppSetupControllerImpl::SetPwaDelegateForTesting(
    std::unique_ptr<PwaDelegate> test_pwa_delegate) {
  pwa_delegate_ = std::move(test_pwa_delegate);
}

}  // namespace android_sms

}  // namespace chromeos
