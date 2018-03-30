// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_PLATFORM_IMPL_SPDY_FLAGS_IMPL_H_
#define NET_SPDY_PLATFORM_IMPL_SPDY_FLAGS_IMPL_H_

#include "net/base/net_export.h"

namespace net {

NET_EXPORT_PRIVATE extern bool http2_propagate_unknown_settings;

inline bool GetSpdyReloadableFlagImpl(bool flag) {
  return flag;
}

inline bool GetSpdyRestartFlagImpl(bool flag) {
  return flag;
}

}  // namespace net

#endif  // NET_SPDY_PLATFORM_IMPL_SPDY_FLAGS_IMPL_H_
