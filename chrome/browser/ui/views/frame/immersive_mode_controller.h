// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_H_

#include "base/compiler_specific.h"

class BrowserView;

// Controller for an "immersive mode" similar to MacOS presentation mode where
// the top-of-window views are hidden until the mouse hits the top of the
// screen. The tab strip is optionally painted with miniature "tab indicator"
// rectangles.
// Currently, immersive mode is only available for Chrome OS.
class ImmersiveModeController {
 public:
  // Base class for a lock which keeps the top-of-window views revealed for the
  // duration of its lifetime. See GetRevealedLock() for more details.
  class RevealedLock {
   public:
    virtual ~RevealedLock() {}
  };

  virtual ~ImmersiveModeController() {}

  // Must initialize after browser view has a Widget and native window.
  virtual void Init(BrowserView* browser_view) = 0;

  // Enables or disables immersive mode.
  virtual void SetEnabled(bool enabled) = 0;
  virtual bool IsEnabled() const = 0;

  // True if the miniature "tab indicators" should be hidden in the main browser
  // view when immersive mode is enabled.
  virtual bool ShouldHideTabIndicators() const = 0;

  // True when the top views are hidden due to immersive mode.
  virtual bool ShouldHideTopViews() const = 0;

  // True when the top views are fully or partially visible.
  virtual bool IsRevealed() const = 0;

  // If the controller is temporarily revealing the top views ensures that
  // the reveal view's layer is on top and hence visible over web contents.
  virtual void MaybeStackViewAtTop() = 0;

  // Shows the reveal view if immersive mode is enabled. Used when focus is
  // placed in the location bar, tools menu, etc.
  virtual void MaybeStartReveal() = 0;

  // Immediately hides the reveal view, without animating.
  virtual void CancelReveal() = 0;

  // Returns a lock which will keep the top-of-window views revealed for its
  // lifetime. Several locks can be obtained. When all of the locks are
  // destroyed, if immersive mode is enabled and there is nothing else keeping
  // the top-of-window views revealed, the top-of-window views will be closed.
  // This method always returns a valid lock regardless of whether immersive
  // mode is enabled. The lock's lifetime can span immersive mode being
  // enabled / disabled.
  // The caller takes ownership of the returned lock.
  virtual RevealedLock* GetRevealedLock() WARN_UNUSED_RESULT = 0;
};

namespace chrome {

// Returns true if immersive mode should be used for fullscreen based on
// command line flags.
// Implemented in immersive_mode_controller_factory.cc.
bool UseImmersiveFullscreen();

// Implemented in immersive_mode_controller_factory.cc.
ImmersiveModeController* CreateImmersiveModeController();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_H_
