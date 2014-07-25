// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_UTIL_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_UTIL_H_

#include <string>

namespace extensions {
namespace core_api {
namespace cast_channel {

class CastMessage;

// Authenticates the given |challenge_reply|:
// 1. Signature contained in the reply is valid.
// 2. Certficate used to sign is rooted to a trusted CA.
bool AuthenticateChallengeReply(const CastMessage& challenge_reply,
                                const std::string& peer_cert);

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_UTIL_H_
