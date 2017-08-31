// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_DISCONNECT_TETHERING_REQUEST_SENDER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_DISCONNECT_TETHERING_REQUEST_SENDER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/components/tether/disconnect_tethering_request_sender.h"

namespace chromeos {

namespace tether {

// Test double for DisconnectTetheringRequestSender.
class FakeDisconnectTetheringRequestSender
    : public DisconnectTetheringRequestSender {
 public:
  FakeDisconnectTetheringRequestSender();
  ~FakeDisconnectTetheringRequestSender() override;

  // DisconnectTetheringRequestSender:
  void SendDisconnectRequestToDevice(const std::string& device_id) override;
  bool HasPendingRequests() override;

  void NotifyPendingDisconnectRequestsComplete();

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDisconnectTetheringRequestSender);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_DISCONNECT_TETHERING_REQUEST_SENDER_H_
