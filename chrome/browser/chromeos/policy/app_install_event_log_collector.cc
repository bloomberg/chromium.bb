// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_log_collector.h"
#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

std::unique_ptr<em::AppInstallReportLogEvent> CreateSessionChangeEvent(
    em::AppInstallReportLogEvent::SessionStateChangeType type) {
  std::unique_ptr<em::AppInstallReportLogEvent> event =
      std::make_unique<em::AppInstallReportLogEvent>();
  event->set_event_type(em::AppInstallReportLogEvent::SESSION_STATE_CHANGE);
  event->set_session_state_change_type(type);
  return event;
}

}  // namespace

AppInstallEventLogCollector::AppInstallEventLogCollector(
    Delegate* delegate,
    Profile* profile,
    const std::set<std::string>& pending_packages)
    : delegate_(delegate), profile_(profile) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
}

AppInstallEventLogCollector::~AppInstallEventLogCollector() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

void AppInstallEventLogCollector::OnPendingPackagesChanged(
    const std::set<std::string>& added,
    const std::set<std::string>& removed) {}

void AppInstallEventLogCollector::AddLoginEvent() {
  // Don't log in case session is restared or recovered from crash.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kLoginUser) ||
      profile_->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
    return;
  }

  delegate_->AddForAllPackages(
      CreateSessionChangeEvent(em::AppInstallReportLogEvent::LOGIN));
}

void AppInstallEventLogCollector::AddLogoutEvent() {
  delegate_->AddForAllPackages(
      CreateSessionChangeEvent(em::AppInstallReportLogEvent::LOGOUT));
}

void AppInstallEventLogCollector::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  delegate_->AddForAllPackages(
      CreateSessionChangeEvent(em::AppInstallReportLogEvent::SUSPEND));
}

void AppInstallEventLogCollector::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  delegate_->AddForAllPackages(
      CreateSessionChangeEvent(em::AppInstallReportLogEvent::RESUME));
}

}  // namespace policy
