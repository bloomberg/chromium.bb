// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_state_notifier.h"

#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace chromeos {

using base::Time;
using base::TimeDelta;

// static
NetworkStateNotifier* NetworkStateNotifier::Get() {
  return Singleton<NetworkStateNotifier>::get();
}

// static
TimeDelta NetworkStateNotifier::GetOfflineDuration() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  // TODO(oshima): make this instance method so that
  // we can mock this for ui_tests.
  // http://crbug.com/4825 .
  return base::Time::Now() - Get()->offline_start_time_;
}

NetworkStateNotifier::NetworkStateNotifier()
    : ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      state_(RetrieveState()),
      offline_start_time_(Time::Now()) {
}

void NetworkStateNotifier::NetworkChanged(NetworkLibrary* cros) {
  DCHECK(CrosLibrary::Get()->EnsureLoaded());
  // Update the state 2 seconds later using UI thread.
  // See http://crosbug.com/4558
  ChromeThread::PostDelayedTask(
      ChromeThread::UI, FROM_HERE,
      task_factory_.NewRunnableMethod(
          &NetworkStateNotifier::UpdateNetworkState,
          RetrieveState()),
      2000);
}

void NetworkStateNotifier::UpdateNetworkState(
    NetworkStateDetails::State new_state) {
  DLOG(INFO) << "UpdateNetworkState: new="
             << new_state << ", old=" << state_;
  if (state_ == NetworkStateDetails::CONNECTED &&
      new_state != NetworkStateDetails::CONNECTED) {
    offline_start_time_ = Time::Now();
  }

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
