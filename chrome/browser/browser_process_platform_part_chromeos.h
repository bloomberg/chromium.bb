// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/browser/browser_process_platform_part_base.h"
#include "services/ui/common/image_cursors_set.h"

namespace chromeos {
class ChromeSessionManager;
class ChromeUserManager;
class ProfileHelper;
class TimeZoneResolver;
}

namespace chromeos {
namespace system {
class AutomaticRebootManager;
class DeviceDisablingManager;
class DeviceDisablingManagerDefaultDelegate;
class SystemClock;
class TimeZoneResolverManager;
}
}

namespace policy {
class BrowserPolicyConnector;
class BrowserPolicyConnectorChromeOS;
}

namespace ui {
class InputDeviceControllerClient;
}

class ScopedKeepAlive;

class BrowserProcessPlatformPart : public BrowserProcessPlatformPartBase {
 public:
  BrowserProcessPlatformPart();
  ~BrowserProcessPlatformPart() override;

  void InitializeAutomaticRebootManager();
  void ShutdownAutomaticRebootManager();

  void InitializeChromeUserManager();
  void DestroyChromeUserManager();

  void InitializeDeviceDisablingManager();
  void ShutdownDeviceDisablingManager();

  void InitializeSessionManager();
  void ShutdownSessionManager();

  // Disable the offline interstitial easter egg if the device is enterprise
  // enrolled.
  void DisableDinoEasterEggIfEnrolled();

  // Used to register a KeepAlive when Ash is initialized, and release it
  // when until Chrome starts exiting. Ensure we stay running the whole time.
  void RegisterKeepAlive();
  void UnregisterKeepAlive();

  // Returns the ProfileHelper instance that is used to identify
  // users and their profiles in Chrome OS multi user session.
  chromeos::ProfileHelper* profile_helper();

  chromeos::system::AutomaticRebootManager* automatic_reboot_manager() {
    return automatic_reboot_manager_.get();
  }

  policy::BrowserPolicyConnectorChromeOS* browser_policy_connector_chromeos();

  chromeos::ChromeSessionManager* session_manager() {
    return session_manager_.get();
  }

  chromeos::ChromeUserManager* user_manager() {
    return chrome_user_manager_.get();
  }

  chromeos::system::DeviceDisablingManager* device_disabling_manager() {
    return device_disabling_manager_.get();
  }

  chromeos::system::TimeZoneResolverManager* GetTimezoneResolverManager();

  chromeos::TimeZoneResolver* GetTimezoneResolver();

  // Overridden from BrowserProcessPlatformPartBase:
  void StartTearDown() override;
  std::unique_ptr<policy::BrowserPolicyConnector> CreateBrowserPolicyConnector()
      override;
  void RegisterInProcessServices(
      content::ContentBrowserClient::StaticServiceMap* services) override;

  chromeos::system::SystemClock* GetSystemClock();
  void DestroySystemClock();

  void AddCompatibleCrOSComponent(const std::string& name);

  bool IsCompatibleCrOSComponent(const std::string& name);

#if defined(USE_OZONE)
  ui::InputDeviceControllerClient* GetInputDeviceControllerClient();
#endif

 private:
  void CreateProfileHelper();

  std::unique_ptr<chromeos::ChromeSessionManager> session_manager_;

  bool created_profile_helper_;
  std::unique_ptr<chromeos::ProfileHelper> profile_helper_;

  std::unique_ptr<chromeos::system::AutomaticRebootManager>
      automatic_reboot_manager_;

  std::unique_ptr<chromeos::ChromeUserManager> chrome_user_manager_;

  std::unique_ptr<chromeos::system::DeviceDisablingManagerDefaultDelegate>
      device_disabling_manager_delegate_;
  std::unique_ptr<chromeos::system::DeviceDisablingManager>
      device_disabling_manager_;

  std::unique_ptr<chromeos::system::TimeZoneResolverManager>
      timezone_resolver_manager_;
  std::unique_ptr<chromeos::TimeZoneResolver> timezone_resolver_;

  std::unique_ptr<chromeos::system::SystemClock> system_clock_;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  base::flat_set<std::string> compatible_cros_components_;

  // Used by the UI Service.
  ui::ImageCursorsSet image_cursors_set_;

#if defined(USE_OZONE)
  std::unique_ptr<ui::InputDeviceControllerClient>
      input_device_controller_client_;
#endif

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessPlatformPart);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_
