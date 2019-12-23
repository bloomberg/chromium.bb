// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/connection_namespace_handler.h"

#include <type_traits>

#include "absl/types/optional.h"
#include "cast/common/channel/cast_socket.h"
#include "cast/common/channel/message_util.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "cast/common/channel/virtual_connection.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "cast/common/channel/virtual_connection_router.h"
#include "util/json/json_serialization.h"
#include "util/json/json_value.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {

using ::cast::channel::CastMessage;
using ::cast::channel::CastMessage_PayloadType;

namespace {

bool IsValidProtocolVersion(int version) {
  return ::cast::channel::CastMessage_ProtocolVersion_IsValid(version);
}

absl::optional<int> FindMaxProtocolVersion(const Json::Value* version,
                                           const Json::Value* version_list) {
  using ArrayIndex = Json::Value::ArrayIndex;
  static_assert(std::is_integral<ArrayIndex>::value,
                "Assuming ArrayIndex is integral");
  absl::optional<int> max_version;
  if (version_list && version_list->isArray()) {
    max_version = ::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0;
    for (auto it = version_list->begin(), end = version_list->end(); it != end;
         ++it) {
      if (it->isInt()) {
        int version_int = it->asInt();
        if (IsValidProtocolVersion(version_int) && version_int > *max_version) {
          max_version = version_int;
        }
      }
    }
  }
  if (version && version->isInt()) {
    int version_int = version->asInt();
    if (IsValidProtocolVersion(version_int)) {
      if (!max_version) {
        max_version = ::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0;
      }
      if (version_int > max_version) {
        max_version = version_int;
      }
    }
  }
  return max_version;
}

VirtualConnection::CloseReason GetCloseReason(
    const Json::Value& parsed_message) {
  VirtualConnection::CloseReason reason =
      VirtualConnection::CloseReason::kClosedByPeer;
  absl::optional<int> reason_code = MaybeGetInt(
      parsed_message, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyReasonCode));
  if (reason_code) {
    int code = reason_code.value();
    if (code >= VirtualConnection::CloseReason::kFirstReason &&
        code <= VirtualConnection::CloseReason::kLastReason) {
      reason = static_cast<VirtualConnection::CloseReason>(code);
    }
  }
  return reason;
}

}  // namespace

ConnectionNamespaceHandler::ConnectionNamespaceHandler(
    VirtualConnectionManager* vc_manager,
    VirtualConnectionPolicy* vc_policy)
    : vc_manager_(vc_manager), vc_policy_(vc_policy) {
  OSP_DCHECK(vc_manager);
  OSP_DCHECK(vc_policy);
}

ConnectionNamespaceHandler::~ConnectionNamespaceHandler() = default;

void ConnectionNamespaceHandler::OnMessage(VirtualConnectionRouter* router,
                                           CastSocket* socket,
                                           CastMessage message) {
  if (message.payload_type() !=
      CastMessage_PayloadType::CastMessage_PayloadType_STRING) {
    return;
  }
  ErrorOr<Json::Value> result = json::Parse(message.payload_utf8());
  if (result.is_error()) {
    return;
  }

  Json::Value& value = result.value();
  if (!value.isObject()) {
    return;
  }

  absl::optional<absl::string_view> type =
      MaybeGetString(value, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyType));
  if (!type) {
    // TODO(btolsch): Some of these paths should have error reporting.  One
    // possibility is to pass errors back through |router| so higher-level code
    // can decide whether to show an error to the user, stop talking to a
    // particular device, etc.
    return;
  }

  absl::string_view type_str = type.value();
  if (type_str == kMessageTypeConnect) {
    HandleConnect(router, socket, std::move(message), std::move(value));
  } else if (type_str == kMessageTypeClose) {
    HandleClose(router, socket, std::move(message), std::move(value));
  } else {
    // NOTE: Unknown message type so ignore it.
    // TODO(btolsch): Should be included in future error reporting.
  }
}

