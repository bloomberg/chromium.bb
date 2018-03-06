// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_PLATFORM_API_SPDY_FLAGS_H_
#define NET_SPDY_PLATFORM_API_SPDY_FLAGS_H_

#include "net/spdy/platform/impl/spdy_flags_impl.h"

#define GetSpdyReloadableFlag(flag) GetSpdyReloadableFlagImpl(flag)
#define GetSpdyRestartFlag(flag) GetSpdyRestartFlagImpl(flag)

#endif  // NET_SPDY_PLATFORM_API_SPDY_FLAGS_H_
