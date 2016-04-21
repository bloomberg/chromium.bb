// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_FAKE_WIRE_MESSAGE_H
#define COMPONENTS_PROXIMITY_AUTH_FAKE_WIRE_MESSAGE_H

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/proximity_auth/wire_message.h"

namespace proximity_auth {

class FakeWireMessage : public WireMessage {
 public:
  FakeWireMessage(const std::string& payload);

  static std::unique_ptr<FakeWireMessage> Deserialize(
      const std::string& serialized_message,
      bool* is_incomplete_message);

  std::string Serialize() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeWireMessage);
};
}

#endif  // COMPONENTS_PROXIMITY_AUTH_WIRE_MESSAGE_H
