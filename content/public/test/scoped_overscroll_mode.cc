// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/scoped_overscroll_mode.h"

namespace content {

ScopedOverscrollMode::ScopedOverscrollMode(OverscrollConfig::Mode mode) {
  OverscrollConfig::SetMode(mode);
}

ScopedOverscrollMode::~ScopedOverscrollMode() {
  OverscrollConfig::ResetMode();
}

}  // namespace content
