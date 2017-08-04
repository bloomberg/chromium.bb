// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/browser/ui/views/frame/avatar_button_manager.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/windows_10_caption_button.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/non_client_view.h"

class BrowserView;
class TabStrip;

class GlassBrowserFrameView : public BrowserNonClientFrameView,
                              public views::ButtonListener,
                              public TabIconViewModel,
                              public TabStripObserver {
 public:
  // Constructs a non-client view for an BrowserFrame.
  GlassBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  ~GlassBrowserFrameView() override;

  // BrowserNonClientFrameView:
  gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateThrobber(bool running) override;
  gfx::Size GetMinimumSize() const override;
  views::View* GetProfileSwitcherView() const override;
  void OnBrowserViewInitViewsComplete() override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {}
  void ResetWindowControls() override {}
  void SizeConstraintsChanged() override {}

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // TabIconViewModel:
  bool ShouldTabIconViewAnimate() const override;
  gfx::ImageSkia GetFaviconForTabIconView() override;

  // TabStripObserver:
  void TabStripMaxXChanged(TabStrip* tab_strip) override;
  void TabStripDeleted(TabStrip* tab_strip) override;
  void TabStripRemovedTabAt(TabStrip* tab_strip, int index) override;

  bool IsMaximized() const;

  // Visual height of the titlebar when the window is maximized (i.e. excluding
  // the area above the top of the screen).
  int TitlebarMaximizedVisualHeight() const;

  SkColor GetTitlebarColor() const;

 protected:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;

  // BrowserNonClientFrameView:
  void UpdateProfileIcons() override;

 private:
  // views::NonClientFrameView:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // Returns the thickness of the border around the client area (web content,
  // toolbar, and tabs) that separates it from the frame border. If |restored|
  // is true, this is calculated as if the window was restored, regardless of
  // its current state.
  int ClientBorderThickness(bool restored) const;

  // Returns the thickness of the window border for the left, right, and bottom
  // edges of the frame. On Windows 10 this is a mostly-transparent handle that
  // allows you to resize the window.
  int FrameBorderThickness() const;

  // Returns the thickness of the window border for the top edge of the frame,
  // which is sometimes different than FrameBorderThickness(). Does not include
  // the titlebar/tabstrip area. If |restored| is true, this is calculated as if
  // the window was restored, regardless of its current state.
  int FrameTopBorderThickness(bool restored) const;
  int FrameTopBorderThicknessPx(bool restored) const;

  // Returns the height of everything above the tabstrip's hit-test region,
  // including both the window border (i.e. FrameTopBorderThickness()) and any
  // additional draggable area that's considered part of the window frame rather
  // than the tabstrip. If |restored| is true, this is calculated as if the
  // window was restored, regardless of its current state.
  int TopAreaHeight(bool restored) const;

  // Returns the height of the titlebar for popups or other browser types that
  // don't have tabs.
  int TitlebarHeight(bool restored) const;

  // Returns the y coordinate for the top of the frame, which in tablet mode
  // is the top of the screen and in restored mode is 1 pixel below the top of
  // the window to leave room for the visual border that Windows draws.
  int WindowTopY() const;

  // Returns the distance from the leading edge of the window to the leading
  // edge of the caption buttons.
  int MinimizeButtonX() const;

  // Returns whether the toolbar is currently visible.
  bool IsToolbarVisible() const;

  // Returns whether the caption buttons are drawn at the leading edge (i.e. the
  // left in LTR mode, or the right in RTL mode).
  bool CaptionButtonsOnLeadingEdge() const;

  bool ShowCustomIcon() const;
  bool ShowCustomTitle() const;
  bool ShowSystemIcon() const;

  Windows10CaptionButton* CreateCaptionButton(ViewID button_type);

  // Paint various sub-components of this view.
  void PaintTitlebar(gfx::Canvas* canvas) const;
  void PaintClientEdge(gfx::Canvas* canvas) const;
  void FillClientEdgeRects(int x,
                           int y,
                           int right,
                           int bottom,
                           SkColor color,
                           gfx::Canvas* canvas) const;

  // Layout various sub-components of this view.
  void LayoutIncognitoIcon();
  void LayoutTitleBar();
  void LayoutProfileSwitcher();
  void LayoutCaptionButtons();
  void LayoutCaptionButton(Windows10CaptionButton* button,
                           int previous_button_x);
  void LayoutClientView();

  // Returns the insets of the client area. If |restored| is true, this is
  // calculated as if the window was restored, regardless of its current state.
  gfx::Insets GetClientAreaInsets(bool restored) const;

  // Returns the bounds of the client area for the specified view size.
  gfx::Rect CalculateClientAreaBounds() const;

  // Starts/Stops the window throbber running.
  void StartThrobber();
  void StopThrobber();

  // Displays the next throbber frame.
  void DisplayNextThrobberFrame();

  // The layout rect of the incognito icon, if visible.
  gfx::Rect incognito_bounds_;

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  // The small icon created from the bitmap image of the window icon.
  base::win::ScopedHICON small_window_icon_;

  // The big icon created from the bitmap image of the window icon.
  base::win::ScopedHICON big_window_icon_;

  // Icon and title. Only used when custom-drawing the titlebar for popups.
  TabIconView* window_icon_;
  views::Label* window_title_;

  // Wrapper around the in-frame profile switcher.
  AvatarButtonManager profile_switcher_;

  // Custom-drawn caption buttons. Only used when custom-drawing the titlebar.
  Windows10CaptionButton* minimize_button_;
  Windows10CaptionButton* maximize_button_;
  Windows10CaptionButton* restore_button_;
  Windows10CaptionButton* close_button_;

  // Whether or not the window throbber is currently animating.
  bool throbber_running_;

  // The index of the current frame of the throbber animation.
  int throbber_frame_;

  // The window's tabstrip, if any, is observed so we know when to resize any
  // avatar button.
  ScopedObserver<TabStrip, GlassBrowserFrameView> tab_strip_observer_;

  static const int kThrobberIconCount = 24;
  static HICON throbber_icons_[kThrobberIconCount];
  static void InitThrobberIcons();

  DISALLOW_COPY_AND_ASSIGN(GlassBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
