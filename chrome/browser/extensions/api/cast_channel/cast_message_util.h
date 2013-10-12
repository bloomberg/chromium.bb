// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_

#include <string>

namespace extensions {
namespace api {
namespace cast_channel {

class CastMessage;
struct MessageInfo;

// Fills |message_proto| from |message| and returns true on success.
bool MessageInfoToCastMessage(const MessageInfo& message,
                              CastMessage* message_proto);

// Fills |message| from |message_proto| and returns true on success.
bool CastMessageToMessageInfo(const CastMessage& message_proto,
                              MessageInfo* message);

// Returns a human readable string for |message_proto|.
const std::string MessageProtoToString(const CastMessage& message_proto);

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CAST_CHANNEL_CAST_MESSAGE_UTIL_H_
