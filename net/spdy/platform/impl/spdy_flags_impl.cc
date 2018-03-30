// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/platform/impl/spdy_flags_impl.h"

namespace net {

// If true, Http2FrameDecoderAdapter will pass decoded HTTP/2 SETTINGS through
// the SpdyFramerVisitorInterface callback OnSetting(), which will also accept
// unknown SETTINGS IDs.
bool http2_propagate_unknown_settings = true;

}  // namespace net
