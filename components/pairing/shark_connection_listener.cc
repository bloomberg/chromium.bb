// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pairing/shark_connection_listener.h"

#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "components/pairing/bluetooth_host_pairing_controller.h"

namespace pairing_chromeos {

SharkConnectionListener::SharkConnectionListener(OnConnectedCallback callback)
    : callback_(callback) {
  controller_.reset(new BluetoothHostPairingController());
  controller_->AddObserver(this);
  controller_->StartPairing();
}

SharkConnectionListener::~SharkConnectionListener() {
  if (controller_)
    controller_->RemoveObserver(this);
}

void SharkConnectionListener::PairingStageChanged(Stage new_stage) {
  if (new_stage == HostPairingController::STAGE_WAITING_FOR_CODE_CONFIRMATION) {
    controller_->RemoveObserver(this);
    callback_.Run(controller_.Pass());
    callback_.Reset();
  }
}

void SharkConnectionListener::ConfigureHost(
    bool accepted_eula,
    const std::string& lang,
    const std::string& timezone,
    bool send_reports,
    const std::string& keyboard_layout) {
  NOTREACHED();
}

void SharkConnectionListener::EnrollHost(const std::string& auth_token) {
  NOTREACHED();
}

}  // namespace pairing_chromeos
