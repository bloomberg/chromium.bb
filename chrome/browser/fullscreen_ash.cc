// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fullscreen.h"

#include "ash/wm/window_util.h"

bool IsFullScreenMode() {
  // This is used only by notification_ui_manager.cc. On aura, notification
  // will be managed in panel. This is temporary to get certain feature running
  // until we implement it for aura.
  return ash::wm::IsActiveWindowFullscreen();
}
