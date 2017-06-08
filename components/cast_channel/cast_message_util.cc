// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_message_util.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/cast_channel/cast_auth_util.h"
#include "components/cast_channel/proto/cast_channel.pb.h"

namespace {
static const char kAuthNamespace[] = "urn:x-cast:com.google.cast.tp.deviceauth";
// Sender and receiver IDs to use for platform messages.
static const char kPlatformSenderId[] = "sender-0";
static const char kPlatformReceiverId[] = "receiver-0";
}  // namespace

namespace cast_channel {

bool IsCastMessageValid(const CastMessage& message_proto) {
  if (message_proto.namespace_().empty() || message_proto.source_id().empty() ||
      message_proto.destination_id().empty()) {
    return false;
  }
  return (message_proto.payload_type() == CastMessage_PayloadType_STRING &&
          message_proto.has_payload_utf8()) ||
         (message_proto.payload_type() == CastMessage_PayloadType_BINARY &&
          message_proto.has_payload_binary());
}

std::string CastMessageToString(const CastMessage& message_proto) {
  std::string out("{");
  out += "namespace = " + message_proto.namespace_();
  out += ", sourceId = " + message_proto.source_id();
  out += ", destId = " + message_proto.destination_id();
  out += ", type = " + base::IntToString(message_proto.payload_type());
  out += ", str = \"" + message_proto.payload_utf8() + "\"}";
  return out;
}

std::string AuthMessageToString(const DeviceAuthMessage& message) {
  std::string out("{");
  if (message.has_challenge()) {
    out += "challenge: {}, ";
  }
  if (message.has_response()) {
    out += "response: {signature: (";
    out += base::SizeTToString(message.response().signature().length());
    out += " bytes), certificate: (";
    out += base::SizeTToString(
        message.response().client_auth_certificate().length());
    out += " bytes)}";
  }
  if (message.has_error()) {
    out += ", error: {";
    out += base::IntToString(message.error().error_type());
    out += "}";
  }
  out += "}";
  return out;
}

void CreateAuthChallengeMessage(CastMessage* message_proto,
                                const AuthContext& auth_context) {
  CHECK(message_proto);
  DeviceAuthMessage auth_message;
  auth_message.mutable_challenge()->set_sender_nonce(auth_context.nonce());
  std::string auth_message_string;
  auth_message.SerializeToString(&auth_message_string);

  message_proto->set_protocol_version(CastMessage_ProtocolVersion_CASTV2_1_0);
  message_proto->set_source_id(kPlatformSenderId);
  message_proto->set_destination_id(kPlatformReceiverId);
  message_proto->set_namespace_(kAuthNamespace);
  message_proto->set_payload_type(CastMessage_PayloadType_BINARY);
  message_proto->set_payload_binary(auth_message_string);
}

bool IsAuthMessage(const CastMessage& message) {
  return message.namespace_() == kAuthNamespace;
}

}  // namespace cast_channel
