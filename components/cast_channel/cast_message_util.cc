// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_message_util.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/cast_channel/cast_auth_util.h"
#include "components/cast_channel/enum_table.h"
#include "components/cast_channel/proto/cast_channel.pb.h"

using base::Value;
using cast_util::EnumToString;
using cast_util::StringToEnum;

namespace cast_util {

using namespace cast_channel;

template <>
const EnumTable<CastMessageType> EnumTable<CastMessageType>::instance(
    {
        {CastMessageType::kPing, "PING"},
        {CastMessageType::kPong, "PONG"},
        {CastMessageType::kGetAppAvailability, "GET_APP_AVAILABILITY"},
        {CastMessageType::kReceiverStatusRequest, "GET_STATUS"},
        {CastMessageType::kConnect, "CONNECT"},
        {CastMessageType::kCloseConnection, "CLOSE"},
        {CastMessageType::kBroadcast, "APPLICATION_BROADCAST"},
        {CastMessageType::kLaunch, "LAUNCH"},
        {CastMessageType::kStop, "STOP"},
        {CastMessageType::kReceiverStatus, "RECEIVER_STATUS"},
        {CastMessageType::kMediaStatus, "MEDIA_STATUS"},
        {CastMessageType::kLaunchError, "LAUNCH_ERROR"},
        {CastMessageType::kOther},
    },
    CastMessageType::kMaxValue);

template <>
const EnumTable<V2MessageType> EnumTable<V2MessageType>::instance(
    {
        {V2MessageType::kEditTracksInfo, "EDIT_TRACKS_INFO"},
        {V2MessageType::kGetStatus, "GET_STATUS"},
        {V2MessageType::kLoad, "LOAD"},
        {V2MessageType::kMediaGetStatus, "MEDIA_GET_STATUS"},
        {V2MessageType::kMediaSetVolume, "MEDIA_SET_VOLUME"},
        {V2MessageType::kPause, "PAUSE"},
        {V2MessageType::kPlay, "PLAY"},
        {V2MessageType::kPrecache, "PRECACHE"},
        {V2MessageType::kQueueInsert, "QUEUE_INSERT"},
        {V2MessageType::kQueueLoad, "QUEUE_LOAD"},
        {V2MessageType::kQueueRemove, "QUEUE_REMOVE"},
        {V2MessageType::kQueueReorder, "QUEUE_REORDER"},
        {V2MessageType::kQueueUpdate, "QUEUE_UPDATE"},
        {V2MessageType::kSeek, "SEEK"},
        {V2MessageType::kSetVolume, "SET_VOLUME"},
        {V2MessageType::kStop, "STOP"},
        {V2MessageType::kStopMedia, "STOP_MEDIA"},
        {V2MessageType::kOther},
    },
    V2MessageType::kMaxValue);

template <>
const EnumTable<GetAppAvailabilityResult>
    EnumTable<GetAppAvailabilityResult>::instance(
        {
            {GetAppAvailabilityResult::kAvailable, "APP_AVAILABLE"},
            {GetAppAvailabilityResult::kUnavailable, "APP_UNAVAILABLE"},
            {GetAppAvailabilityResult::kUnknown},
        },
        GetAppAvailabilityResult::kMaxValue);

}  // namespace cast_util

