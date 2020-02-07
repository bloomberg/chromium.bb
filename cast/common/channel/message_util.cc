// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/message_util.h"

#include "util/logging.h"

namespace openscreen {
namespace cast {
namespace {

using ::cast::channel::CastMessage;

CastMessage MakeConnectionMessage(const std::string& source_id,
                                  const std::string& destination_id) {
  CastMessage connect_message;
  connect_message.set_protocol_version(kDefaultOutgoingMessageVersion);
  connect_message.set_source_id(source_id);
  connect_message.set_destination_id(destination_id);
  connect_message.set_namespace_(kConnectionNamespace);
  return connect_message;
}

}  // namespace

std::string ToString(AppAvailabilityResult availability) {
  switch (availability) {
    case AppAvailabilityResult::kAvailable:
      return "Available";
    case AppAvailabilityResult::kUnavailable:
      return "Unavailable";
    case AppAvailabilityResult::kUnknown:
      return "Unknown";
    default:
      OSP_NOTREACHED();
      return "bad value";
  }
}

CastMessage MakeSimpleUTF8Message(const std::string& namespace_,
                                  std::string payload) {
  CastMessage message;
  message.set_protocol_version(kDefaultOutgoingMessageVersion);
  message.set_namespace_(namespace_);
  message.set_payload_type(::cast::channel::CastMessage_PayloadType_STRING);
  message.set_payload_utf8(std::move(payload));
  return message;
}

CastMessage MakeConnectMessage(const std::string& source_id,
                               const std::string& destination_id) {
  CastMessage connect_message =
      MakeConnectionMessage(source_id, destination_id);
  connect_message.set_payload_type(
      ::cast::channel::CastMessage_PayloadType_STRING);
  connect_message.set_payload_utf8(R"!({"type": "CONNECT"})!");
  return connect_message;
}

CastMessage MakeCloseMessage(const std::string& source_id,
                             const std::string& destination_id) {
  CastMessage close_message = MakeConnectionMessage(source_id, destination_id);
  close_message.set_payload_type(
      ::cast::channel::CastMessage_PayloadType_STRING);
  close_message.set_payload_utf8(R"!({"type": "CLOSE"})!");
  return close_message;
}

}  // namespace cast
}  // namespace openscreen
