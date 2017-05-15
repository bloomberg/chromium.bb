// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_upstart_client.h"

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_auth_policy_client.h"
#include "chromeos/dbus/fake_media_analytics_client.h"

namespace chromeos {

FakeUpstartClient::FakeUpstartClient() {}

FakeUpstartClient::~FakeUpstartClient() {}

void FakeUpstartClient::Init(dbus::Bus* bus) {}

void FakeUpstartClient::StartAuthPolicyService() {
  static_cast<FakeAuthPolicyClient*>(
      DBusThreadManager::Get()->GetAuthPolicyClient())
      ->set_started(true);
}

void FakeUpstartClient::RestartAuthPolicyService() {
  FakeAuthPolicyClient* authpolicy_client = static_cast<FakeAuthPolicyClient*>(
      DBusThreadManager::Get()->GetAuthPolicyClient());
  DLOG_IF(WARNING, !authpolicy_client->started())
      << "Trying to restart authpolicyd which is not started";
  authpolicy_client->set_started(true);
}

void FakeUpstartClient::StartMediaAnalytics(const UpstartCallback& callback) {
  FakeMediaAnalyticsClient* media_analytics_client =
      static_cast<FakeMediaAnalyticsClient*>(
          DBusThreadManager::Get()->GetMediaAnalyticsClient());
  DLOG_IF(WARNING, media_analytics_client->process_running())
      << "Trying to start media analytics which is already started.";
  media_analytics_client->set_process_running(true);
  callback.Run(true);
}

void FakeUpstartClient::StopMediaAnalytics() {
  FakeMediaAnalyticsClient* media_analytics_client =
      static_cast<FakeMediaAnalyticsClient*>(
          DBusThreadManager::Get()->GetMediaAnalyticsClient());
  DLOG_IF(WARNING, !media_analytics_client->process_running())
      << "Trying to stop media analytics which is not started.";
  media_analytics_client->set_process_running(false);
}

}  // namespace chromeos
