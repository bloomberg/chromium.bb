// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FULLSCREEN_H_
#define CHROME_BROWSER_FULLSCREEN_H_
#pragma once

#include "build/build_config.h"

// For MacOSX, InitFullScreenMonitor needs to be called first to setup the
// monitor. StopFullScreenMonitor should be called if it is not needed any more.
#if defined(OS_MACOSX)
void InitFullScreenMonitor();
void StopFullScreenMonitor();
#endif

bool IsFullScreenMode();

#endif  // CHROME_BROWSER_FULLSCREEN_H_
