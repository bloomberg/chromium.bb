// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cast_channel/cast_auth_util.h"

#include "base/logging.h"

namespace extensions {
namespace api {
namespace cast_channel {

bool AuthenticateChallengeReply(const CastMessage& challenge_reply,
                                const std::string& peer_cert) {
  NOTREACHED();
  return false;
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
