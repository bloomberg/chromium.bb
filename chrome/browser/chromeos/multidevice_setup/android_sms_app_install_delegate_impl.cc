// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/multidevice_setup/android_sms_app_install_delegate_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace multidevice_setup {

AndroidSmsAppInstallDelegateImpl::AndroidSmsAppInstallDelegateImpl(
    Profile* profile)
    : pending_app_manager_(
          &web_app::WebAppProvider::Get(profile)->pending_app_manager()),
      weak_ptr_factory_(this) {}

AndroidSmsAppInstallDelegateImpl::AndroidSmsAppInstallDelegateImpl(
    web_app::PendingAppManager* pending_app_manager)
    : pending_app_manager_(pending_app_manager), weak_ptr_factory_(this) {}

AndroidSmsAppInstallDelegateImpl::~AndroidSmsAppInstallDelegateImpl() = default;

void AndroidSmsAppInstallDelegateImpl::InstallAndroidSmsApp() {
  // TODO(crbug.com/874605): Consider retries and error handling here. This call
  // can easily fail.
  pending_app_manager_->Install(
      web_app::PendingAppManager::AppInfo(
          chromeos::android_sms::GetAndroidMessagesURLWithExperiments(),
          web_app::PendingAppManager::LaunchContainer::kWindow),
      base::BindOnce(&AndroidSmsAppInstallDelegateImpl::OnAppInstalled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AndroidSmsAppInstallDelegateImpl::OnAppInstalled(
    const GURL& app_url,
    const std::string& app_id) {
  PA_LOG(INFO) << "Messages app installed! URL: " << app_url
               << ". Id: " << app_id;
}

}  // namespace multidevice_setup

}  // namespace chromeos
