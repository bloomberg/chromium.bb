// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_DEVICE_TO_DEVICE_INITIATOR_OPERATIONS_H_
#define COMPONENTS_CRYPTAUTH_DEVICE_TO_DEVICE_INITIATOR_OPERATIONS_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"

namespace cryptauth {

class SecureMessageDelegate;

// Utility class containing operations in the DeviceToDevice protocol that the
// initiator needs to perform. For Smart Lock, in which a phone unlocks a
// laptop, the initiator is laptop.
//
// All operations are asynchronous because we use the SecureMessageDelegate for
// crypto operations, whose implementation may be asynchronous.
//
// In the DeviceToDevice protocol, the initiator needs to send two messages to
// the responder and parse one message from the responder:
//   1. Send [Hello] Message
//      This message contains a public key that the initiator generates for the
//      current session. This message is signed by the long term symmetric key.
//   2. Parse [Responder Auth] Message
//      The responder parses [Hello] and sends this message, which contains the
//      responder's session public key. This message also contains sufficient
//      information for the initiator to authenticate the responder.
//   3. Send [Initiator Auth] Message
//      After receiving the responder's session public key, the initiator crafts
//      and sends this message so the responder can authenticate the initiator.
class DeviceToDeviceInitiatorOperations {
 public:
  // Callback for operations that create a message. Invoked with the serialized
  // SecureMessage upon success or the empty string upon failure.
  typedef base::Callback<void(const std::string&)> MessageCallback;

  // Callback for ValidateResponderAuthMessage. The first argument will be
  // called with the validation outcome. If validation succeeded, then the
  // second argument will contain the session symmetric key derived from the
  // [Responder Auth] message.
  typedef base::Callback<void(bool, const std::string&)>
      ValidateResponderAuthCallback;

  // Creates the [Hello] message, which is the first message that is sent:
  // |session_public_key|: This session public key will be stored in plaintext
  //     (but signed) so the responder can parse it.
  // |persistent_symmetric_key|: The long-term symmetric key that is shared by
  //     the initiator and responder.
  // |secure_message_delegate|: Delegate for SecureMessage operations. This
  //     instance is not owned, and must live until after |callback| is invoked.
  // |callback|: Invoked upon operation completion with the serialized message
  //     or an empty string.
  static void CreateHelloMessage(
      const std::string& session_public_key,
      const std::string& persistent_symmetric_key,
      SecureMessageDelegate* secure_message_delegate,
      const MessageCallback& callback);

  // Validates that the [Responder Auth] message, received from the responder,
  // is properly signed and encrypted.
  // |responder_auth_message|: The bytes of the [Responder Auth] message to
  //     validate.
  // |persistent_responder_public_key|: The long-term public key possessed by
  //     the responder device.
  // |persistent_symmetric_key|: The long-term symmetric key that is shared by
  //     the initiator and responder.
  // |session_private_key|: The session private key is used in an Diffie-Helmann
  //     key exchange once the responder public key is extracted. The derived
  //     session symmetric key is used in the validation process.
  // |hello_message|: The initial [Hello] message that was sent, which is used
  //     in the signature calculation.
  // |secure_message_delegate|: Delegate for SecureMessage operations. This
  //     instance is not owned, and must live until after |callback| is invoked.
  // |callback|: Invoked upon operation completion with whether
  //     |responder_auth_message| is validated successfully.
  static void ValidateResponderAuthMessage(
      const std::string& responder_auth_message,
      const std::string& persistent_responder_public_key,
      const std::string& persistent_symmetric_key,
      const std::string& session_private_key,
      const std::string& hello_message,
      SecureMessageDelegate* secure_message_delegate,
      const ValidateResponderAuthCallback& callback);

  // Creates the [Initiator Auth] message, which allows the responder to
  // authenticate the initiator:
  // |persistent_symmetric_key|: The long-term symmetric key that is shared by
  //     the initiator and responder.
  // |responder_auth_message|: The [Responder Auth] message sent previously to
  // the responder. These bytes are used in the signature calculation.
  // |secure_message_delegate|: Delegate for SecureMessage operations. This
  //     instance is not owned, and must live until after |callback| is invoked.
  // |callback|: Invoked upon operation completion with the serialized message
  //     or an empty string.
  static void CreateInitiatorAuthMessage(
      const std::string& session_symmetric_key,
      const std::string& persistent_symmetric_key,
      const std::string& responder_auth_message,
      SecureMessageDelegate* secure_message_delegate,
      const MessageCallback& callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DeviceToDeviceInitiatorOperations);
};

}  // cryptauth

#endif  // COMPONENTS_CRYPTAUTH_DEVICE_TO_DEVICE_INITIATOR_OPERATIONS_H_
