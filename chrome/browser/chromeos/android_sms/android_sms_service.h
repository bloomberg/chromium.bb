// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_SERVICE_H_

#include <memory>
#include "chrome/browser/chromeos/android_sms/android_sms_app_helper_delegate_impl.h"
#include "chrome/browser/chromeos/android_sms/android_sms_pairing_state_tracker_impl.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"

class HostContentSettingsMap;
class Profile;

namespace web_app {
class WebAppProvider;
}  // namespace web_app

namespace chromeos {

namespace multidevice_setup {
class AndroidSmsAppHelperDelegate;
class AndroidSmsPairingStateTracker;
class MultiDeviceSetupClient;
}  // namespace multidevice_setup

namespace android_sms {

class ConnectionManager;

// KeyedService which manages Android Messages integration. This service
// has three main responsibilities:
//   (1) Maintaining a connection with the Messages ServiceWorker,
//   (2) Managing installation/launching of the Messages PWA, and
//   (3) Tracking the pairing state of the PWA.
class AndroidSmsService : public KeyedService,
                          public session_manager::SessionManagerObserver {
 public:
  AndroidSmsService(
      Profile* profile,
      HostContentSettingsMap* host_content_settings_map,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      web_app::WebAppProvider* web_app_provider);
  ~AndroidSmsService() override;

  multidevice_setup::AndroidSmsAppHelperDelegate*
  android_sms_app_helper_delegate() {
    return android_sms_app_helper_delegate_.get();
  }

  multidevice_setup::AndroidSmsPairingStateTracker*
  android_sms_pairing_state_tracker() {
    return android_sms_pairing_state_tracker_.get();
  }

 private:
  // KeyedService:
  void Shutdown() override;

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

  Profile* profile_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;

  std::unique_ptr<AndroidSmsAppHelperDelegateImpl>
      android_sms_app_helper_delegate_;
  std::unique_ptr<AndroidSmsPairingStateTrackerImpl>
      android_sms_pairing_state_tracker_;
  std::unique_ptr<ConnectionManager> connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsService);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_SERVICE_H_
