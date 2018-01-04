// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_message_util.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/cast_channel/cast_auth_util.h"
#include "components/cast_channel/proto/cast_channel.pb.h"

namespace cast_channel {

namespace {
// Message namespaces.
constexpr char kAuthNamespace[] = "urn:x-cast:com.google.cast.tp.deviceauth";
constexpr char kHeartbeatNamespace[] =
    "urn:x-cast:com.google.cast.tp.heartbeat";

// Sender and receiver IDs to use for platform messages.
constexpr char kPlatformSenderId[] = "sender-0";
constexpr char kPlatformReceiverId[] = "receiver-0";

// Text payload keys.
constexpr char kTypeNodeId[] = "type";

// Cast application protocol message types.
constexpr char kKeepAlivePingType[] = "PING";
constexpr char kKeepAlivePongType[] = "PONG";

CastMessage CreateKeepAliveMessage(const char* keep_alive_type) {
  CastMessage output;
  output.set_protocol_version(CastMessage::CASTV2_1_0);
  output.set_source_id(kPlatformSenderId);
  output.set_destination_id(kPlatformReceiverId);
  output.set_namespace_(kHeartbeatNamespace);
  output.set_payload_type(
      CastMessage::PayloadType::CastMessage_PayloadType_STRING);

  base::DictionaryValue type_dict;
  type_dict.SetString(kTypeNodeId, keep_alive_type);
  CHECK(base::JSONWriter::Write(type_dict, output.mutable_payload_utf8()));
  return output;
}

}  // namespace

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

CastMessageType ParseMessageType(const CastMessage& message) {
  std::unique_ptr<base::Value> parsed_payload(
      base::JSONReader::Read(message.payload_utf8()));
  base::DictionaryValue* payload_as_dict;
  if (!parsed_payload || !parsed_payload->GetAsDictionary(&payload_as_dict))
    return CastMessageType::kOther;

  const base::Value* type_string =
      payload_as_dict->FindKeyOfType(kTypeNodeId, base::Value::Type::STRING);
  if (!type_string)
    return CastMessageType::kOther;

  const std::string& type = type_string->GetString();
  if (type == kKeepAlivePingType)
    return CastMessageType::kPing;
  if (type == kKeepAlivePongType)
    return CastMessageType::kPong;

  DVLOG(1) << "Unknown message type: " << type;
  return CastMessageType::kOther;
}

const char* CastMessageTypeToString(CastMessageType message_type) {
  switch (message_type) {
    case CastMessageType::kPing:
      return kKeepAlivePingType;
    case CastMessageType::kPong:
      return kKeepAlivePongType;
    case CastMessageType::kOther:
      return "unknown";
  }
  NOTREACHED();
  return "";
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
    out += base::NumberToString(message.response().signature().length());
    out += " bytes), certificate: (";
    out += base::NumberToString(
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

  AuthChallenge* challenge = auth_message.mutable_challenge();
  DCHECK(challenge);
  challenge->set_sender_nonce(auth_context.nonce());
  challenge->set_hash_algorithm(SHA256);

  std::string auth_message_string;
  auth_message.SerializeToString(&auth_message_string);

  message_proto->set_protocol_version(CastMessage::CASTV2_1_0);
  message_proto->set_source_id(kPlatformSenderId);
  message_proto->set_destination_id(kPlatformReceiverId);
  message_proto->set_namespace_(kAuthNamespace);
  message_proto->set_payload_type(CastMessage_PayloadType_BINARY);
  message_proto->set_payload_binary(auth_message_string);
}

bool IsAuthMessage(const CastMessage& message) {
  return message.namespace_() == kAuthNamespace;
}

CastMessage CreateKeepAlivePingMessage() {
  return CreateKeepAliveMessage(kKeepAlivePingType);
}

CastMessage CreateKeepAlivePongMessage() {
  return CreateKeepAliveMessage(kKeepAlivePongType);
}

}  // namespace cast_channel