namespace cast_channel {

namespace {

// The value used for "sdkType" in a virtual connect request. Historically, this
// value is used in the Media Router extension, but here it is reused in Chrome.
constexpr int kVirtualConnectSdkType = 2;

// The value used for "connectionType" in a virtual connect request. This value
// stands for CONNECTION_TYPE_LOCAL, which is the only type used in Chrome.
constexpr int kVirtualConnectTypeLocal = 1;

void FillCommonCastMessageFields(CastMessage* message,
                                 const std::string& source_id,
                                 const std::string& destination_id,
                                 const std::string& message_namespace) {
  message->set_protocol_version(CastMessage::CASTV2_1_0);
  message->set_source_id(source_id);
  message->set_destination_id(destination_id);
  message->set_namespace_(message_namespace);
}

CastMessage CreateKeepAliveMessage(base::StringPiece keep_alive_type) {
  base::Value type_dict(base::Value::Type::DICTIONARY);
  type_dict.SetKey("type", base::Value(keep_alive_type));
  return CreateCastMessage(kHeartbeatNamespace, type_dict, kPlatformSenderId,
                           kPlatformReceiverId);
}

// Returns the value to be set as the "platform" value in a virtual connect
// request. The value is platform-dependent and is taken from the Platform enum
// defined in third_party/metrics_proto/cast_logs.proto.
int GetVirtualConnectPlatformValue() {
#if defined(OS_WIN)
  return 3;
#elif defined(OS_MACOSX)
  return 4;
#elif defined(OS_CHROMEOS)
  return 5;
#elif defined(OS_LINUX)
  return 6;
#else
  return 0;
#endif
}

// Maps from from API-internal message types to "real" message types from the
// Cast V2 protocol.  This is necessary because the protocol defines messages
// with the same type in different namespaces, and the namespace is lost when
// messages are passed using a CastInternalMessage object.
base::StringPiece GetRemappedMediaRequestType(
    base::StringPiece v2_message_type) {
  base::Optional<V2MessageType> type =
      StringToEnum<V2MessageType>(v2_message_type);
  DCHECK(type && IsMediaRequestMessageType(*type));
  switch (*type) {
    case V2MessageType::kStopMedia:
      type = V2MessageType::kStop;
      break;
    case V2MessageType::kMediaSetVolume:
      type = V2MessageType::kSetVolume;
      break;
    case V2MessageType::kMediaGetStatus:
      type = V2MessageType::kGetStatus;
      break;
    default:
      return v2_message_type;
  }
  return *EnumToString(*type);
}

}  // namespace

bool IsCastMessageValid(const CastMessage& message_proto) {
  if (!message_proto.IsInitialized())
    return false;

  if (message_proto.namespace_().empty() || message_proto.source_id().empty() ||
      message_proto.destination_id().empty()) {
    return false;
  }
  return (message_proto.payload_type() == CastMessage_PayloadType_STRING &&
          message_proto.has_payload_utf8()) ||
         (message_proto.payload_type() == CastMessage_PayloadType_BINARY &&
          message_proto.has_payload_binary());
}

bool IsCastInternalNamespace(const std::string& message_namespace) {
  // Note: any namespace with the prefix is assumed to be reserved for internal
  // messages.
  return base::StartsWith(message_namespace, kCastInternalNamespacePrefix,
                          base::CompareCase::SENSITIVE);
}

CastMessageType ParseMessageTypeFromPayload(const base::Value& payload) {
  const Value* type_string = payload.FindKeyOfType("type", Value::Type::STRING);
  return type_string ? CastMessageTypeFromString(type_string->GetString())
                     : CastMessageType::kOther;
}

const char* ToString(CastMessageType message_type) {
  return EnumToString(message_type).value_or("").data();
}

const char* ToString(V2MessageType message_type) {
  return EnumToString(message_type).value_or(nullptr).data();
}

CastMessageType CastMessageTypeFromString(const std::string& type) {
  auto result = StringToEnum<CastMessageType>(type);
  DVLOG_IF(1, !result) << "Unknown message type: " << type;
  return result.value_or(CastMessageType::kOther);
}

V2MessageType V2MessageTypeFromString(const std::string& type) {
  return StringToEnum<V2MessageType>(type).value_or(V2MessageType::kOther);
}

std::string CastMessageToString(const CastMessage& message_proto) {
  std::string out("{");
  out += "namespace = " + message_proto.namespace_();
  out += ", sourceId = " + message_proto.source_id();
  out += ", destId = " + message_proto.destination_id();
  out += ", type = " + base::NumberToString(message_proto.payload_type());
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
    out += base::NumberToString(message.error().error_type());
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

  FillCommonCastMessageFields(message_proto, kPlatformSenderId,
                              kPlatformReceiverId, kAuthNamespace);
  message_proto->set_payload_type(CastMessage_PayloadType_BINARY);
  message_proto->set_payload_binary(auth_message_string);
}

bool IsAuthMessage(const CastMessage& message) {
  return message.namespace_() == kAuthNamespace;
}

bool IsReceiverMessage(const CastMessage& message) {
  return message.namespace_() == kReceiverNamespace;
}

bool IsPlatformSenderMessage(const CastMessage& message) {
  return message.destination_id() != cast_channel::kPlatformSenderId;
}

CastMessage CreateKeepAlivePingMessage() {
  return CreateKeepAliveMessage(
      EnumToString<CastMessageType, CastMessageType::kPing>());
}

CastMessage CreateKeepAlivePongMessage() {
  return CreateKeepAliveMessage(
      EnumToString<CastMessageType, CastMessageType::kPong>());
}

CastMessage CreateVirtualConnectionRequest(
    const std::string& source_id,
    const std::string& destination_id,
    VirtualConnectionType connection_type,
    const std::string& user_agent,
    const std::string& browser_version) {
  DCHECK(destination_id != kPlatformReceiverId || connection_type == kStrong);

  // Parse system_version from user agent string. It contains platform, OS and
  // CPU info and is contained in the first set of parentheses of the user
  // agent string (e.g., X11; Linux x86_64).
  std::string system_version;
  size_t start_index = user_agent.find('(');
  if (start_index != std::string::npos) {
    size_t end_index = user_agent.find(')', start_index + 1);
    if (end_index != std::string::npos) {
      system_version =
          user_agent.substr(start_index + 1, end_index - start_index - 1);
    }
  }

  Value dict(Value::Type::DICTIONARY);
  dict.SetKey(
      "type",
      Value(EnumToString<CastMessageType, CastMessageType::kConnect>()));
  dict.SetKey("userAgent", Value(user_agent));
  dict.SetKey("connType", Value(connection_type));
  dict.SetKey("origin", Value(Value::Type::DICTIONARY));

  Value sender_info(Value::Type::DICTIONARY);
  sender_info.SetKey("sdkType", Value(kVirtualConnectSdkType));
  sender_info.SetKey("version", Value(browser_version));
  sender_info.SetKey("browserVersion", Value(browser_version));
  sender_info.SetKey("platform", Value(GetVirtualConnectPlatformValue()));
  sender_info.SetKey("connectionType", Value(kVirtualConnectTypeLocal));
  if (!system_version.empty())
    sender_info.SetKey("systemVersion", Value(system_version));

  dict.SetKey("senderInfo", std::move(sender_info));

  return CreateCastMessage(kConnectionNamespace, dict, source_id,
                           destination_id);
}

CastMessage CreateGetAppAvailabilityRequest(const std::string& source_id,
                                            int request_id,
                                            const std::string& app_id) {
  Value dict(Value::Type::DICTIONARY);
  dict.SetKey("type",
              Value(EnumToString<CastMessageType,
                                 CastMessageType::kGetAppAvailability>()));
  Value app_id_value(Value::Type::LIST);
  app_id_value.GetList().push_back(Value(app_id));
  dict.SetKey("appId", std::move(app_id_value));
  dict.SetKey("requestId", Value(request_id));

  return CreateCastMessage(kReceiverNamespace, dict, source_id,
                           kPlatformReceiverId);
}

CastMessage CreateReceiverStatusRequest(const std::string& source_id,
                                        int request_id) {
  Value dict(Value::Type::DICTIONARY);
  dict.SetKey("type",
              Value(EnumToString<CastMessageType,
                                 CastMessageType::kReceiverStatusRequest>()));
  dict.SetKey("requestId", Value(request_id));
  return CreateCastMessage(kReceiverNamespace, dict, source_id,
                           kPlatformReceiverId);
}

BroadcastRequest::BroadcastRequest(const std::string& broadcast_namespace,
                                   const std::string& message)
    : broadcast_namespace(broadcast_namespace), message(message) {}
BroadcastRequest::~BroadcastRequest() = default;

bool BroadcastRequest::operator==(const BroadcastRequest& other) const {
  return broadcast_namespace == other.broadcast_namespace &&
         message == other.message;
}

CastMessage CreateBroadcastRequest(const std::string& source_id,
                                   int request_id,
                                   const std::vector<std::string>& app_ids,
                                   const BroadcastRequest& request) {
  Value dict(Value::Type::DICTIONARY);
  dict.SetKey(
      "type",
      Value(EnumToString<CastMessageType, CastMessageType::kBroadcast>()));
  std::vector<Value> app_ids_value;
  for (const std::string& app_id : app_ids)
    app_ids_value.push_back(Value(app_id));

  dict.SetKey("appIds", Value(std::move(app_ids_value)));
  dict.SetKey("namespace", Value(request.broadcast_namespace));
  dict.SetKey("message", Value(request.message));
  return CreateCastMessage(kBroadcastNamespace, dict, source_id,
                           kPlatformReceiverId);
}

CastMessage CreateLaunchRequest(const std::string& source_id,
                                int request_id,
                                const std::string& app_id,
                                const std::string& locale) {
  Value dict(Value::Type::DICTIONARY);
  dict.SetKey("type",
              Value(EnumToString<CastMessageType, CastMessageType::kLaunch>()));
  dict.SetKey("requestId", Value(request_id));
  dict.SetKey("appId", Value(app_id));
  dict.SetKey("language", Value(locale));

  return CreateCastMessage(kReceiverNamespace, dict, source_id,
                           kPlatformReceiverId);
}

CastMessage CreateStopRequest(const std::string& source_id,
                              int request_id,
                              const std::string& session_id) {
  Value dict(Value::Type::DICTIONARY);
  dict.SetKey("type",
              Value(EnumToString<CastMessageType, CastMessageType::kStop>()));
  dict.SetKey("requestId", Value(request_id));
  dict.SetKey("sessionId", Value(session_id));
  return CreateCastMessage(kReceiverNamespace, dict, source_id,
                           kPlatformReceiverId);
}

CastMessage CreateCastMessage(const std::string& message_namespace,
                              const base::Value& message,
                              const std::string& source_id,
                              const std::string& destination_id) {
  CastMessage output;
  FillCommonCastMessageFields(&output, source_id, destination_id,
                              message_namespace);
  output.set_payload_type(
      CastMessage::PayloadType::CastMessage_PayloadType_STRING);
  CHECK(base::JSONWriter::Write(message, output.mutable_payload_utf8()));
  return output;
}

CastMessage CreateMediaRequest(const base::Value& body,
                               int request_id,
                               const std::string& source_id,
                               const std::string& destination_id) {
  Value dict = body.Clone();
  Value* type = dict.FindKeyOfType("type", Value::Type::STRING);
  CHECK(type);
  dict.SetKey("type", Value(GetRemappedMediaRequestType(type->GetString())));
  dict.SetKey("requestId", Value(request_id));
  return CreateCastMessage(kMediaNamespace, dict, source_id, destination_id);
}

CastMessage CreateSetVolumeRequest(const base::Value& body,
                                   int request_id,
                                   const std::string& source_id) {
  DCHECK(body.FindKeyOfType("type", Value::Type::STRING) &&
         body.FindKeyOfType("type", Value::Type::STRING)->GetString() ==
             (EnumToString<V2MessageType, V2MessageType::kSetVolume>())
                 .as_string());
  Value dict = body.Clone();
  dict.RemoveKey("sessionId");
  dict.SetKey("requestId", Value(request_id));
  return CreateCastMessage(kReceiverNamespace, dict, source_id,
                           kPlatformReceiverId);
}

bool IsMediaRequestMessageType(V2MessageType type) {
  switch (type) {
    case V2MessageType::kEditTracksInfo:
    case V2MessageType::kLoad:
    case V2MessageType::kMediaGetStatus:
    case V2MessageType::kMediaSetVolume:
    case V2MessageType::kPause:
    case V2MessageType::kPlay:
    case V2MessageType::kPrecache:
    case V2MessageType::kQueueInsert:
    case V2MessageType::kQueueLoad:
    case V2MessageType::kQueueRemove:
    case V2MessageType::kQueueReorder:
    case V2MessageType::kQueueUpdate:
    case V2MessageType::kSeek:
    case V2MessageType::kStopMedia:
      return true;
    default:
      return false;
  }
}

const char* ToString(GetAppAvailabilityResult result) {
  return EnumToString(result).value_or(nullptr).data();
}

base::Optional<int> GetRequestIdFromResponse(const Value& payload) {
  DCHECK(payload.is_dict());

  const Value* request_id_value =
      payload.FindKeyOfType("requestId", Value::Type::INTEGER);
  if (!request_id_value)
    return base::nullopt;
  return request_id_value->GetInt();
}

GetAppAvailabilityResult GetAppAvailabilityResultFromResponse(
    const Value& payload,
    const std::string& app_id) {
  DCHECK(payload.is_dict());
  const Value* availability_value =
      payload.FindPathOfType({"availability", app_id}, Value::Type::STRING);
  if (!availability_value)
    return GetAppAvailabilityResult::kUnknown;

  return StringToEnum<GetAppAvailabilityResult>(availability_value->GetString())
      .value_or(GetAppAvailabilityResult::kUnknown);
}

LaunchSessionResponse::LaunchSessionResponse() {}
LaunchSessionResponse::LaunchSessionResponse(LaunchSessionResponse&& other) =
    default;
LaunchSessionResponse::~LaunchSessionResponse() = default;

LaunchSessionResponse GetLaunchSessionResponse(const base::Value& payload) {
  const Value* type_value = payload.FindKeyOfType("type", Value::Type::STRING);
  if (!type_value)
    return LaunchSessionResponse();

  const auto type = CastMessageTypeFromString(type_value->GetString());
  if (type != CastMessageType::kReceiverStatus &&
      type != CastMessageType::kLaunchError)
    return LaunchSessionResponse();

  LaunchSessionResponse response;
  if (type == CastMessageType::kLaunchError) {
    response.result = LaunchSessionResponse::Result::kError;
    return response;
  }

  const Value* receiver_status =
      payload.FindKeyOfType("status", Value::Type::DICTIONARY);
  if (!receiver_status)
    return LaunchSessionResponse();

  response.result = LaunchSessionResponse::Result::kOk;
  response.receiver_status = receiver_status->Clone();
  return response;
}

}  // namespace cast_channel
