// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_app_helper_delegate_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/constants.h"
#include "ui/base/window_open_disposition.h"

namespace {

const char kDefaultToPersistCookieName[] = "default_to_persist";
const char kDefaultToPersistCookieValue[] = "true";

void OnAppUninstallResult(const GURL& app_url, bool succeeded) {
  UMA_HISTOGRAM_BOOLEAN("AndroidSms.PWAUninstallationResult", succeeded);

  if (succeeded)
    return;

  PA_LOG(ERROR) << "Failed to uninstall messages app; URL: " << app_url;
}

}  // namespace

namespace chromeos {

namespace android_sms {

AndroidSmsAppHelperDelegateImpl::PwaDelegate::PwaDelegate() = default;

AndroidSmsAppHelperDelegateImpl::PwaDelegate::~PwaDelegate() = default;

const extensions::Extension*
AndroidSmsAppHelperDelegateImpl::PwaDelegate::GetPwaForUrl(Profile* profile,
                                                           GURL gurl) {
  return extensions::util::GetInstalledPwaForUrl(profile, gurl);
}

content::WebContents* AndroidSmsAppHelperDelegateImpl::PwaDelegate::OpenApp(
    const AppLaunchParams& params) {
  // Note: OpenApplications() is not namespaced and is defined in
  // application_launch.h.
  return OpenApplication(params);
}

network::mojom::CookieManager*
AndroidSmsAppHelperDelegateImpl::PwaDelegate::GetCookieManager(
    Profile* profile) {
  return GetCookieManagerForAndroidMessagesURL(profile);
}

AndroidSmsAppHelperDelegateImpl::AndroidSmsAppHelperDelegateImpl(
    Profile* profile,
    web_app::PendingAppManager* pending_app_manager,
    HostContentSettingsMap* host_content_settings_map)
    : profile_(profile),
      pending_app_manager_(pending_app_manager),
      host_content_settings_map_(host_content_settings_map),
      pwa_delegate_(std::make_unique<PwaDelegate>()),
      weak_ptr_factory_(this) {}

AndroidSmsAppHelperDelegateImpl::~AndroidSmsAppHelperDelegateImpl() = default;

void AndroidSmsAppHelperDelegateImpl::SetUpAndroidSmsApp(
    bool launch_on_install) {
  // Before setting up the new app, check to see whether an app exists for the
  // old URL (i.e., the URL used before the kUseMessagesGoogleComDomain flag
  // was flipped).
  const GURL old_messages_url =
      chromeos::android_sms::GetAndroidMessagesURLOld();
  const extensions::Extension* old_android_sms_pwa =
      pwa_delegate_->GetPwaForUrl(profile_, old_messages_url);
  if (old_android_sms_pwa) {
    PA_LOG(INFO) << "Messages PWA exists for old URL (" << old_messages_url
                 << "); uninstalling before continuing.";
    pending_app_manager_->UninstallApps(
        std::vector<GURL>{old_messages_url},
        base::BindRepeating(&OnAppUninstallResult));
    TearDownAndroidSmsAppAtUrl(old_messages_url);
  }

  // Now that the old app has been uninstalled, continue.
  SetUpAndroidSmsAppWithNoOldApp(launch_on_install);
}

void AndroidSmsAppHelperDelegateImpl::SetUpAndroidSmsAppWithNoOldApp(
    bool launch_on_install) {
  PA_LOG(INFO) << "Setting DefaultToPersist Cookie";
  pwa_delegate_->GetCookieManager(profile_)->SetCanonicalCookie(
      *net::CanonicalCookie::CreateSanitizedCookie(
          chromeos::android_sms::GetAndroidMessagesURL(),
          kDefaultToPersistCookieName, kDefaultToPersistCookieValue,
          std::string() /* domain */, std::string() /* path */,
          base::Time::Now() /* creation_time */,
          base::Time() /* expiration_time */,
          base::Time::Now() /* last_access_time */, true /* secure */,
          false /* http_only */, net::CookieSameSite::STRICT_MODE,
          net::COOKIE_PRIORITY_DEFAULT),
      true /* secure_source */, false /* modify_http_only */,
      base::BindOnce(&AndroidSmsAppHelperDelegateImpl::
                         OnSetDefaultToPersistCookieForInstall,
                     weak_ptr_factory_.GetWeakPtr(), launch_on_install));
}

void AndroidSmsAppHelperDelegateImpl::OnSetDefaultToPersistCookieForInstall(
    bool launch_on_install,
    bool set_cookie_success) {
  if (!set_cookie_success)
    PA_LOG(WARNING) << "Failed to set default to persist cookie";

  // TODO(crbug.com/874605): Consider retries and error handling here. This call
  // can easily fail.
  web_app::PendingAppManager::AppInfo info(
      chromeos::android_sms::GetAndroidMessagesURL(),
      web_app::LaunchContainer::kWindow, web_app::InstallSource::kInternal);
  info.override_previous_user_uninstall = true;
  // The service worker does not load in time for the installability
  // check so we bypass it as a workaround.
  info.bypass_service_worker_check = true;
  info.require_manifest = true;
  pending_app_manager_->Install(
      std::move(info),
      base::BindOnce(&AndroidSmsAppHelperDelegateImpl::OnAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), launch_on_install));
}

void AndroidSmsAppHelperDelegateImpl::SetUpAndroidSmsApp() {
  SetUpAndroidSmsApp(false /* launch_on_install */);
}

void AndroidSmsAppHelperDelegateImpl::SetUpAndLaunchAndroidSmsApp() {
  const extensions::Extension* android_sms_pwa = pwa_delegate_->GetPwaForUrl(
      profile_, android_sms::GetAndroidMessagesURL());
  if (!android_sms_pwa) {
    PA_LOG(VERBOSE) << "No Messages app found. Installing it.";
    SetUpAndroidSmsApp(true /* launch_on_install */);
    return;
  }

  LaunchAndroidSmsApp();
}

void AndroidSmsAppHelperDelegateImpl::LaunchAndroidSmsApp() {
  const extensions::Extension* android_sms_pwa = pwa_delegate_->GetPwaForUrl(
      profile_, android_sms::GetAndroidMessagesURL());
  DCHECK(android_sms_pwa);

  PA_LOG(VERBOSE) << "Messages app Launching...";
  pwa_delegate_->OpenApp(AppLaunchParams(
      profile_, android_sms_pwa, extensions::LAUNCH_CONTAINER_WINDOW,
      WindowOpenDisposition::NEW_WINDOW, extensions::SOURCE_CHROME_INTERNAL));
}

void AndroidSmsAppHelperDelegateImpl::OnAppInstalled(
    bool launch_on_install,
    const GURL& app_url,
    web_app::InstallResultCode code) {
  UMA_HISTOGRAM_ENUMERATION("AndroidSms.PWAInstallationResult", code);

  if (code == web_app::InstallResultCode::kSuccess) {
    // Pre-Grant notification permission for Messages.
    host_content_settings_map_->SetWebsiteSettingDefaultScope(
        chromeos::android_sms::GetAndroidMessagesURL(),
        GURL() /* top_level_url */,
        ContentSettingsType::CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
        content_settings::ResourceIdentifier(),
        std::make_unique<base::Value>(ContentSetting::CONTENT_SETTING_ALLOW));

    PA_LOG(VERBOSE) << "Messages app installed! URL: " << app_url;
    if (launch_on_install)
      LaunchAndroidSmsApp();
  } else {
    PA_LOG(WARNING) << "Messages app failed to install! URL: " << app_url
                    << ", InstallResultCode: " << static_cast<int>(code);
  }
}

void AndroidSmsAppHelperDelegateImpl::TearDownAndroidSmsApp() {
  TearDownAndroidSmsAppAtUrl(chromeos::android_sms::GetAndroidMessagesURL());
}

void AndroidSmsAppHelperDelegateImpl::TearDownAndroidSmsAppAtUrl(GURL pwa_url) {
  PA_LOG(INFO) << "Clearing DefaultToPersist Cookie";
  network::mojom::CookieDeletionFilterPtr filter(
      network::mojom::CookieDeletionFilter::New());
  filter->url = pwa_url;
  filter->cookie_name = kDefaultToPersistCookieName;
  pwa_delegate_->GetCookieManager(profile_)->DeleteCookies(std::move(filter),
                                                           base::DoNothing());
}

void AndroidSmsAppHelperDelegateImpl::SetPwaDelegateForTesting(
    std::unique_ptr<PwaDelegate> test_pwa_delegate) {
  pwa_delegate_ = std::move(test_pwa_delegate);
}

}  // namespace android_sms

}  // namespace chromeos
