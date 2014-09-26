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

  // Returns the deserialized message from |serialized_message|, or NULL if the
  // message is malformed. Sets |is_incomplete_message| to true if the message
  // does not have enough data to parse the header, or if the message length
  // encoded in the message header exceeds the size of the |serialized_message|.
  static scoped_ptr<WireMessage> Deserialize(
      const std::string& serialized_message,
      bool* is_incomplete_message);

  // Returns a serialized representation of |this| message.
  virtual std::string Serialize() const;

  const std::string& permit_id() const { return permit_id_; }
  const std::string& payload() const { return payload_; }

 protected:
  // Visible for tests.
  WireMessage(const std::string& permit_id, const std::string& payload);

 private:
  // Identifier of the permit being used.
  // TODO(isherman): Describe in a bit more detail.
  const std::string permit_id_;

  // The message payload.
  const std::string payload_;

  DISALLOW_COPY_AND_ASSIGN(WireMessage);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_WIRE_MESSAGE_H
