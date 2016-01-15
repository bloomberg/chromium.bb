// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#include "net/cookies/cookie_options.h"

namespace net {

CookieOptions::CookieOptions()
    : exclude_httponly_(true),
      include_first_party_only_cookies_(false),
      enforce_strict_secure_(false),
      server_time_() {}

}  // namespace net
