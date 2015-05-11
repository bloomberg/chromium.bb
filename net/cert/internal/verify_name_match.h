// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_VERIFY_NAME_MATCH_H_
#define NET_CERT_INTERNAL_VERIFY_NAME_MATCH_H_

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace net {

namespace der {
class Input;
}  // namespace der

// Compare DER-encoded X.501 Name values according to RFC 5280 rules.
// TODO(mattm): Doesn't actually implement RFC 5280 matching yet.
// Returns true if |a| and |b| match.
NET_EXPORT bool VerifyNameMatch(const der::Input& a, const der::Input& b);

}  // namespace net

#endif  // NET_CERT_INTERNAL_VERIFY_NAME_MATCH_H_
