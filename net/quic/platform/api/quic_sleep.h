// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_API_QUIC_SLEEP_H_
#define NET_QUIC_PLATFORM_API_QUIC_SLEEP_H_

#include "net/quic/core/quic_time.h"
#include "net/quic/platform/impl/quic_sleep_impl.h"

namespace net {

inline void QuicSleep(QuicTime::Delta duration) {
  QuicSleepImpl(duration);
}

}  // namespace net

#endif  // NET_QUIC_PLATFORM_API_QUIC_SLEEP_H_
