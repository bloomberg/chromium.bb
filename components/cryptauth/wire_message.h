// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_WIRE_MESSAGE_H_
#define COMPONENTS_CRYPTAUTH_WIRE_MESSAGE_H_

#include <memory>
#include <string>

#include "base/macros.h"

namespace cryptauth {

class WireMessage {
 public:
  // Creates a WireMessage containing |payload| for feature |feature|.
  explicit WireMessage(const std::string& payload, const std::string& feature);
  virtual ~WireMessage();

  // Returns the deserialized message from |serialized_message|, or nullptr if
  // the message is malformed. Sets |is_incomplete_message| to true if the
  // message does not have enough data to parse the header, or if the message
  // length encoded in the message header exceeds the size of the
  // |serialized_message|.
  static std::unique_ptr<WireMessage> Deserialize(
      const std::string& serialized_message,
      bool* is_incomplete_message);

  // Returns a serialized representation of |this| message.
  virtual std::string Serialize() const;

  const std::string& payload() const { return payload_; }
  const std::string& feature() const { return feature_; }

 private:
  // The message payload.
  const std::string payload_;

  // The feature which sends or intends to receive this message (e.g.,
  // EasyUnlock).
  const std::string feature_;

  DISALLOW_COPY_AND_ASSIGN(WireMessage);
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_WIRE_MESSAGE_H_
