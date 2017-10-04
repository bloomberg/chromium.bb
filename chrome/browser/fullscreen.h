// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FULLSCREEN_H_
#define CHROME_BROWSER_FULLSCREEN_H_

#include <stdint.h>

#include "build/build_config.h"

// |display_id| is used in OS_CHROMEOS build config only, ignored otherwise.
bool IsFullScreenMode(int64_t display_id);

#endif  // CHROME_BROWSER_FULLSCREEN_H_
