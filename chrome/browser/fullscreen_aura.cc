// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fullscreen.h"

#include "base/logging.h"

#if !defined(USE_ASH)

bool IsFullScreenMode() {
  // TODO(erg): An implementation here would have to check all existing
  // RootWindows instead of just recursively walking the Shell's RootWindow as
  // in the ash implementaiton.
  NOTIMPLEMENTED();
  return false;
}

#endif