void ConnectionNamespaceHandler::HandleConnect(VirtualConnectionRouter* router,
                                               CastSocket* socket,
                                               CastMessage message,
                                               Json::Value parsed_message) {
  if (message.destination_id() == kBroadcastId ||
      message.source_id() == kBroadcastId) {
    return;
  }

  VirtualConnection virtual_conn{std::move(message.destination_id()),
                                 std::move(message.source_id()),
                                 socket->socket_id()};
  if (!vc_policy_->IsConnectionAllowed(virtual_conn)) {
    SendClose(router, std::move(virtual_conn));
    return;
  }

  absl::optional<int> maybe_conn_type = MaybeGetInt(
      parsed_message, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyConnType));
  VirtualConnection::Type conn_type = VirtualConnection::Type::kStrong;
  if (maybe_conn_type) {
    int int_type = maybe_conn_type.value();
    if (int_type < static_cast<int>(VirtualConnection::Type::kMinValue) ||
        int_type > static_cast<int>(VirtualConnection::Type::kMaxValue)) {
      SendClose(router, std::move(virtual_conn));
      return;
    }
    conn_type = static_cast<VirtualConnection::Type>(int_type);
  }

  VirtualConnection::AssociatedData data;

  data.type = conn_type;

  absl::optional<absl::string_view> user_agent = MaybeGetString(
      parsed_message, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyUserAgent));
  if (user_agent) {
    data.user_agent = std::string(user_agent.value());
  }

  const Json::Value* sender_info_value = parsed_message.find(
      JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeySenderInfo));
  if (!sender_info_value || !sender_info_value->isObject()) {
    // TODO(btolsch): Should this be guessed from user agent?
    OSP_DVLOG << "No sender info from protocol.";
  }

  const Json::Value* version_value = parsed_message.find(
      JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyProtocolVersion));
  const Json::Value* version_list_value = parsed_message.find(
      JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyProtocolVersionList));
  absl::optional<int> negotiated_version =
      FindMaxProtocolVersion(version_value, version_list_value);
  if (negotiated_version) {
    data.max_protocol_version = static_cast<VirtualConnection::ProtocolVersion>(
        negotiated_version.value());
  } else {
    data.max_protocol_version = VirtualConnection::ProtocolVersion::kV2_1_0;
  }

  data.ip_fragment = socket->GetSanitizedIpAddress();

  OSP_DVLOG << "Connection opened: " << virtual_conn.local_id << ", "
            << virtual_conn.peer_id << ", " << virtual_conn.socket_id;

  // NOTE: Only send a response for senders that actually sent a version.  This
  // maintains compatibility with older senders that don't send a version and
  // don't expect a response.
  if (negotiated_version) {
    SendConnectedResponse(router, virtual_conn, negotiated_version.value());
  }

  vc_manager_->AddConnection(std::move(virtual_conn), std::move(data));
}

void ConnectionNamespaceHandler::HandleClose(VirtualConnectionRouter* router,
                                             CastSocket* socket,
                                             CastMessage message,
                                             Json::Value parsed_message) {
  VirtualConnection virtual_conn{std::move(message.destination_id()),
                                 std::move(message.source_id()),
                                 socket->socket_id()};
  if (!vc_manager_->GetConnectionData(virtual_conn)) {
    return;
  }

  VirtualConnection::CloseReason reason = GetCloseReason(parsed_message);

  OSP_DVLOG << "Connection closed (reason: " << reason
            << "): " << virtual_conn.local_id << ", " << virtual_conn.peer_id
            << ", " << virtual_conn.socket_id;
  vc_manager_->RemoveConnection(virtual_conn, reason);
}

void ConnectionNamespaceHandler::SendClose(VirtualConnectionRouter* router,
                                           VirtualConnection virtual_conn) {
  Json::Value close_message(Json::ValueType::objectValue);
  close_message[kMessageKeyType] = kMessageTypeClose;

  ErrorOr<std::string> result = json::Stringify(close_message);
  if (result.is_error()) {
    return;
  }

  router->SendMessage(
      std::move(virtual_conn),
      MakeSimpleUTF8Message(kConnectionNamespace, std::move(result.value())));
}

void ConnectionNamespaceHandler::SendConnectedResponse(
    VirtualConnectionRouter* router,
    const VirtualConnection& virtual_conn,
    int max_protocol_version) {
  Json::Value connected_message(Json::ValueType::objectValue);
  connected_message[kMessageKeyType] = kMessageTypeConnected;
  connected_message[kMessageKeyProtocolVersion] =
      static_cast<int>(max_protocol_version);

  ErrorOr<std::string> result = json::Stringify(connected_message);
  if (result.is_error()) {
    return;
  }

  router->SendMessage(
      virtual_conn,
      MakeSimpleUTF8Message(kConnectionNamespace, std::move(result.value())));
}

}  // namespace cast
}  // namespace openscreen
