// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/switches.h"

namespace cc {
namespace switches {

// On platforms where checkerboards are used, prefer background colors instead
// of checkerboards.
const char kBackgroundColorInsteadOfCheckerboard[] =
    "background-color-instead-of-checkerboard";

const char kDisableThreadedAnimation[]      = "disable-threaded-animation";

const char kEnablePartialSwap[]             = "enable-partial-swap";

const char kEnablePerTilePainting[]         = "enable-per-tile-painting";

// Enables an alternative pinch-zoom gesture support, via the threaded
// compositor.
const char kEnablePinchInCompositor[]       = "enable-pinch-in-compositor";

// When threaded compositing is turned on, wait until the entire frame has
// content (i.e. jank) instead of showing checkerboards for missing content.
const char kJankInsteadOfCheckerboard[] = "jank-instead-of-checkerboard";

}  // namespace switches
}  // namespace cc
