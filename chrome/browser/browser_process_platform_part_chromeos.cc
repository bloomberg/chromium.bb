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
#include "chrome/browser/chromeos/memory/oom_priority_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system/automatic_reboot_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/session_manager/core/session_manager.h"

BrowserProcessPlatformPart::BrowserProcessPlatformPart()
    : created_profile_helper_(false) {
}

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() {
}

void BrowserProcessPlatformPart::InitializeAutomaticRebootManager() {
  DCHECK(!automatic_reboot_manager_);

  automatic_reboot_manager_.reset(new chromeos::system::AutomaticRebootManager(
      scoped_ptr<base::TickClock>(new base::DefaultTickClock)));
}

void BrowserProcessPlatformPart::ShutdownAutomaticRebootManager() {
  automatic_reboot_manager_.reset();
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

chromeos::OomPriorityManager*
    BrowserProcessPlatformPart::oom_priority_manager() {
  DCHECK(CalledOnValidThread());
  if (!oom_priority_manager_.get())
    oom_priority_manager_.reset(new chromeos::OomPriorityManager());
  return oom_priority_manager_.get();
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

void BrowserProcessPlatformPart::StartTearDown() {
  profile_helper_.reset();
}

scoped_ptr<policy::BrowserPolicyConnector>
BrowserProcessPlatformPart::CreateBrowserPolicyConnector() {
  return scoped_ptr<policy::BrowserPolicyConnector>(
      new policy::BrowserPolicyConnectorChromeOS());
}

void BrowserProcessPlatformPart::CreateProfileHelper() {
  DCHECK(!created_profile_helper_ && profile_helper_.get() == NULL);
  created_profile_helper_ = true;
  profile_helper_.reset(new chromeos::ProfileHelper());
}
