// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_WIRE_MESSAGE_H
#define COMPONENTS_PROXIMITY_AUTH_WIRE_MESSAGE_H

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace proximity_auth {

class WireMessage {
 public:
  virtual ~WireMessage();

  // Returns |true| iff the size of |message_bytes| is at least equal to the
  // message length encoded in the message header. Returns false if the message
  // header is not available.
  static bool IsCompleteMessage(const std::string& message_bytes);

  // Returns the deserialized message from |serialized_message|, or NULL if the
  // message is malformed.
  static scoped_ptr<WireMessage> Deserialize(
      const std::string& serialized_message);

 protected:
  // Visible for tests.
  WireMessage();

 private:
  DISALLOW_COPY_AND_ASSIGN(WireMessage);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_WIRE_MESSAGE_H
