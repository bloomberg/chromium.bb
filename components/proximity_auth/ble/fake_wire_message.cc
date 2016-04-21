// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/fake_wire_message.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/proximity_auth/wire_message.h"

namespace proximity_auth {

FakeWireMessage::FakeWireMessage(const std::string& payload)
    : WireMessage(payload) {}

std::unique_ptr<FakeWireMessage> FakeWireMessage::Deserialize(
    const std::string& serialized_message,
    bool* is_incomplete_message) {
  *is_incomplete_message = false;
  return std::unique_ptr<FakeWireMessage>(
      new FakeWireMessage(serialized_message));
}

std::string FakeWireMessage::Serialize() const {
  return std::string(payload());
}
}
