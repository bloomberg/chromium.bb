// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_SECURE_CONTEXT_H
#define COMPONENTS_PROXIMITY_AUTH_SECURE_CONTEXT_H

namespace proximity_auth {

// An interface used to decode and encode messages.
class SecureContext {
 public:
  // The protocol version used during authentication.
  enum ProtocolVersion {
    PROTOCOL_VERSION_THREE_ZERO,  // 3.0
    PROTOCOL_VERSION_THREE_ONE,   // 3.1
  };

  virtual ~SecureContext() {}

  // Decodes the |encoded_message| and returns the result.
  virtual std::string Decode(const std::string& encoded_message) = 0;

  // Encodes the |message| and returns the result.
  virtual std::string Encode(const std::string& message) = 0;

  // Returns the message received from the remote device that authenticates it.
  // This message should have been received during the handshake that
  // establishes the secure channel.
  virtual std::string GetReceivedAuthMessage() const = 0;

  // Returns the protocol version that was used during authentication.
  virtual ProtocolVersion GetProtocolVersion() const = 0;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_SECURE_CONTEXT_H
