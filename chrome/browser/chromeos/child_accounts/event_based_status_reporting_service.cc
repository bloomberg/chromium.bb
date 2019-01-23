// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/event_based_status_reporting_service.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

EventBasedStatusReportingService::EventBasedStatusReportingService(
    content::BrowserContext* context)
    : context_(context) {
  ArcAppListPrefs* arc_app_prefs = ArcAppListPrefs::Get(context_);
  // arc_app_prefs may not available in some browser tests.
  if (arc_app_prefs)
    arc_app_prefs->AddObserver(this);
  session_manager::SessionManager::Get()->AddObserver(this);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
}

EventBasedStatusReportingService::~EventBasedStatusReportingService() = default;

void EventBasedStatusReportingService::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  RequestStatusReport("Request status report due to an app install.");
}

void EventBasedStatusReportingService::OnPackageModified(
    const arc::mojom::ArcPackageInfo& package_info) {
  RequestStatusReport("Request status report due to an app update.");
}

void EventBasedStatusReportingService::OnSessionStateChanged() {
  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();

  // Do not request status report when the device has just started the session
  // because not all components may be ready yet, which may cause an incomplete
  // status report.
  if (session_just_started_ &&
      session_state == session_manager::SessionState::ACTIVE) {
    session_just_started_ = false;
    return;
  }

  if (session_state == session_manager::SessionState::ACTIVE) {
    RequestStatusReport("Request status report due to a unlock screen.");
  } else if (session_state == session_manager::SessionState::LOCKED) {
    RequestStatusReport("Request status report due to a lock screen.");
  }
}

void EventBasedStatusReportingService::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE)
    RequestStatusReport("Request status report due to device going online.");
}

void EventBasedStatusReportingService::SuspendDone(
    const base::TimeDelta& duration) {
  RequestStatusReport(
      "Request status report after a suspend has been completed.");
}

void EventBasedStatusReportingService::RequestStatusReport(
    const std::string& reason) {
  VLOG(1) << reason;
  ConsumerStatusReportingServiceFactory::GetForBrowserContext(context_)
      ->RequestImmediateStatusReport();
}

void EventBasedStatusReportingService::Shutdown() {
  ArcAppListPrefs* arc_app_prefs = ArcAppListPrefs::Get(context_);
  // arc_app_prefs may not available in some browser tests.
  if (arc_app_prefs)
    arc_app_prefs->RemoveObserver(this);
  session_manager::SessionManager::Get()->RemoveObserver(this);
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

}  // namespace chromeos
