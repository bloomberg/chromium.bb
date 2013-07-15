// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FULLSCREEN_H_
#define CHROME_BROWSER_FULLSCREEN_H_

#include "build/build_config.h"

bool IsFullScreenMode();

#if defined(OS_MACOSX)
namespace chrome {
namespace mac {
// Returns true if this system supports Lion-style system fullscreen.
bool SupportsSystemFullscreen();
}  // namespace mac
}  // namespace chrome
#endif

#endif  // CHROME_BROWSER_FULLSCREEN_H_
