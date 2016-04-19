// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_chromeos.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/session/chrome_session_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/automatic_reboot_manager.h"
#include "chrome/browser/chromeos/system/device_disabling_manager.h"
#include "chrome/browser/chromeos/system/device_disabling_manager_default_delegate.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/geolocation/simple_geolocation_provider.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"

BrowserProcessPlatformPart::BrowserProcessPlatformPart()
    : created_profile_helper_(false) {
}

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() {
}

void BrowserProcessPlatformPart::InitializeAutomaticRebootManager() {
  DCHECK(!automatic_reboot_manager_);

  automatic_reboot_manager_.reset(new chromeos::system::AutomaticRebootManager(
      std::unique_ptr<base::TickClock>(new base::DefaultTickClock)));
}

void BrowserProcessPlatformPart::ShutdownAutomaticRebootManager() {
  automatic_reboot_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeChromeUserManager() {
  DCHECK(!chrome_user_manager_);
  chrome_user_manager_ =
      chromeos::ChromeUserManagerImpl::CreateChromeUserManager();
  chrome_user_manager_->Initialize();
}

void BrowserProcessPlatformPart::DestroyChromeUserManager() {
  chrome_user_manager_->Destroy();
  chrome_user_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeDeviceDisablingManager() {
  DCHECK(!device_disabling_manager_);

  device_disabling_manager_delegate_.reset(
      new chromeos::system::DeviceDisablingManagerDefaultDelegate);
  device_disabling_manager_.reset(new chromeos::system::DeviceDisablingManager(
      device_disabling_manager_delegate_.get(),
      chromeos::CrosSettings::Get(),
      user_manager::UserManager::Get()));
}

void BrowserProcessPlatformPart::ShutdownDeviceDisablingManager() {
  device_disabling_manager_.reset();
  device_disabling_manager_delegate_.reset();
}

void BrowserProcessPlatformPart::InitializeSessionManager(
    const base::CommandLine& parsed_command_line,
    Profile* profile,
    bool is_running_test) {
  DCHECK(!session_manager_);
  session_manager_ = chromeos::ChromeSessionManager::CreateSessionManager(
      parsed_command_line, profile, is_running_test);
}

void BrowserProcessPlatformPart::ShutdownSessionManager() {
  session_manager_.reset();
}

session_manager::SessionManager* BrowserProcessPlatformPart::SessionManager() {
  DCHECK(CalledOnValidThread());
  return session_manager_.get();
}

void BrowserProcessPlatformPart::RegisterKeepAlive() {
  DCHECK(!keep_alive_);
  keep_alive_.reset(
      new ScopedKeepAlive(KeepAliveOrigin::BROWSER_PROCESS_CHROMEOS,
                          KeepAliveRestartOption::DISABLED));
}

void BrowserProcessPlatformPart::UnregisterKeepAlive() {
  keep_alive_.reset();
}

chromeos::ProfileHelper* BrowserProcessPlatformPart::profile_helper() {
  DCHECK(CalledOnValidThread());
  if (!created_profile_helper_)
    CreateProfileHelper();
  return profile_helper_.get();
}

policy::BrowserPolicyConnectorChromeOS*
BrowserProcessPlatformPart::browser_policy_connector_chromeos() {
  return static_cast<policy::BrowserPolicyConnectorChromeOS*>(
      g_browser_process->browser_policy_connector());
}

chromeos::system::TimeZoneResolverManager*
BrowserProcessPlatformPart::GetTimezoneResolverManager() {
  if (!timezone_resolver_manager_.get()) {
    timezone_resolver_manager_.reset(
        new chromeos::system::TimeZoneResolverManager());
  }
  return timezone_resolver_manager_.get();
}

chromeos::TimeZoneResolver* BrowserProcessPlatformPart::GetTimezoneResolver() {
  if (!timezone_resolver_.get()) {
    timezone_resolver_.reset(new chromeos::TimeZoneResolver(
        GetTimezoneResolverManager(),
        g_browser_process->system_request_context(),
        chromeos::SimpleGeolocationProvider::DefaultGeolocationProviderURL(),
        base::Bind(&chromeos::system::ApplyTimeZone),
        base::Bind(&chromeos::DelayNetworkCall,
                   base::TimeDelta::FromMilliseconds(
                       chromeos::kDefaultNetworkRetryDelayMS)),
        g_browser_process->local_state()));
  }
  return timezone_resolver_.get();
}

chromeos::system::SystemClock* BrowserProcessPlatformPart::GetSystemClock() {
  if (!system_clock_.get())
    system_clock_.reset(new chromeos::system::SystemClock());

  return system_clock_.get();
}
void BrowserProcessPlatformPart::StartTearDown() {
  // interactive_ui_tests check for memory leaks before this object is
  // destroyed.  So we need to destroy |timezone_resolver_| here.
  timezone_resolver_.reset();
  profile_helper_.reset();
}

std::unique_ptr<policy::BrowserPolicyConnector>
BrowserProcessPlatformPart::CreateBrowserPolicyConnector() {
  return std::unique_ptr<policy::BrowserPolicyConnector>(
      new policy::BrowserPolicyConnectorChromeOS());
}

void BrowserProcessPlatformPart::CreateProfileHelper() {
  DCHECK(!created_profile_helper_ && profile_helper_.get() == NULL);
  created_profile_helper_ = true;
  profile_helper_.reset(new chromeos::ProfileHelper());
}
