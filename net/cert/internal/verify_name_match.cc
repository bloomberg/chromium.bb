// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/verify_name_match.h"
#include "net/der/input.h"

namespace net {

bool VerifyNameMatch(const der::Input& a, const der::Input& b) {
  // TODO(mattm): use normalization as specified in RFC 5280 section 7.
  return a.Equals(b);
}

}  // namespace net
