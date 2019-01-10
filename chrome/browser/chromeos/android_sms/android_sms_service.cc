// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_service.h"

#include "base/time/default_clock.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/chromeos/android_sms/connection_establisher_impl.h"
#include "chrome/browser/chromeos/android_sms/connection_manager.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/storage_partition.h"

namespace chromeos {

namespace android_sms {

AndroidSmsService::AndroidSmsService(
    Profile* profile,
    HostContentSettingsMap* host_content_settings_map,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    web_app::WebAppProvider* web_app_provider)
    : profile_(profile),
      multidevice_setup_client_(multidevice_setup_client),
      android_sms_app_helper_delegate_(
          std::make_unique<AndroidSmsAppHelperDelegateImpl>(
              profile_,
              &web_app_provider->pending_app_manager(),
              host_content_settings_map)),
      android_sms_pairing_state_tracker_(
          std::make_unique<AndroidSmsPairingStateTrackerImpl>(profile_)) {
  session_manager::SessionManager::Get()->AddObserver(this);
}

AndroidSmsService::~AndroidSmsService() = default;

void AndroidSmsService::Shutdown() {
  connection_manager_.reset();
  android_sms_app_helper_delegate_.reset();
  android_sms_pairing_state_tracker_.reset();
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void AndroidSmsService::OnSessionStateChanged() {
  // At most one ConnectionManager should be created.
  if (connection_manager_)
    return;

  // ConnectionManager should not be created for blocked sessions.
  if (session_manager::SessionManager::Get()->IsUserSessionBlocked())
    return;

  connection_manager_ = std::make_unique<ConnectionManager>(
      GetStoragePartitionForAndroidMessagesURL(profile_)
          ->GetServiceWorkerContext(),
      std::make_unique<ConnectionEstablisherImpl>(
          base::DefaultClock::GetInstance()),
      multidevice_setup_client_);
}

}  // namespace android_sms

}  // namespace chromeos
