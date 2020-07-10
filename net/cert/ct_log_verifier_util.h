// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_LOG_VERIFIER_UTIL_H_
#define NET_CERT_CT_LOG_VERIFIER_UTIL_H_

#include <stdint.h>

#include <string>

#include "net/base/net_export.h"

namespace net {

namespace ct {

namespace internal {

// Hash |lh| and |rh| to produce a node hash according to
// http://tools.ietf.org/html/rfc6962#section-2.1
NET_EXPORT std::string HashNodes(const std::string& lh, const std::string& rh);

}  // namespace internal

}  // namespace ct

}  // namespace net

#endif  // NET_CERT_CT_LOG_VERIFIER_UTIL_H_
