// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/platform/impl/spdy_flags_impl.h"

namespace net {

// Consider SETTINGS identifier 0x07 as invalid.
bool http2_check_settings_id_007 = true;

}  // namespace net
