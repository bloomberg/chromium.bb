// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/chromeos_gcm_connection_observer.h"

#include "base/callback.h"
#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"

namespace gcm {

ChromeOSGCMConnectionObserver::ChromeOSGCMConnectionObserver() {
}

ChromeOSGCMConnectionObserver::~ChromeOSGCMConnectionObserver() {
}

// static
void ChromeOSGCMConnectionObserver::ErrorCallback(
    const std::string& error_name,
    const std::string& error) {
  LOG(ERROR) << "GCM D-Bus method error " << error_name << ": " << error;
}

void ChromeOSGCMConnectionObserver::OnConnected(
    const net::IPEndPoint& ip_endpoint) {
  ip_endpoint_ = ip_endpoint;
  chromeos::DBusThreadManager::Get()->
      GetShillManagerClient()->
      AddWakeOnPacketConnection(
          ip_endpoint,
          base::Bind(&base::DoNothing),
          base::Bind(&ChromeOSGCMConnectionObserver::ErrorCallback));
}

void ChromeOSGCMConnectionObserver::OnDisconnected() {
  chromeos::DBusThreadManager::Get()->
      GetShillManagerClient()->
      RemoveWakeOnPacketConnection(
          ip_endpoint_,
          base::Bind(&base::DoNothing),
          base::Bind(&ChromeOSGCMConnectionObserver::ErrorCallback));
}

}  // namespace gcm
