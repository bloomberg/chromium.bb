// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_POPUP_NON_CLIENT_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_POPUP_NON_CLIENT_FRAME_VIEW_H_
#pragma once

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

// BrowserNonClientFrameView implementation for popups. We let the window
// manager implementation render the decorations for popups, so this draws
// nothing.
class PopupNonClientFrameView : public BrowserNonClientFrameView {
 public:
  PopupNonClientFrameView() {}

  // NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual bool AlwaysUseCustomFrame() const OVERRIDE;
  virtual bool AlwaysUseNativeFrame() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask)
      OVERRIDE;
  virtual void EnableClose(bool enable) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;

  // BrowserNonClientFrameView:
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const OVERRIDE;
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const OVERRIDE;
  virtual void UpdateThrobber(bool running) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PopupNonClientFrameView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_POPUP_NON_CLIENT_FRAME_VIEW_H_
