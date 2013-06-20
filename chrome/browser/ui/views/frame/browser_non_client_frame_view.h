// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_

#include "ui/views/window/non_client_view.h"

class AvatarLabel;
class AvatarMenuButton;
class BrowserFrame;
class BrowserView;

// A specialization of the NonClientFrameView object that provides additional
// Browser-specific methods.
class BrowserNonClientFrameView : public views::NonClientFrameView {
 public:
  // Insets around the tabstrip.
  struct TabStripInsets {
    TabStripInsets() : top(0), left(0), right(0) {}
    TabStripInsets(int top, int left, int right)
        : top(top),
          left(left),
          right(right) {}

    int top;
    int left;
    int right;
  };

  BrowserNonClientFrameView(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~BrowserNonClientFrameView();

  AvatarMenuButton* avatar_button() const { return avatar_button_; }

  AvatarLabel* avatar_label() const { return avatar_label_; }

  // Returns the bounds within which the TabStrip should be laid out.
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const = 0;

  // Returns the TabStripInsets within the window at which the tab strip is
  // positioned. If |as_restored| is true, this is calculated as if we were in
  // restored mode regardless of the current mode.
  virtual TabStripInsets GetTabStripInsets(bool force_restored) const = 0;

  // Returns the amount that the theme background should be inset.
  virtual int GetThemeBackgroundXInset() const = 0;

  // Updates the throbber.
  virtual void UpdateThrobber(bool running) = 0;

  // Overriden from views::View.
  virtual void VisibilityChanged(views::View* starting_from,
                                 bool is_visible) OVERRIDE;
  virtual void OnThemeChanged() OVERRIDE;

 protected:
  BrowserView* browser_view() const { return browser_view_; }
  BrowserFrame* frame() const { return frame_; }

  // Updates the title and icon of the avatar button.
  void UpdateAvatarInfo();

 private:
  // The frame that hosts this view.
  BrowserFrame* frame_;

  // The BrowserView hosted within this View.
  BrowserView* browser_view_;

  // Menu button that displays that either the incognito icon or the profile
  // icon.  May be NULL for some frame styles.
  AvatarMenuButton* avatar_button_;

  // Avatar label that is used for a managed user.
  AvatarLabel* avatar_label_;
};

namespace chrome {

// Provided by a browser_non_client_frame_view_factory_*.cc implementation
BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
