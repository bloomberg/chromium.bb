// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_WIRE_MESSAGE_H
#define COMPONENTS_PROXIMITY_AUTH_WIRE_MESSAGE_H

#include <memory>
#include <string>

#include "base/macros.h"

namespace proximity_auth {

class WireMessage {
 public:
  // Creates a WireMessage containing |payload|.
  explicit WireMessage(const std::string& payload);

  // Creates a WireMessage containing |payload| and |permit_id| in the metadata.
  WireMessage(const std::string& payload, const std::string& permit_id);

  virtual ~WireMessage();

  // Returns the deserialized message from |serialized_message|, or NULL if the
  // message is malformed. Sets |is_incomplete_message| to true if the message
  // does not have enough data to parse the header, or if the message length
  // encoded in the message header exceeds the size of the |serialized_message|.
  static std::unique_ptr<WireMessage> Deserialize(
      const std::string& serialized_message,
      bool* is_incomplete_message);

  // Returns a serialized representation of |this| message.
  virtual std::string Serialize() const;

  const std::string& payload() const { return payload_; }
  const std::string& permit_id() const { return permit_id_; }

 private:
  // The message payload.
  const std::string payload_;

  // Identifier of the permit being used. A permit contains the credentials used
  // to authenticate a device. For example, when sending a WireMessage to the
  // remote device the |permit_id_| indexes a permit possibly containing the
  // public key
  // of the local device or a symmetric key shared between the devices.
  const std::string permit_id_;

  DISALLOW_COPY_AND_ASSIGN(WireMessage);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_WIRE_MESSAGE_H
