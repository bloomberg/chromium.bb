// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_

#include "ash/shell_observer.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "ui/views/controls/button/button.h"

class TabIconView;

namespace ash {
class FrameBorderHitTestController;
class FrameCaptionButton;
class FrameCaptionButtonContainerView;
class HeaderPainter;
}
namespace views {
class ImageButton;
class ToggleImageButton;
}

class BrowserNonClientFrameViewAsh : public BrowserNonClientFrameView,
                                     public ash::ShellObserver,
                                     public chrome::TabIconViewModel,
                                     public CommandObserver,
                                     public views::ButtonListener {
 public:
  static const char kViewClassName[];

  BrowserNonClientFrameViewAsh(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~BrowserNonClientFrameViewAsh();

  void Init();

  // BrowserNonClientFrameView:
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const OVERRIDE;
  virtual int GetTopInset() const OVERRIDE;
  virtual int GetThemeBackgroundXInset() const OVERRIDE;
  virtual void UpdateThrobber(bool running) OVERRIDE;

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
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;

  // ash::ShellObserver:
  virtual void OnMaximizeModeStarted() OVERRIDE;
  virtual void OnMaximizeModeEnded() OVERRIDE;

  // chrome::TabIconViewModel:
  virtual bool ShouldTabIconViewAnimate() const OVERRIDE;
  virtual gfx::ImageSkia GetFaviconForTabIconView() OVERRIDE;

  // CommandObserver:
  virtual void EnabledStateChangedForCommand(int id, bool enabled) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest, WindowHeader);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           NonImmersiveFullscreen);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           ImmersiveFullscreen);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           ToggleMaximizeModeRelayout);

  // views::NonClientFrameView:
  virtual bool DoesIntersectRect(const views::View* target,
                                 const gfx::Rect& rect) const OVERRIDE;

  // Distance between the left edge of the NonClientFrameView and the tab strip.
  int GetTabStripLeftInset() const;

  // Distance between the right edge of the NonClientFrameView and the tab
  // strip.
  int GetTabStripRightInset() const;

  // Returns true if we should use a super short header with light bars instead
  // of regular tabs. This header is used in immersive fullscreen when the
  // top-of-window views are not revealed.
  bool UseImmersiveLightbarHeaderStyle() const;

  // Returns true if the header should be painted so that it looks the same as
  // the header used for packaged apps. Packaged apps use a different color
  // scheme than browser windows.
  bool UsePackagedAppHeaderStyle() const;

  // Returns true if the header should be painted with a WebApp header style.
  // The WebApp header style has a back button and title along with the usual
  // accoutrements.
  bool UseWebAppHeaderStyle() const;

  // Layout the incognito icon.
  void LayoutAvatar();

  // Returns true if there is anything to paint. Some fullscreen windows do not
  // need their frames painted.
  bool ShouldPaint() const;

  // Paints the header background when the frame is in immersive fullscreen and
  // tab light bar is visible.
  void PaintImmersiveLightbarStyleHeader(gfx::Canvas* canvas);

  void PaintToolbarBackground(gfx::Canvas* canvas);

  // Draws the line under the header for windows without a toolbar and not using
  // the packaged app header style.
  void PaintContentEdge(gfx::Canvas* canvas);

  // Update the state of the back button.
  void UpdateBackButtonState(bool enabled);

  // View which contains the window controls.
  ash::FrameCaptionButtonContainerView* caption_button_container_;

  // The back button for web app style header.
  ash::FrameCaptionButton* web_app_back_button_;

  // For popups, the window icon.
  TabIconView* window_icon_;

  // Helper class for painting the header.
  scoped_ptr<ash::HeaderPainter> header_painter_;

  // Updates the hittest bounds overrides based on the window show type.
  scoped_ptr<ash::FrameBorderHitTestController>
      frame_border_hit_test_controller_;

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_
