// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/wire_message.h"

namespace proximity_auth {

WireMessage::~WireMessage() {
}

// static
bool WireMessage::IsCompleteMessage(const std::string& serialized_message) {
  // TODO(isherman): Implement.
  return false;
}

// static
scoped_ptr<WireMessage> WireMessage::Deserialize(
    const std::string& serialized_message) {
  // TODO(isherman): Implement.
  return scoped_ptr<WireMessage>();
}

WireMessage::WireMessage() {
  // TODO(isherman): Implement.
}

}  // namespace proximity_auth
