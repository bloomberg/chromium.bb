// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/network_state_notifier.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

#include "base/message_loop.h"

namespace chromeos {

NetworkStateNotifier* NetworkStateNotifier::Get() {
  return Singleton<NetworkStateNotifier>::get();
}

NetworkStateNotifier::NetworkStateNotifier()
    : task_factory_(this) {
  state_ = RetrieveState();
}

void NetworkStateNotifier::NetworkChanged(NetworkLibrary* cros) {
  DCHECK(CrosLibrary::Get()->EnsureLoaded());
  // Update the state 1 seconds later using UI thread.
  // See http://crosbug.com/4558
  ChromeThread::PostDelayedTask(
      ChromeThread::UI, FROM_HERE,
      task_factory_.NewRunnableMethod(
          &NetworkStateNotifier::UpdateNetworkState,
          RetrieveState()),
      1000);
}

void NetworkStateNotifier::UpdateNetworkState(
    NetworkStateDetails::State new_state) {
  DLOG(INFO) << "UpdateNetworkState: new="
             << new_state << ", old=" << state_;
  state_ = new_state;
  NetworkStateDetails details(state_);
  NotificationService::current()->Notify(
      NotificationType::NETWORK_STATE_CHANGED,
      NotificationService::AllSources(),
      Details<NetworkStateDetails>(&details));
};

// static
NetworkStateDetails::State NetworkStateNotifier::RetrieveState() {
  // Running on desktop means always connected, for now.
  if (!CrosLibrary::Get()->EnsureLoaded())
    return NetworkStateDetails::CONNECTED;
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if (cros->Connected()) {
    return NetworkStateDetails::CONNECTED;
  } else if (cros->Connecting()) {
    return NetworkStateDetails::CONNECTING;
  } else {
    return NetworkStateDetails::DISCONNECTED;
  }
}


}  // namespace chromeos
