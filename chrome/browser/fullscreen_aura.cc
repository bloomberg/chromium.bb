// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fullscreen.h"

#include "base/logging.h"
#include "ui/aura/desktop.h"
#include "ui/aura/window.h"

bool IsFullScreenMode() {
  return aura::Desktop::GetInstance()->window()->IsOrContainsFullscreenWindow();
}
