// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILE_CHOOSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILE_CHOOSER_VIEW_H_

#include <map>
#include <vector>

#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/styled_label_listener.h"

namespace gfx {
class Image;
}

namespace views {
class Link;
class TextButton;
}

class Browser;
class ProfileItemView;

// This bubble view is displayed when the user clicks on the avatar button.
// It displays a list of profiles and allows users to switch between profiles.
class ProfileChooserView : public views::BubbleDelegateView,
                           public views::ButtonListener,
                           public views::LinkListener,
                           public AvatarMenuModelObserver {
 public:
  // Shows the bubble if one is not already showing.  This allows us to easily
  // make a button toggle the bubble on and off when clicked: we unconditionally
  // call this function when the button is clicked and if the bubble isn't
  // showing it will appear while if it is showing, nothing will happen here and
  // the existing bubble will auto-close due to focus loss.
  static void ShowBubble(views::View* anchor_view,
                         views::BubbleBorder::Arrow arrow,
                         views::BubbleBorder::BubbleAlignment border_alignment,
                         const gfx::Rect& anchor_rect,
                         Browser* browser);
  static bool IsShowing();
  static void Hide();

  // We normally close the bubble any time it becomes inactive but this can lead
  // to flaky tests where unexpected UI events are triggering this behavior.
  // Tests should call this with "false" for more consistent operation.
  static void set_close_on_deactivate(bool close) {
    close_on_deactivate_ = close;
  }

 private:
  friend class AvatarMenuButtonTest;
  FRIEND_TEST_ALL_PREFIXES(AvatarMenuButtonTest, NewSignOut);
  FRIEND_TEST_ALL_PREFIXES(AvatarMenuButtonTest, LaunchUserManagerScreen);

  typedef std::vector<size_t> Indexes;
  typedef std::map<views::Button*, int> ButtonIndexes;

  // Different views that can be displayed in the bubble.
  enum BubbleViewMode{
    PROFILE_CHOOSER_VIEW,    // Displays a "fast profile switcher" view.
    ACCOUNT_MANAGEMENT_VIEW  // Displays a list of accounts for the active user.
  };

  ProfileChooserView(views::View* anchor_view,
                     views::BubbleBorder::Arrow arrow,
                     const gfx::Rect& anchor_rect,
                     Browser* browser);
  virtual ~ProfileChooserView();

  // BubbleDelegateView:
  virtual void Init() OVERRIDE;
  virtual void WindowClosing() OVERRIDE;

  // ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // LinkListener:
  virtual void LinkClicked(views::Link* sender, int event_flags) OVERRIDE;

  // AvatarMenuModelObserver:
  virtual void OnAvatarMenuModelChanged(
      AvatarMenuModel* avatar_menu_model) OVERRIDE;

  static ProfileChooserView* profile_bubble_;
  static bool close_on_deactivate_;

  void ResetLinksAndButtons();

  // Shows either the profile chooser or the account management views.
  void ShowView(BubbleViewMode view_to_display,
                AvatarMenuModel* avatar_menu_model);

  // Creates the main profile card for the profile |avatar_item|. |is_guest|
  // is used to determine whether to show any Sign in/Sign out/Manage accounts
  // links.
  views::View* CreateCurrentProfileView(
      const AvatarMenuModel::Item& avatar_item,
      bool is_guest);
  views::View* CreateGuestProfileView();
  views::View* CreateOtherProfilesView(const Indexes& avatars_to_show);
  views::View* CreateOptionsView(bool is_guest_view);

  // Account Management view for the profile |avatar_item|.
  views::View* CreateCurrentProfileEditableView(
      const AvatarMenuModel::Item& avatar_item);
  views::View* CreateCurrentProfileAccountsView(
      const AvatarMenuModel::Item& avatar_item);

  scoped_ptr<AvatarMenuModel> avatar_menu_model_;
  Browser* browser_;

  // Other profiles used in the "fast profile switcher" view.
  ButtonIndexes open_other_profile_indexes_map_;

  // Links displayed in the active profile card.
  views::Link* manage_accounts_link_;
  views::Link* signout_current_profile_link_;
  views::Link* signin_current_profile_link_;
  views::Link* change_photo_link_;

  // Action buttons.
  views::TextButton* guest_button_;
  views::TextButton* end_guest_button_;
  views::TextButton* users_button_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILE_CHOOSER_VIEW_H_
