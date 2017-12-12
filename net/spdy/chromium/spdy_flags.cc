// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/chromium/spdy_flags.h"

namespace net {

// Deliver OnPaddingLength separately from OnPadding.
bool FLAGS_chromium_http2_flag_h2_on_stream_pad_length = true;

}  // namespace net
