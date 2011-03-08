// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/views/frame/browser_frame_win.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "views/controls/button/button.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/window/non_client_view.h"

class BrowserView;
class SkBitmap;

namespace views {
class ProfileMenuButton;
class ProfileMenuModel;
class ProfileTagView;
}

class GlassBrowserFrameView : public BrowserNonClientFrameView,
                              public NotificationObserver,
                              public views::ViewMenuDelegate {
 public:
  // Constructs a non-client view for an BrowserFrame.
  GlassBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~GlassBrowserFrameView();

  // Overridden from BrowserNonClientFrameView:
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const;
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const;
  virtual void UpdateThrobber(bool running);

  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const;
  virtual bool AlwaysUseNativeFrame() const;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) { }
  virtual void EnableClose(bool enable) { }
  virtual void ResetWindowControls() { }

  // views::ViewMenuDelegate implementation:
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

 protected:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas);
  virtual void Layout();

 private:
  // Returns the thickness of the border that makes up the window frame edges.
  // This does not include any client edge.
  int FrameBorderThickness() const;

  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge.
  int NonClientBorderThickness() const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.  If |restored| is
  // true, acts as if the window is restored regardless of the real mode.  If
  // |ignore_vertical_tabs| is true, acts as if vertical tabs are off regardless
  // of the real state.
  int NonClientTopBorderHeight(bool restored, bool ignore_vertical_tabs) const;

  // Paint various sub-components of this view.
  void PaintToolbarBackground(gfx::Canvas* canvas);
  void PaintOTRAvatar(gfx::Canvas* canvas);
  void PaintRestoredClientEdge(gfx::Canvas* canvas);

  // Layout various sub-components of this view.
  void LayoutOTRAvatar();
  void LayoutClientView();
  void LayoutProfileTag();

  // Returns the bounds of the client area for the specified view size.
  gfx::Rect CalculateClientAreaBounds(int width, int height) const;

  // Starts/Stops the window throbber running.
  void StartThrobber();
  void StopThrobber();

  // Displays the next throbber frame.
  void DisplayNextThrobberFrame();

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Receive notifications when the user's Google services user name changes.
  void RegisterLoginNotifications();

  // Returns true if the ProfileButton has been created.
  bool show_profile_button() const { return profile_button_.get() != NULL; }

  // The layout rect of the OTR avatar icon, if visible.
  gfx::Rect otr_avatar_bounds_;

  // The frame that hosts this view.
  BrowserFrame* frame_;

  // The BrowserView hosted within this View.
  BrowserView* browser_view_;

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  // Menu button that displays user's name and multi-profile menu.
  scoped_ptr<views::ProfileMenuButton> profile_button_;

  // Image tag displayed on frame beneath profile_button_.
  scoped_ptr<views::ProfileTagView> profile_tag_;

  // Multi-profile menu for profile_button_.
  scoped_ptr<views::ProfileMenuModel> profile_menu_model_;

  // Whether or not the window throbber is currently animating.
  bool throbber_running_;

  // The index of the current frame of the throbber animation.
  int throbber_frame_;

  // The Google services user name associated with this BrowserView's profile.
  StringPrefMember username_pref_;

  static const int kThrobberIconCount = 24;
  static HICON throbber_icons_[kThrobberIconCount];
  static void InitThrobberIcons();

  DISALLOW_COPY_AND_ASSIGN(GlassBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
