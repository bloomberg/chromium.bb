// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/canonical_cookie_hash.h"

#include "base/hash.h"

namespace canonical_cookie {

size_t FastHash(const net::CanonicalCookie& cookie) {
  return base::SuperFastHash(cookie.Name().c_str(), cookie.Name().size()) +
         3 * base::SuperFastHash(cookie.Domain().c_str(),
                                 cookie.Domain().size()) +
         7 * base::SuperFastHash(cookie.Path().c_str(), cookie.Path().size());
}

};  // namespace canonical_cookie
