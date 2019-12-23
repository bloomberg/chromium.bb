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

// JSON message field values.
static constexpr char kMessageTypeConnect[] = "CONNECT";
static constexpr char kMessageTypeClose[] = "CLOSE";
static constexpr char kMessageTypeConnected[] = "CONNECTED";

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
