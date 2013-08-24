// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_APP_NON_CLIENT_FRAME_VIEW_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_APP_NON_CLIENT_FRAME_VIEW_ASH_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

namespace aura {
class Window;
}

namespace ash {
class FrameCaptionButtonContainerView;
}

// NonClientFrameViewAsh implementation for maximized apps.
class AppNonClientFrameViewAsh : public BrowserNonClientFrameView {
 public:
  static const char kViewClassName[];  // visible for test
  static const char kControlWindowName[];  // visible for test

  AppNonClientFrameViewAsh(
      BrowserFrame* frame, BrowserView* browser_view);
  virtual ~AppNonClientFrameViewAsh();

  // NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(
      const gfx::Size& size,
      gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void UpdateWindowTitle() OVERRIDE;

  // BrowserNonClientFrameView:
  virtual gfx::Rect GetBoundsForTabStrip(
      views::View* tabstrip) const OVERRIDE;
  virtual TabStripInsets GetTabStripInsets(bool restored) const OVERRIDE;
  virtual int GetThemeBackgroundXInset() const OVERRIDE;
  virtual void UpdateThrobber(bool running) OVERRIDE;

  // views::View:
  virtual const char* GetClassName() const OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

 private:
  class FrameObserver;

  gfx::Rect GetControlBounds() const;

  // Closes |control_widget_|.
  void CloseControlWidget();

  // The View containing the restore and close buttons.
  ash::FrameCaptionButtonContainerView* control_view_;
  // The widget holding the control_view_.
  views::Widget* control_widget_;
  // Observer for browser frame close.
  scoped_ptr<FrameObserver> frame_observer_;

  DISALLOW_COPY_AND_ASSIGN(AppNonClientFrameViewAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_APP_NON_CLIENT_FRAME_VIEW_ASH_H_
