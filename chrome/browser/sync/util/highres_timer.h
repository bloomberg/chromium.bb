// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// High resolution timer functions defined for each OS.

#if defined(OS_WINDOWS)
#include "chrome/browser/sync/util/highres_timer-win32.h"
#elif defined(OS_MACOSX)
#error "Mac timer functions are missing."
#else
#include "chrome/browser/sync/util/highres_timer-linux.h"
#endif
