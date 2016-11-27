// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_

#include <memory>

#include "ash/common/shell_observer.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"

class TabIconView;
class WebAppLeftHeaderView;

namespace ash {
class FrameCaptionButtonContainerView;
class HeaderPainter;
}

class BrowserNonClientFrameViewAsh : public BrowserNonClientFrameView,
                                     public ash::ShellObserver,
                                     public TabIconViewModel {
 public:
  BrowserNonClientFrameViewAsh(BrowserFrame* frame, BrowserView* browser_view);
  ~BrowserNonClientFrameViewAsh() override;

  void Init();

  // BrowserNonClientFrameView:
  gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateThrobber(bool running) override;
  void UpdateToolbar() override;
  views::View* GetLocationIconView() const override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;
  const char* GetClassName() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size GetMinimumSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // ash::ShellObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnded() override;
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnded() override;

  // TabIconViewModel:
  bool ShouldTabIconViewAnimate() const override;
  gfx::ImageSkia GetFaviconForTabIconView() override;

 protected:
  // BrowserNonClientFrameView:
  void UpdateProfileIcons() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest, WindowHeader);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           NonImmersiveFullscreen);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           ImmersiveFullscreen);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           ToggleMaximizeModeRelayout);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           AvatarDisplayOnTeleportedWindow);
  FRIEND_TEST_ALL_PREFIXES(WebAppLeftHeaderViewTest, BackButton);
  FRIEND_TEST_ALL_PREFIXES(WebAppLeftHeaderViewTest, LocationIcon);
  friend class BrowserHeaderPainterAsh;

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

  void LayoutProfileIndicatorIcon();

  // Returns true if there is anything to paint. Some fullscreen windows do not
  // need their frames painted.
  bool ShouldPaint() const;

  void PaintToolbarBackground(gfx::Canvas* canvas);

  // View which contains the window controls.
  ash::FrameCaptionButtonContainerView* caption_button_container_;

  // The holder for the buttons on the left side of the header. This is included
  // for web app style frames, and includes a back button and location icon.
  WebAppLeftHeaderView* web_app_left_header_view_;

  // For popups, the window icon.
  TabIconView* window_icon_;

  // Helper class for painting the header.
  std::unique_ptr<ash::HeaderPainter> header_painter_;

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_
