// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_CUSTOM_FRAME_VIEW_ASH_H_
#define ASH_FRAME_CUSTOM_FRAME_VIEW_ASH_H_

#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/window/non_client_view.h"

namespace ash {
class FrameBorderHitTestController;
class FrameCaptionButtonContainerView;
class ImmersiveFullscreenController;
}
namespace views {
class Widget;
}

namespace ash {

// A NonClientFrameView used for packaged apps, dialogs and other non-browser
// windows. It supports immersive fullscreen. When in immersive fullscreen, the
// client view takes up the entire widget and the window header is an overlay.
// The window header overlay slides onscreen when the user hovers the mouse at
// the top of the screen. See also views::CustomFrameView and
// BrowserNonClientFrameViewAsh.
class ASH_EXPORT CustomFrameViewAsh : public views::NonClientFrameView {
 public:
  // Internal class name.
  static const char kViewClassName[];

  explicit CustomFrameViewAsh(views::Widget* frame);
  virtual ~CustomFrameViewAsh();

  // Inits |immersive_fullscreen_controller| so that the controller reveals
  // and hides |header_view_| in immersive fullscreen.
  // CustomFrameViewAsh does not take ownership of
  // |immersive_fullscreen_controller|.
  void InitImmersiveFullscreenControllerForView(
      ImmersiveFullscreenController* immersive_fullscreen_controller);

  // views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void UpdateWindowTitle() OVERRIDE;
  virtual void SizeConstraintsChanged() OVERRIDE;

  // views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual gfx::Size GetMaximumSize() const OVERRIDE;
  virtual void SchedulePaintInRect(const gfx::Rect& r) OVERRIDE;
  virtual void VisibilityChanged(views::View* starting_from,
                                 bool is_visible) OVERRIDE;

  // Get the view of the header.
  views::View* GetHeaderView();

  const views::View* GetAvatarIconViewForTest() const;

 private:
  class OverlayView;
  friend class TestWidgetConstraintsDelegate;

  // views::NonClientFrameView:
  virtual bool DoesIntersectRect(const views::View* target,
                                 const gfx::Rect& rect) const OVERRIDE;

  // Returns the container for the minimize/maximize/close buttons that is held
  // by the HeaderView. Used in testing.
  FrameCaptionButtonContainerView* GetFrameCaptionButtonContainerViewForTest();

  // Height from top of window to top of client area.
  int NonClientTopBorderHeight() const;

  // Not owned.
  views::Widget* frame_;

  // View which contains the title and window controls.
  class HeaderView;
  HeaderView* header_view_;

  // Updates the hittest bounds overrides based on the window state type.
  scoped_ptr<FrameBorderHitTestController> frame_border_hit_test_controller_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameViewAsh);
};

}  // namespace ash

#endif  // ASH_FRAME_CUSTOM_FRAME_VIEW_ASH_H_
