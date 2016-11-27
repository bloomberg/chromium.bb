// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MUS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MUS_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/frame/avatar_button_manager.h"
#endif

class TabIconView;
class WebAppLeftHeaderView;

namespace ui {
class Window;
}

class BrowserNonClientFrameViewMus : public BrowserNonClientFrameView,
                                     public TabIconViewModel,
                                     public TabStripObserver {
 public:
  static const char kViewClassName[];

  BrowserNonClientFrameViewMus(BrowserFrame* frame, BrowserView* browser_view);
  ~BrowserNonClientFrameViewMus() override;

  void Init();

  // BrowserNonClientFrameView:
  void OnBrowserViewInitViewsComplete() override;
  gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateThrobber(bool running) override;
  void UpdateToolbar() override;
  views::View* GetLocationIconView() const override;
  views::View* GetProfileSwitcherView() const override;

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

  // TabIconViewModel:
  bool ShouldTabIconViewAnimate() const override;
  gfx::ImageSkia GetFaviconForTabIconView() override;

 protected:
  // BrowserNonClientFrameView:
  void UpdateProfileIcons() override;

 private:
  ui::Window* mus_window();

  // Resets the client area on the ui::Window.
  void UpdateClientArea();

  // TabStripObserver:
  void TabStripMaxXChanged(TabStrip* tab_strip) override;
  void TabStripDeleted(TabStrip* tab_strip) override;

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

  // Layout the incognito button.
  void LayoutIncognitoButton();

  // Layout the profile switcher (if there is one).
  void LayoutProfileSwitcher();

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

  // TODO(sky): Figure out how to support WebAppLeftHeaderView.

  // For popups, the window icon.
  TabIconView* window_icon_;

#if !defined(OS_CHROMEOS)
  // Wrapper around the in-frame avatar switcher.
  AvatarButtonManager profile_switcher_;
#endif

  TabStrip* tab_strip_;

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewMus);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MUS_H_
