// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_
#define COMPONENTS_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_

#include <string>

#include "base/values.h"

namespace cast_channel {

class AuthContext;
class CastMessage;
class DeviceAuthMessage;

// Cast application protocol message types.
enum class CastMessageType { kOther, kPing, kPong };

// Checks if the contents of |message_proto| are valid.
bool IsCastMessageValid(const CastMessage& message_proto);

// Parses and returns the UTF-8 payload from |message|. Returns nullptr
// if the UTF-8 payload doesn't exist, or if it is not a dictionary.
std::unique_ptr<base::Value> GetDictionaryFromCastMessage(
    const CastMessage& message);

// Parses the JSON-encoded payload of |message| and returns the value in the
// "type" field or |kUnknown| if the parse fails or the field is not found.
// The result is only valid if |message| is a Cast application protocol message.
CastMessageType ParseMessageType(const CastMessage& message);

// Returns a human readable string for |message_type|.
const char* CastMessageTypeToString(CastMessageType message_type);

// Returns a human readable string for |message_proto|.
std::string CastMessageToString(const CastMessage& message_proto);

// Returns a human readable string for |message|.
std::string AuthMessageToString(const DeviceAuthMessage& message);

// Fills |message_proto| appropriately for an auth challenge request message.
// Uses the nonce challenge in |auth_context|.
void CreateAuthChallengeMessage(CastMessage* message_proto,
                                const AuthContext& auth_context);

// Returns whether the given message is an auth handshake message.
bool IsAuthMessage(const CastMessage& message);

// Returns whether |message| is a Cast receiver message.
bool IsReceiverMessage(const CastMessage& message);

// Returns whether |message| is destined for the platform sender.
bool IsPlatformSenderMessage(const CastMessage& message);

// Creates a keep-alive message of either type PING or PONG.
CastMessage CreateKeepAlivePingMessage();
CastMessage CreateKeepAlivePongMessage();

// Creates a virtual connection request message for |source_id| and
// |destination_id|.
CastMessage CreateVirtualConnectionRequest(const std::string& source_id,
                                           const std::string& destination_id);

// Creates an app availability request for |app_id| from |source_id| with
// ID |request_id|.
// TODO(imcheng): May not need |source_id|, just use sender-0?
CastMessage CreateGetAppAvailabilityRequest(const std::string& source_id,
                                            int request_id,
                                            const std::string& app_id);

// Possible results of a GET_APP_AVAILABILITY request.
enum class GetAppAvailabilityResult {
  kAvailable,
  kUnavailable,
  kUnknown,
};

// Extracts request ID from |payload| corresponding to a Cast message response.
// If request ID is available, assigns it to |request_id|. Return |true| if
// request ID is found.
bool GetRequestIdFromResponse(const base::Value& payload, int* request_id);

// Returns the GetAppAvailabilityResult corresponding to |app_id| in |payload|.
// Returns kUnknown if result is not found.
GetAppAvailabilityResult GetAppAvailabilityResultFromResponse(
    const base::Value& payload,
    const std::string& app_id);

}  // namespace cast_channel

#endif  // COMPONENTS_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_
