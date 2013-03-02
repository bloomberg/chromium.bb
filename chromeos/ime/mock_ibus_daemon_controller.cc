// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/mock_ibus_daemon_controller.h"

namespace chromeos {

MockIBusDaemonController::MockIBusDaemonController()
  : start_count_(0),
    stop_count_(0) {
}

MockIBusDaemonController::~MockIBusDaemonController() {
}

void MockIBusDaemonController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockIBusDaemonController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool MockIBusDaemonController::Start() {
  ++start_count_;
  return true;
}

bool MockIBusDaemonController::Stop() {
  ++stop_count_;
  return true;
}

void MockIBusDaemonController::EmulateConnect() {
  FOR_EACH_OBSERVER(Observer, observers_, OnConnected());
}

void MockIBusDaemonController::EmulateDisconnect() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDisconnected());
}


}  // namespace chromeos
