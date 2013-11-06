// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_STUB_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_STUB_H_

#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"

// Stub implementation of ImmersiveModeController for platforms which do not
// support immersive mode yet.
class ImmersiveModeControllerStub : public ImmersiveModeController {
 public:
  ImmersiveModeControllerStub();
  virtual ~ImmersiveModeControllerStub();

  // ImmersiveModeController overrides:
  virtual void Init(BrowserView* browser_view) OVERRIDE;
  virtual void SetEnabled(bool enabled) OVERRIDE;
  virtual bool IsEnabled() const OVERRIDE;
  virtual bool ShouldHideTabIndicators() const OVERRIDE;
  virtual bool ShouldHideTopViews() const OVERRIDE;
  virtual bool IsRevealed() const OVERRIDE;
  virtual int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const OVERRIDE;
  virtual ImmersiveRevealedLock* GetRevealedLock(
      AnimateReveal animate_reveal) OVERRIDE WARN_UNUSED_RESULT;
  virtual void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds_in_screen) OVERRIDE;
  virtual void SetupForTest() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerStub);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_STUB_H_
