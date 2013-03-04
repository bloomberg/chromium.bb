// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_OVERLAY_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/instant/instant_overlay_model_observer.h"

class Browser;
class InstantOverlayModel;

// Abstract base class for platform-specific Instant overlay controllers.
class InstantOverlayController : public InstantOverlayModelObserver {
 protected:
  explicit InstantOverlayController(Browser* browser);
  virtual ~InstantOverlayController();

  Browser* const browser_;

 private:
  // Overridden from InstantOverlayModelObserver:
  virtual void OverlayStateChanged(
      const InstantOverlayModel& model) OVERRIDE = 0;
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_OVERLAY_CONTROLLER_H_
