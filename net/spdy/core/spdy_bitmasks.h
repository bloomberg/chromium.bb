// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_CORE_SPDY_BITMASKS_H_
#define NET_SPDY_CORE_SPDY_BITMASKS_H_

namespace net {

// StreamId mask from the SpdyHeader
const unsigned int kStreamIdMask = 0x7fffffff;

// Mask the lower 24 bits.
const unsigned int kLengthMask = 0xffffff;

}  // namespace net

#endif  // NET_SPDY_CORE_SPDY_BITMASKS_H_
