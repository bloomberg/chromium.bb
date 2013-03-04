// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_overlay_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_instant_controller.h"

InstantOverlayController::InstantOverlayController(Browser* browser)
    : browser_(browser) {
  if (browser_->instant_controller())
    browser_->instant_controller()->instant()->model()->AddObserver(this);
}

InstantOverlayController::~InstantOverlayController() {
  if (browser_->instant_controller())
    browser_->instant_controller()->instant()->model()->RemoveObserver(this);
}
