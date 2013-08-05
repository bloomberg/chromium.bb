// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILE_CHOOSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILE_CHOOSER_VIEW_H_

#include <map>
#include <vector>

#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
class Image;
}

namespace views {
class LabelButton;
}

class AvatarMenuModel;
class Browser;
class ProfileItemView;

// This bubble view is displayed when the user clicks on the avatar button.
// It displays a list of profiles and allows users to switch between profiles.
class ProfileChooserView : public views::BubbleDelegateView,
                           public views::ButtonListener,
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
  typedef std::map<views::View*, int> ViewIndexes;

  ProfileChooserView(views::View* anchor_view,
                     views::BubbleBorder::Arrow arrow,
                     const gfx::Rect& anchor_rect,
                     Browser* browser);
  virtual ~ProfileChooserView();

  // BubbleDelegateView:
  virtual void Init() OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;

  // ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // AvatarMenuModelObserver:
  virtual void OnAvatarMenuModelChanged(
      AvatarMenuModel* avatar_menu_model) OVERRIDE;

  static ProfileChooserView* profile_bubble_;
  static bool close_on_deactivate_;

  views::View* CreateProfileImageView(const gfx::Image& icon, int side);
  views::View* CreateProfileCardView(size_t avatar_to_show);
  views::View* CreateCurrentProfileView(size_t avatars_to_show);
  views::View* CreateOtherProfilesView(const Indexes& avatars_to_show);
  views::View* CreateOptionsView();

  scoped_ptr<AvatarMenuModel> avatar_menu_model_;
  Browser* browser_;
  views::View* current_profile_view_;
  views::LabelButton* guest_button_view_;
  views::LabelButton* users_button_view_;
  ViewIndexes open_other_profile_indexes_map_;
  views::View* other_profiles_view_;
  views::View* signout_current_profile_view_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILE_CHOOSER_VIEW_H_
