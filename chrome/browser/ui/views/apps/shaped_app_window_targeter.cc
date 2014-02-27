// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/shaped_app_window_targeter.h"

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"
#include "ui/gfx/path.h"

ShapedAppWindowTargeter::ShapedAppWindowTargeter(
    aura::Window* window,
    ChromeNativeAppWindowViews* app_window)
    : wm::MaskedWindowTargeter(window), app_window_(app_window) {}

ShapedAppWindowTargeter::~ShapedAppWindowTargeter() {
}

bool ShapedAppWindowTargeter::GetHitTestMask(aura::Window* window,
                                             gfx::Path* mask) const {
  SkRegion* shape = app_window_->shape();
  return shape ? shape->getBoundaryPath(mask) : false;
}
