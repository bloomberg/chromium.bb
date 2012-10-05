// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/switches.h"

namespace cc {
namespace switches {

// When threaded compositing is turned on, wait until the entire frame has
// content (i.e. jank) instead of showing checkerboards for missing content.
const char kJankInsteadOfCheckerboard[] = "jank-instead-of-checkerboard";

// On platforms where checkerboards are used, prefer background colors instead
// of checkerboards.
const char kBackgroundColorInsteadOfCheckerboard[] =
    "background-color-instead-of-checkerboard";

}  // namespace switches
}  // namespace cc
