// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_app_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/android_sms/android_sms_app_setup_controller.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "extensions/common/extension.h"

namespace chromeos {

namespace android_sms {

AndroidSmsAppManagerImpl::PwaDelegate::PwaDelegate() = default;

AndroidSmsAppManagerImpl::PwaDelegate::~PwaDelegate() = default;

content::WebContents* AndroidSmsAppManagerImpl::PwaDelegate::OpenApp(
    const AppLaunchParams& params) {
  // Note: OpenApplications() is not namespaced and is defined in
  // application_launch.h.
  return OpenApplication(params);
}

bool AndroidSmsAppManagerImpl::PwaDelegate::TransferItemAttributes(
    const std::string& from_app_id,
    const std::string& to_app_id,
    app_list::AppListSyncableService* app_list_syncable_service) {
  return app_list_syncable_service->TransferItemAttributes(from_app_id,
                                                           to_app_id);
}

AndroidSmsAppManagerImpl::AndroidSmsAppManagerImpl(
    Profile* profile,
    AndroidSmsAppSetupController* setup_controller,
    app_list::AppListSyncableService* app_list_syncable_service,
    scoped_refptr<base::TaskRunner> task_runner)
    : profile_(profile),
      setup_controller_(setup_controller),
      app_list_syncable_service_(app_list_syncable_service),
      installed_url_at_last_notify_(GetInstalledAppUrl()),
      pwa_delegate_(std::make_unique<PwaDelegate>()),
      weak_ptr_factory_(this) {
  // Post a task to complete initialization. This portion of the flow must be
  // posted asynchronously because it accesses the networking stack, which is
  // not completely loaded until after this class is instantiated.
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&AndroidSmsAppManagerImpl::CompleteAsyncInitialization,
                     weak_ptr_factory_.GetWeakPtr()));
}

AndroidSmsAppManagerImpl::~AndroidSmsAppManagerImpl() = default;

base::Optional<GURL> AndroidSmsAppManagerImpl::GetInstalledAppUrl() {
  if (setup_controller_->GetPwa(GetAndroidMessagesURL()))
    return GetAndroidMessagesURL();

  if (setup_controller_->GetPwa(GetAndroidMessagesURLOld()))
    return GetAndroidMessagesURLOld();

  return base::nullopt;
}

void AndroidSmsAppManagerImpl::SetUpAndroidSmsApp() {
  // If setup is already in progress, there is nothing else to do.
  if (is_new_app_setup_in_progress_)
    return;

  is_new_app_setup_in_progress_ = true;
  setup_controller_->SetUpApp(
      GetAndroidMessagesURL(),
      base::BindOnce(&AndroidSmsAppManagerImpl::OnSetUpNewAppResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AndroidSmsAppManagerImpl::SetUpAndLaunchAndroidSmsApp() {
  is_app_launch_pending_ = true;
  SetUpAndroidSmsApp();
}

void AndroidSmsAppManagerImpl::TearDownAndroidSmsApp() {
  base::Optional<GURL> installed_app_url = GetInstalledAppUrl();
  if (!installed_app_url)
    return;

  setup_controller_->DeleteRememberDeviceByDefaultCookie(*installed_app_url,
                                                         base::DoNothing());
}

void AndroidSmsAppManagerImpl::CompleteAsyncInitialization() {
  // If the kUseMessagesGoogleComDomain flag has been flipped and the installed
  // PWA is at the old URL, set up the new app.
  if (GetInstalledAppUrl() == GetAndroidMessagesURLOld())
    SetUpAndroidSmsApp();
}

void AndroidSmsAppManagerImpl::NotifyInstalledAppUrlChangedIfNecessary() {
  base::Optional<GURL> installed_app_url = GetInstalledAppUrl();
  if (installed_url_at_last_notify_ == installed_app_url)
    return;

  installed_url_at_last_notify_ = installed_app_url;
  NotifyInstalledAppUrlChanged();
}

void AndroidSmsAppManagerImpl::OnSetUpNewAppResult(bool success) {
  is_new_app_setup_in_progress_ = false;

  const extensions::Extension* new_pwa =
      setup_controller_->GetPwa(GetAndroidMessagesURL());

  // If the installation succeeded, a PWA should exist at the new URL.
  DCHECK_EQ(success, new_pwa != nullptr);

  // If the app failed to install, it should no longer be launched.
  if (!success) {
    is_app_launch_pending_ = false;
    return;
  }

  const extensions::Extension* old_pwa =
      setup_controller_->GetPwa(GetAndroidMessagesURLOld());

  // If there is no PWA installed at the old URL, no migration is needed and
  // setup is finished.
  if (!old_pwa) {
    HandleAppSetupFinished();
    return;
  }

  // Transfer attributes from the old PWA to the new one. This ensures that the
  // PWA's placement in the app launcher and shelf remains constant..
  bool transfer_attributes_success = pwa_delegate_->TransferItemAttributes(
      old_pwa->id() /* from_app_id */, new_pwa->id() /* to_app_id */,
      app_list_syncable_service_);
  if (!transfer_attributes_success) {
    PA_LOG(ERROR) << "AndroidSmsAppManagerImpl::OnSetUpNewAppResult(): Failed "
                  << "to transfer item attributes from "
                  << GetAndroidMessagesURLOld() << " to "
                  << GetAndroidMessagesURL() << ".";
  }

  // Finish the migration by removing the old app now that it has been replaced.
  setup_controller_->RemoveApp(
      GetAndroidMessagesURLOld(),
      base::BindOnce(&AndroidSmsAppManagerImpl::OnRemoveOldAppResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AndroidSmsAppManagerImpl::OnRemoveOldAppResult(bool success) {
  // If app removal fails, log an error but continue anyway, since clients
  // should still be notified of the URL change.
  if (!success) {
    PA_LOG(ERROR) << "AndroidSmsAppManagerImpl::OnRemoveOldAppResult(): Failed "
                  << "to remove PWA at old URL " << GetAndroidMessagesURLOld()
                  << ".";
  }

  HandleAppSetupFinished();
}

void AndroidSmsAppManagerImpl::HandleAppSetupFinished() {
  NotifyInstalledAppUrlChangedIfNecessary();

  // If no launch was requested, setup is complete.
  if (!is_app_launch_pending_)
    return;

  is_app_launch_pending_ = false;

  // If launch was requested but setup failed, there is no app to launch.
  base::Optional<GURL> installed_app_url = GetInstalledAppUrl();
  if (!installed_app_url)
    return;

  // Otherwise, launch the app.
  PA_LOG(VERBOSE) << "AndroidSmsAppManagerImpl::HandleAppSetupFinished(): "
                  << "Launching Messages PWA.";
  pwa_delegate_->OpenApp(AppLaunchParams(
      profile_, setup_controller_->GetPwa(*installed_app_url),
      extensions::LAUNCH_CONTAINER_WINDOW, WindowOpenDisposition::NEW_WINDOW,
      extensions::SOURCE_CHROME_INTERNAL));
}

void AndroidSmsAppManagerImpl::SetPwaDelegateForTesting(
    std::unique_ptr<PwaDelegate> test_pwa_delegate) {
  pwa_delegate_ = std::move(test_pwa_delegate);
}

}  // namespace android_sms

}  // namespace chromeos
