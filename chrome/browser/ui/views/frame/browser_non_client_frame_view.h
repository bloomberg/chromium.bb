// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_

#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "ui/views/window/non_client_view.h"

#if defined(FRAME_AVATAR_BUTTON)
#include "chrome/browser/ui/views/profiles/new_avatar_button.h"
#endif

#if defined(ENABLE_SUPERVISED_USERS)
class SupervisedUserAvatarLabel;
#endif
class AvatarMenuButton;
class BrowserFrame;
class BrowserView;

// A specialization of the NonClientFrameView object that provides additional
// Browser-specific methods.
class BrowserNonClientFrameView : public views::NonClientFrameView,
                                  public ProfileInfoCacheObserver {
 public:
  BrowserNonClientFrameView(BrowserFrame* frame, BrowserView* browser_view);
  ~BrowserNonClientFrameView() override;

  AvatarMenuButton* avatar_button() const { return avatar_button_; }

#if defined(FRAME_AVATAR_BUTTON)
  NewAvatarButton* new_avatar_button() const { return new_avatar_button_; }
#endif

#if defined(ENABLE_SUPERVISED_USERS)
  SupervisedUserAvatarLabel* supervised_user_avatar_label() const {
    return supervised_user_avatar_label_;
  }

  void OnThemeChanged() override;
#endif

  // Retrieves the bounds, in non-client view coordinates within which the
  // TabStrip should be laid out.
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const = 0;

  // Returns the inset of the topmost view in the client view from the top of
  // the non-client view. The topmost view depends on the window type. The
  // topmost view is the tab strip for tabbed browser windows, the toolbar for
  // popups, the web contents for app windows and varies for fullscreen windows.
  // If |restored| is true, this is calculated as if the window was restored,
  // regardless of its current state.
  virtual int GetTopInset(bool restored) const = 0;

  // Returns the amount that the theme background should be inset.
  virtual int GetThemeBackgroundXInset() const = 0;

  // Updates the throbber.
  virtual void UpdateThrobber(bool running) = 0;

  // Updates any toolbar components in the frame. The default implementation
  // does nothing.
  virtual void UpdateToolbar();

  // Returns the location icon, if this frame has any.
  virtual views::View* GetLocationIconView() const;

  // Overriden from views::View.
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  void ChildPreferredSizeChanged(View* child) override;

 protected:
  BrowserView* browser_view() const { return browser_view_; }
  BrowserFrame* frame() const { return frame_; }

  // Whether the frame should be painted with theming.
  // By default, tabbed browser windows are themed but popup and app windows are
  // not.
  virtual bool ShouldPaintAsThemed() const;

  // Compute aspects of the frame needed to paint the frame background.
  SkColor GetFrameColor() const;
  gfx::ImageSkia* GetFrameImage() const;
  gfx::ImageSkia* GetFrameOverlayImage() const;
  int GetTopAreaHeight() const;

  // Updates the avatar button using the old or new UI based on the BrowserView
  // type, and the presence of the --enable-new-avatar-menu flag. Calls either
  // UpdateOldAvatarButton() or UpdateNewAvatarButtonImpl() accordingly.
  void UpdateAvatar();

  // Updates the title and icon of the old avatar button.
  void UpdateOldAvatarButton();

  // Updates the avatar button displayed in the caption area by calling
  // UpdateNewAvatarButton() with an implementation specific |listener|
  // and button |style|.
  virtual void UpdateNewAvatarButtonImpl() = 0;

#if defined(FRAME_AVATAR_BUTTON)
  // Updates the title of the avatar button displayed in the caption area.
  // The button uses |style| to match the browser window style and notifies
  // |listener| when it is clicked.
  void UpdateNewAvatarButton(views::ButtonListener* listener,
                             const NewAvatarButton::AvatarButtonStyle style);
#endif

 private:
  // Overriden from ProfileInfoCacheObserver.
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;

  // Draws a taskbar icon if avatars are enabled, erases it otherwise.
  void UpdateTaskbarDecoration();

  // The frame that hosts this view.
  BrowserFrame* frame_;

  // The BrowserView hosted within this View.
  BrowserView* browser_view_;

#if defined(ENABLE_SUPERVISED_USERS)
  SupervisedUserAvatarLabel* supervised_user_avatar_label_;
#endif

#if defined(FRAME_AVATAR_BUTTON)
  // Menu button that displays the name of the active or guest profile.
  // May be null and will not be displayed for off the record profiles.
  NewAvatarButton* new_avatar_button_;
#endif

  // Menu button that displays the incognito icon. May be null for some frame
  // styles. TODO(anthonyvd): simplify/rename.
  AvatarMenuButton* avatar_button_;
};

namespace chrome {

// Provided by a browser_non_client_frame_view_factory_*.cc implementation
BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
