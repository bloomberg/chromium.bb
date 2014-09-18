// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/logger_util.h"
#include "net/base/net_errors.h"

namespace extensions {
namespace core_api {
namespace cast_channel {

LastErrors::LastErrors()
    : event_type(proto::EVENT_TYPE_UNKNOWN),
      challenge_reply_error_type(proto::CHALLENGE_REPLY_ERROR_NONE),
      net_return_value(net::OK),
      nss_error_code(0) {
}

LastErrors::~LastErrors() {
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
