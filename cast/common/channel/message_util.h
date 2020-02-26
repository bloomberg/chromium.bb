// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_MESSAGE_UTIL_H_
#define CAST_COMMON_CHANNEL_MESSAGE_UTIL_H_

#include "absl/strings/string_view.h"
#include "cast/common/channel/proto/cast_channel.pb.h"

namespace openscreen {
namespace cast {

// Reserved message namespaces for internal messages.
static constexpr char kCastInternalNamespacePrefix[] =
    "urn:x-cast:com.google.cast.";
static constexpr char kTransportNamespacePrefix[] =
    "urn:x-cast:com.google.cast.tp.";
static constexpr char kAuthNamespace[] =
    "urn:x-cast:com.google.cast.tp.deviceauth";
static constexpr char kHeartbeatNamespace[] =
    "urn:x-cast:com.google.cast.tp.heartbeat";
static constexpr char kConnectionNamespace[] =
    "urn:x-cast:com.google.cast.tp.connection";
static constexpr char kReceiverNamespace[] =
    "urn:x-cast:com.google.cast.receiver";
static constexpr char kBroadcastNamespace[] =
    "urn:x-cast:com.google.cast.broadcast";
static constexpr char kMediaNamespace[] = "urn:x-cast:com.google.cast.media";

// Sender and receiver IDs to use for platform messages.
static constexpr char kPlatformSenderId[] = "sender-0";
static constexpr char kPlatformReceiverId[] = "receiver-0";

static constexpr char kBroadcastId[] = "*";

static constexpr ::cast::channel::CastMessage_ProtocolVersion
    kDefaultOutgoingMessageVersion =
        ::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0;

// JSON message key strings.
static constexpr char kMessageKeyType[] = "type";
static constexpr char kMessageKeyConnType[] = "connType";
static constexpr char kMessageKeyUserAgent[] = "userAgent";
static constexpr char kMessageKeySenderInfo[] = "senderInfo";
static constexpr char kMessageKeyProtocolVersion[] = "protocolVersion";
static constexpr char kMessageKeyProtocolVersionList[] = "protocolVersionList";
static constexpr char kMessageKeyReasonCode[] = "reasonCode";
static constexpr char kMessageKeyAppId[] = "appId";
static constexpr char kMessageKeyRequestId[] = "requestId";
static constexpr char kMessageKeyAvailability[] = "availability";

// JSON message field values.
static constexpr char kMessageTypeConnect[] = "CONNECT";
static constexpr char kMessageTypeClose[] = "CLOSE";
static constexpr char kMessageTypeConnected[] = "CONNECTED";
static constexpr char kMessageValueAppAvailable[] = "APP_AVAILABLE";
static constexpr char kMessageValueAppUnavailable[] = "APP_UNAVAILABLE";

// TODO(crbug.com/openscreen/111): Add validation that each message type is
// received on the correct namespace.  This will probably involve creating a
// data structure for mapping between type and namespace.
enum class CastMessageType {
  // Heartbeat messages.
  kPing,
  kPong,

  // RPC control/status messages used by Media Remoting. These occur at high
  // frequency, up to dozens per second at times, and should not be logged.
  kRpc,

  kGetAppAvailability,
  kGetStatus,

  // Virtual connection request.
  kConnect,

  // Close virtual connection.
  kCloseConnection,

  // Application broadcast / precache.
  kBroadcast,

  // Session launch request.
  kLaunch,

  // Session stop request.
  kStop,

  kReceiverStatus,
  kMediaStatus,

  // Error from receiver.
  kLaunchError,

  kOffer,
  kAnswer,
  kCapabilitiesResponse,
  kStatusResponse,

  // The following values are part of the protocol but are not currently used.
  kMultizoneStatus,
  kInvalidPlayerState,
  kLoadFailed,
  kLoadCancelled,
  kInvalidRequest,
  kPresentation,
  kGetCapabilities,

  kOther,  // Add new types above |kOther|.
  kMaxValue = kOther,
};

enum class AppAvailabilityResult {
  kAvailable,
  kUnavailable,
  kUnknown,
};

std::string ToString(AppAvailabilityResult availability);

// TODO(crbug.com/openscreen/111): When this and/or other enums need the
// string->enum mapping, import EnumTable from Chromium's
// //components/cast_channel/enum_table.h.
inline constexpr const char* CastMessageTypeToString(CastMessageType type) {
  switch (type) {
    case CastMessageType::kPing:
      return "PING";
    case CastMessageType::kPong:
      return "PONG";
    case CastMessageType::kRpc:
      return "RPC";
    case CastMessageType::kGetAppAvailability:
      return "GET_APP_AVAILABILITY";
    case CastMessageType::kGetStatus:
      return "GET_STATUS";
    case CastMessageType::kConnect:
      return "CONNECT";
    case CastMessageType::kCloseConnection:
      return "CLOSE";
    case CastMessageType::kBroadcast:
      return "APPLICATION_BROADCAST";
    case CastMessageType::kLaunch:
      return "LAUNCH";
    case CastMessageType::kStop:
      return "STOP";
    case CastMessageType::kReceiverStatus:
      return "RECEIVER_STATUS";
    case CastMessageType::kMediaStatus:
      return "MEDIA_STATUS";
    case CastMessageType::kLaunchError:
      return "LAUNCH_ERROR";
    case CastMessageType::kOffer:
      return "OFFER";
    case CastMessageType::kAnswer:
      return "ANSWER";
    case CastMessageType::kCapabilitiesResponse:
      return "CAPABILITIES_RESPONSE";
    case CastMessageType::kStatusResponse:
      return "STATUS_RESPONSE";
    case CastMessageType::kMultizoneStatus:
      return "MULTIZONE_STATUS";
    case CastMessageType::kInvalidPlayerState:
      return "INVALID_PLAYER_STATE";
    case CastMessageType::kLoadFailed:
      return "LOAD_FAILED";
    case CastMessageType::kLoadCancelled:
      return "LOAD_CANCELLED";
    case CastMessageType::kInvalidRequest:
      return "INVALID_REQUEST";
    case CastMessageType::kPresentation:
      return "PRESENTATION";
    case CastMessageType::kGetCapabilities:
      return "GET_CAPABILITIES";
    case CastMessageType::kOther:
    default:
      return "OTHER";
  }
}

inline bool IsAuthMessage(const ::cast::channel::CastMessage& message) {
  return message.namespace_() == kAuthNamespace;
}

inline bool IsTransportNamespace(absl::string_view namespace_) {
  return (namespace_.size() > (sizeof(kTransportNamespacePrefix) - 1)) &&
         (namespace_.find_first_of(kTransportNamespacePrefix) == 0);
}

::cast::channel::CastMessage MakeSimpleUTF8Message(
    const std::string& namespace_,
    std::string payload);

::cast::channel::CastMessage MakeConnectMessage(
    const std::string& source_id,
    const std::string& destination_id);
::cast::channel::CastMessage MakeCloseMessage(
    const std::string& source_id,
    const std::string& destination_id);

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_COMMON_CHANNEL_MESSAGE_UTIL_H_
