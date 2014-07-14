// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_

#include "chrome/browser/ui/views/profiles/new_avatar_button.h"
#include "ui/views/window/non_client_view.h"

class AvatarLabel;
class AvatarMenuButton;
class BrowserFrame;
class BrowserView;
class NewAvatarButton;

// A specialization of the NonClientFrameView object that provides additional
// Browser-specific methods.
class BrowserNonClientFrameView : public views::NonClientFrameView {
 public:
  BrowserNonClientFrameView(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~BrowserNonClientFrameView();

  AvatarMenuButton* avatar_button() const { return avatar_button_; }

  NewAvatarButton* new_avatar_button() const { return new_avatar_button_; }

  AvatarLabel* avatar_label() const { return avatar_label_; }

  // Retrieves the bounds, in non-client view coordinates within which the
  // TabStrip should be laid out.
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const = 0;

  // Returns the inset of the topmost view in the client view from the top of
  // the non-client view. The topmost view depends on the window type. The
  // topmost view is the tab strip for tabbed browser windows, the toolbar for
  // popups, the web contents for app windows and varies for fullscreen windows.
  virtual int GetTopInset() const = 0;

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

  // Updates the title of the avatar button displayed in the caption area.
  // The button uses |style| to match the browser window style and notifies
  // |listener| when it is clicked.
  void UpdateNewStyleAvatarInfo(views::ButtonListener* listener,
                                const NewAvatarButton::AvatarButtonStyle style);

 private:
  // The frame that hosts this view.
  BrowserFrame* frame_;

  // The BrowserView hosted within this View.
  BrowserView* browser_view_;

  // Menu button that displays that either the incognito icon or the profile
  // icon.  May be NULL for some frame styles.
  AvatarMenuButton* avatar_button_;

  // Avatar label that is used for a supervised user.
  AvatarLabel* avatar_label_;

  // Menu button that displays the name of the active or guest profile.
  // May be NULL and will not be displayed for off the record profiles.
  NewAvatarButton* new_avatar_button_;
};

namespace chrome {

// Provided by a browser_non_client_frame_view_factory_*.cc implementation
BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
