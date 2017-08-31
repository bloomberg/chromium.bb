// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_disconnect_tethering_request_sender.h"

namespace chromeos {

namespace tether {

FakeDisconnectTetheringRequestSender::FakeDisconnectTetheringRequestSender() {}

FakeDisconnectTetheringRequestSender::~FakeDisconnectTetheringRequestSender() {}

void FakeDisconnectTetheringRequestSender::SendDisconnectRequestToDevice(
    const std::string& device_id) {
  // TODO(lesliewatkins): Flesh out.
}

bool FakeDisconnectTetheringRequestSender::HasPendingRequests() {
  // TODO(lesliewatkins): Flesh out.
  return false;
}

void FakeDisconnectTetheringRequestSender::
    NotifyPendingDisconnectRequestsComplete() {
  DisconnectTetheringRequestSender::NotifyPendingDisconnectRequestsComplete();
}

}  // namespace tether

}  // namespace chromeos
