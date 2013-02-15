// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_TEST_UTIL_COMMON_H_
#define NET_SPDY_SPDY_TEST_UTIL_COMMON_H_

#include "net/spdy/spdy_protocol.h"

namespace net {

// Returns the SpdyPriority embedded in the given frame.  Returns true
// and fills in |priority| on success.
bool GetSpdyPriority(int version,
                     const SpdyFrame& frame,
                     SpdyPriority* priority);

}  // namespace net

#endif  // NET_SPDY_SPDY_TEST_UTIL_COMMON_H_
