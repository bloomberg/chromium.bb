// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/log_router.h"

#include "base/stl_util.h"
#include "components/password_manager/core/browser/log_receiver.h"
#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

LogRouter::LogRouter() {
}

LogRouter::~LogRouter() {
}

void LogRouter::ProcessLog(const std::string& text) {
  // This may not be called when there are no receivers (i.e., the router is
  // inactive), because in that case the logs cannot be displayed.
  DCHECK(receivers_.might_have_observers());
  accumulated_logs_.append(text);
  FOR_EACH_OBSERVER(
      LogReceiver, receivers_, LogSavePasswordProgress(text));
}

bool LogRouter::RegisterClient(PasswordManagerClient* client) {
  DCHECK(client);
  clients_.AddObserver(client);
  return receivers_.might_have_observers();
}

void LogRouter::UnregisterClient(PasswordManagerClient* client) {
  DCHECK(clients_.HasObserver(client));
  clients_.RemoveObserver(client);
}

std::string LogRouter::RegisterReceiver(LogReceiver* receiver) {
  DCHECK(receiver);
  DCHECK(accumulated_logs_.empty() || receivers_.might_have_observers());

  if (!receivers_.might_have_observers()) {
    FOR_EACH_OBSERVER(
        PasswordManagerClient, clients_, OnLogRouterAvailabilityChanged(true));
  }
  receivers_.AddObserver(receiver);
  return accumulated_logs_;
}

void LogRouter::UnregisterReceiver(LogReceiver* receiver) {
  DCHECK(receivers_.HasObserver(receiver));
  receivers_.RemoveObserver(receiver);
  if (!receivers_.might_have_observers()) {
    accumulated_logs_.clear();
    FOR_EACH_OBSERVER(
        PasswordManagerClient, clients_, OnLogRouterAvailabilityChanged(false));
  }
}

}  // namespace password_manager
