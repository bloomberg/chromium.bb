// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/browser_process_platform_part_base.h"

namespace base {
class CommandLine;
}

namespace chromeos {
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

namespace session_manager {
class SessionManager;
}

class Profile;
class ScopedKeepAlive;

class BrowserProcessPlatformPart : public BrowserProcessPlatformPartBase,
                                   public base::NonThreadSafe {
 public:
  BrowserProcessPlatformPart();
  ~BrowserProcessPlatformPart() override;

  void InitializeAutomaticRebootManager();
  void ShutdownAutomaticRebootManager();

  void InitializeChromeUserManager();
  void DestroyChromeUserManager();

  void InitializeDeviceDisablingManager();
  void ShutdownDeviceDisablingManager();

  void InitializeSessionManager(const base::CommandLine& parsed_command_line,
                                Profile* profile,
                                bool is_running_test);
  void ShutdownSessionManager();

  // Disable the offline interstitial easter egg if the device is enterprise
  // enrolled.
  void DisableDinoEasterEggIfEnrolled();

  // Returns the SessionManager instance that is used to initialize and
  // start user sessions as well as responsible on launching pre-session UI like
  // out-of-box or login.
  virtual session_manager::SessionManager* SessionManager();

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

  chromeos::system::SystemClock* GetSystemClock();

 private:
  void CreateProfileHelper();

  std::unique_ptr<session_manager::SessionManager> session_manager_;

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

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessPlatformPart);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_
