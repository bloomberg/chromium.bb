// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_

#include <memory>

#include "ash/shell_observer.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/ui/ash/tablet_mode_client_observer.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"

class TabIconView;

namespace ash {
class FrameCaptionButtonContainerView;
class HeaderPainter;
}

// Provides the BrowserNonClientFrameView for Chrome OS.
class BrowserNonClientFrameViewAsh : public BrowserNonClientFrameView,
                                     public ash::ShellObserver,
                                     public TabletModeClientObserver,
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
  void UpdateMinimumSize() override;

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

  // TabletModeClientObserver:
  void OnTabletModeToggled(bool enabled) override;

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
                           ToggleTabletModeRelayout);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           AvatarDisplayOnTeleportedWindow);
  FRIEND_TEST_ALL_PREFIXES(ImmersiveModeControllerAshHostedAppBrowserTest,
                           FrameLayout);

  friend class BrowserHeaderPainterAsh;

  // Distance between the left edge of the NonClientFrameView and the tab strip.
  int GetTabStripLeftInset() const;

  // Distance between the right edge of the NonClientFrameView and the tab
  // strip.
  int GetTabStripRightInset() const;

  // Returns true if the header should be painted so that it looks the same as
  // the header used for packaged apps. Packaged apps use a different color
  // scheme than browser windows.
  bool UsePackagedAppHeaderStyle() const;

  void LayoutProfileIndicatorIcon();

  // Returns true if there is anything to paint. Some fullscreen windows do not
  // need their frames painted.
  bool ShouldPaint() const;

  // View which contains the window controls.
  ash::FrameCaptionButtonContainerView* caption_button_container_;

  // For popups, the window icon.
  TabIconView* window_icon_;

  // Helper class for painting the header.
  std::unique_ptr<ash::HeaderPainter> header_painter_;

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_
