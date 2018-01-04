// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_
#define COMPONENTS_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_

#include <string>

namespace cast_channel {

class AuthContext;
class CastMessage;
class DeviceAuthMessage;

// Cast application protocol message types.
enum class CastMessageType { kOther, kPing, kPong };

// Checks if the contents of |message_proto| are valid.
bool IsCastMessageValid(const CastMessage& message_proto);

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

// Creates a keep-alive message of either type PING or PONG.
CastMessage CreateKeepAlivePingMessage();
CastMessage CreateKeepAlivePongMessage();

}  // namespace cast_channel

#endif  // COMPONENTS_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_
