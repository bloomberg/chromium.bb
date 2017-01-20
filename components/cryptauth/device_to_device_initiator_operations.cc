// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/device_to_device_initiator_operations.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/proto/securemessage.pb.h"
#include "components/cryptauth/secure_message_delegate.h"
#include "components/proximity_auth/logging/logging.h"

namespace cryptauth {

namespace {

// TODO(tengs): Due to a bug with the ChromeOS secure message daemon, we cannot
// create SecureMessages with empty payloads. To workaround this bug, this value
// is put into the payload if it would otherwise be empty.
// See crbug.com/512894.
const char kPayloadFiller[] = "\xae";

// The version to put in the GcmMetadata field.
const int kGcmMetadataVersion = 1;

// Callback for DeviceToDeviceInitiatorOperations::CreateInitiatorAuthMessage(),
// after the inner message is created.
void OnInnerMessageCreatedForInitiatorAuth(
    const std::string& session_symmetric_key,
    SecureMessageDelegate* secure_message_delegate,
    const DeviceToDeviceInitiatorOperations::MessageCallback& callback,
    const std::string& inner_message) {
  if (inner_message.empty()) {
    PA_LOG(INFO) << "Failed to create inner message for [Initiator Auth].";
    callback.Run(std::string());
    return;
  }

  GcmMetadata gcm_metadata;
  gcm_metadata.set_type(DEVICE_TO_DEVICE_MESSAGE);
  gcm_metadata.set_version(kGcmMetadataVersion);

  // Store the inner message inside a DeviceToDeviceMessage proto.
  securemessage::DeviceToDeviceMessage device_to_device_message;
  device_to_device_message.set_message(inner_message);
  device_to_device_message.set_sequence_number(2);

  // Create and return the outer message, which wraps the inner message.
  SecureMessageDelegate::CreateOptions create_options;
  create_options.encryption_scheme = securemessage::AES_256_CBC;
  create_options.signature_scheme = securemessage::HMAC_SHA256;
  gcm_metadata.SerializeToString(&create_options.public_metadata);
  secure_message_delegate->CreateSecureMessage(
      device_to_device_message.SerializeAsString(), session_symmetric_key,
      create_options, callback);
}

// Helper struct containing all the context needed to validate the
// [Responder Auth] message.
struct ValidateResponderAuthMessageContext {
  std::string responder_auth_message;
  std::string persistent_responder_public_key;
  std::string persistent_symmetric_key;
  std::string session_private_key;
  std::string hello_message;
  SecureMessageDelegate* secure_message_delegate;
  DeviceToDeviceInitiatorOperations::ValidateResponderAuthCallback callback;
  std::string responder_session_public_key;
  std::string session_symmetric_key;
};

// Forward declarations of functions used in the [Responder Auth] validation
// flow. These functions are declared in order in which they are called during
// the flow.
void BeginResponderAuthValidation(ValidateResponderAuthMessageContext context);
void OnSessionSymmetricKeyDerived(ValidateResponderAuthMessageContext context,
                                  const std::string& session_symmetric_key);
void OnOuterMessageUnwrappedForResponderAuth(
    const ValidateResponderAuthMessageContext& context,
    bool verified,
    const std::string& payload,
    const securemessage::Header& header);
void OnMiddleMessageUnwrappedForResponderAuth(
    const ValidateResponderAuthMessageContext& context,
    bool verified,
    const std::string& payload,
    const securemessage::Header& header);
void OnInnerMessageUnwrappedForResponderAuth(
    const ValidateResponderAuthMessageContext& context,
    bool verified,
    const std::string& payload,
    const securemessage::Header& header);

// Begins the [Responder Auth] validation flow by validating the header.
void BeginResponderAuthValidation(ValidateResponderAuthMessageContext context) {
  // Parse the encrypted SecureMessage so we can get plaintext data from the
  // header. Note that the payload will be encrypted.
  securemessage::SecureMessage encrypted_message;
  securemessage::HeaderAndBody header_and_body;
  if (!encrypted_message.ParseFromString(context.responder_auth_message) ||
      !header_and_body.ParseFromString(encrypted_message.header_and_body())) {
    PA_LOG(WARNING) << "Failed to parse [Responder Hello] message";
    context.callback.Run(false, std::string());
    return;
  }

  // Check that header public_metadata contains the correct metadata fields.
  securemessage::Header header = header_and_body.header();
  GcmMetadata gcm_metadata;
  if (!gcm_metadata.ParseFromString(header.public_metadata()) ||
      gcm_metadata.type() !=
          DEVICE_TO_DEVICE_RESPONDER_HELLO_PAYLOAD ||
      gcm_metadata.version() != kGcmMetadataVersion) {
    PA_LOG(WARNING) << "Failed to validate GcmMetadata in "
                    << "[Responder Auth] header.";
    context.callback.Run(false, std::string());
    return;
  }

  // Extract responder session public key from |decryption_key_id| field.
  securemessage::ResponderHello responder_hello;
  if (!responder_hello.ParseFromString(header.decryption_key_id()) ||
      !responder_hello.public_dh_key().SerializeToString(
          &context.responder_session_public_key)) {
    PA_LOG(INFO) << "Failed to extract responder session public key in "
                 << "[Responder Auth] header.";
    context.callback.Run(false, std::string());
    return;
  }

  // Perform a Diffie-Helmann key exchange to get the session symmetric key.
  context.secure_message_delegate->DeriveKey(
      context.session_private_key, context.responder_session_public_key,
      base::Bind(&OnSessionSymmetricKeyDerived, context));
}

// Called after the session symmetric key is derived, so now we can unwrap the
// outer message of [Responder Auth].
void OnSessionSymmetricKeyDerived(ValidateResponderAuthMessageContext context,
                                  const std::string& session_symmetric_key) {
  context.session_symmetric_key = session_symmetric_key;

  // Unwrap the outer message, using symmetric key encryption and signature.
  SecureMessageDelegate::UnwrapOptions unwrap_options;
  unwrap_options.encryption_scheme = securemessage::AES_256_CBC;
  unwrap_options.signature_scheme = securemessage::HMAC_SHA256;
  context.secure_message_delegate->UnwrapSecureMessage(
      context.responder_auth_message, session_symmetric_key, unwrap_options,
      base::Bind(&OnOuterMessageUnwrappedForResponderAuth, context));
}

// Called after the outer-most layer of [Responder Auth] is unwrapped.
void OnOuterMessageUnwrappedForResponderAuth(
    const ValidateResponderAuthMessageContext& context,
    bool verified,
    const std::string& payload,
    const securemessage::Header& header) {
  if (!verified) {
    PA_LOG(INFO) << "Failed to unwrap outer [Responder Auth] message.";
    context.callback.Run(false, std::string());
    return;
  }

  // Parse the decrypted payload.
  securemessage::DeviceToDeviceMessage device_to_device_message;
  if (!device_to_device_message.ParseFromString(payload) ||
      device_to_device_message.sequence_number() != 1) {
    PA_LOG(INFO) << "Failed to validate DeviceToDeviceMessage payload.";
    context.callback.Run(false, std::string());
    return;
  }

  // Unwrap the middle level SecureMessage, using symmetric key encryption and
  // signature.
  SecureMessageDelegate::UnwrapOptions unwrap_options;
  unwrap_options.encryption_scheme = securemessage::AES_256_CBC;
  unwrap_options.signature_scheme = securemessage::HMAC_SHA256;
  unwrap_options.associated_data = context.hello_message;
  context.secure_message_delegate->UnwrapSecureMessage(
      device_to_device_message.message(), context.persistent_symmetric_key,
      unwrap_options,
      base::Bind(&OnMiddleMessageUnwrappedForResponderAuth, context));
}

// Called after the middle layer of [Responder Auth] is unwrapped.
void OnMiddleMessageUnwrappedForResponderAuth(
    const ValidateResponderAuthMessageContext& context,
    bool verified,
    const std::string& payload,
    const securemessage::Header& header) {
  if (!verified) {
    PA_LOG(INFO) << "Failed to unwrap middle [Responder Auth] message.";
    context.callback.Run(false, std::string());
    return;
  }

  // Unwrap the inner-most SecureMessage, using no encryption and an asymmetric
  // key signature.
  SecureMessageDelegate::UnwrapOptions unwrap_options;
  unwrap_options.encryption_scheme = securemessage::NONE;
  unwrap_options.signature_scheme = securemessage::ECDSA_P256_SHA256;
  unwrap_options.associated_data = context.hello_message;
  context.secure_message_delegate->UnwrapSecureMessage(
      payload, context.persistent_responder_public_key, unwrap_options,
      base::Bind(&OnInnerMessageUnwrappedForResponderAuth, context));
}

// Called after the inner-most layer of [Responder Auth] is unwrapped.
void OnInnerMessageUnwrappedForResponderAuth(
    const ValidateResponderAuthMessageContext& context,
    bool verified,
    const std::string& payload,
    const securemessage::Header& header) {
  if (!verified)
    PA_LOG(INFO) << "Failed to unwrap inner [Responder Auth] message.";

  // Note: The GMS Core implementation does not properly set the metadata
  // version, so we only check that the type is UNLOCK_KEY_SIGNED_CHALLENGE.
  GcmMetadata gcm_metadata;
  if (!gcm_metadata.ParseFromString(header.public_metadata()) ||
      gcm_metadata.type() != UNLOCK_KEY_SIGNED_CHALLENGE) {
    PA_LOG(WARNING) << "Failed to validate GcmMetadata in inner-most "
                    << "[Responder Auth] message.";
    context.callback.Run(false, std::string());
    return;
  }

  context.callback.Run(verified, context.session_symmetric_key);
}

}  // namespace

// static
void DeviceToDeviceInitiatorOperations::CreateHelloMessage(
    const std::string& session_public_key,
    const std::string& persistent_symmetric_key,
    SecureMessageDelegate* secure_message_delegate,
    const MessageCallback& callback) {
  // Decode public key into the |initator_hello| proto.
  securemessage::InitiatorHello initator_hello;
  if (!initator_hello.mutable_public_dh_key()->ParseFromString(
          session_public_key)) {
    PA_LOG(ERROR) << "Unable to parse user's public key";
    callback.Run(std::string());
    return;
  }

  // The [Hello] message has the structure:
  // {
  //   header: <session_public_key>,
  //           Sig(<session_public_key>, persistent_symmetric_key)
  //   payload: ""
  // }
  SecureMessageDelegate::CreateOptions create_options;
  create_options.encryption_scheme = securemessage::NONE;
  create_options.signature_scheme = securemessage::HMAC_SHA256;
  initator_hello.SerializeToString(&create_options.public_metadata);
  secure_message_delegate->CreateSecureMessage(
      kPayloadFiller, persistent_symmetric_key, create_options, callback);
}

// static
void DeviceToDeviceInitiatorOperations::ValidateResponderAuthMessage(
    const std::string& responder_auth_message,
    const std::string& persistent_responder_public_key,
    const std::string& persistent_symmetric_key,
    const std::string& session_private_key,
    const std::string& hello_message,
    SecureMessageDelegate* secure_message_delegate,
    const ValidateResponderAuthCallback& callback) {
  // The [Responder Auth] message has the structure:
  // {
  //   header: <responder_public_key>,
  //           Sig(<responder_public_key> + payload1,
  //               session_symmetric_key),
  //   payload1: Enc({
  //     header: Sig(payload2 + <hello_message>, persistent_symmetric_key),
  //     payload2: {
  //       sequence_number: 1,
  //       body: Enc({
  //         header: Sig(payload3 + <hello_message>,
  //                     persistent_responder_public_key),
  //         payload3: ""
  //       }, persistent_symmetric_key)
  //     }
  //   }, session_symmetric_key),
  // }
  struct ValidateResponderAuthMessageContext context = {
      responder_auth_message,
      persistent_responder_public_key,
      persistent_symmetric_key,
      session_private_key,
      hello_message,
      secure_message_delegate,
      callback};
  BeginResponderAuthValidation(context);
}

// static
void DeviceToDeviceInitiatorOperations::CreateInitiatorAuthMessage(
    const std::string& session_symmetric_key,
    const std::string& persistent_symmetric_key,
    const std::string& responder_auth_message,
    SecureMessageDelegate* secure_message_delegate,
    const MessageCallback& callback) {
  // The [Initiator Auth] message has the structure:
  // {
  //   header: Sig(payload1, session_symmetric_key)
  //   payload1: Enc({
  //     sequence_number: 2,
  //     body: {
  //       header: Sig(payload2 + responder_auth_message,
  //                   persistent_symmetric_key)
  //       payload2: ""
  //     }
  //   }, session_symmetric_key)
  // }
  SecureMessageDelegate::CreateOptions create_options;
  create_options.encryption_scheme = securemessage::AES_256_CBC;
  create_options.signature_scheme = securemessage::HMAC_SHA256;
  create_options.associated_data = responder_auth_message;
  secure_message_delegate->CreateSecureMessage(
      kPayloadFiller, persistent_symmetric_key, create_options,
      base::Bind(&OnInnerMessageCreatedForInitiatorAuth, session_symmetric_key,
                 secure_message_delegate, callback));
}

}  // cryptauth
