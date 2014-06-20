// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_

#include <map>
#include <vector>

#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/controls/textfield/textfield_controller.h"

class EditableProfilePhoto;
class EditableProfileName;

namespace gfx {
class Image;
}

namespace views {
class GridLayout;
class ImageButton;
class Link;
class LabelButton;
}

class Browser;

// This bubble view is displayed when the user clicks on the avatar button.
// It displays a list of profiles and allows users to switch between profiles.
class ProfileChooserView : public views::BubbleDelegateView,
                           public views::ButtonListener,
                           public views::LinkListener,
                           public views::StyledLabelListener,
                           public views::TextfieldController,
                           public AvatarMenuObserver,
                           public OAuth2TokenService::Observer {
 public:
  // Shows the bubble if one is not already showing.  This allows us to easily
  // make a button toggle the bubble on and off when clicked: we unconditionally
  // call this function when the button is clicked and if the bubble isn't
  // showing it will appear while if it is showing, nothing will happen here and
  // the existing bubble will auto-close due to focus loss.
  static void ShowBubble(
      profiles::BubbleViewMode view_mode,
      const signin::ManageAccountsParams& manage_accounts_params,
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow,
      views::BubbleBorder::BubbleAlignment border_alignment,
      Browser* browser);
  static bool IsShowing();
  static void Hide();

  // We normally close the bubble any time it becomes inactive but this can lead
  // to flaky tests where unexpected UI events are triggering this behavior.
  // Tests should call this with "false" for more consistent operation.
  static void clear_close_on_deactivate_for_testing() {
    close_on_deactivate_for_testing_ = false;
  }

 private:
  friend class NewAvatarMenuButtonTest;
  FRIEND_TEST_ALL_PREFIXES(NewAvatarMenuButtonTest, SignOut);

  typedef std::vector<size_t> Indexes;
  typedef std::map<views::Button*, int> ButtonIndexes;
  typedef std::map<views::Button*, std::string> AccountButtonIndexes;

  ProfileChooserView(views::View* anchor_view,
                     views::BubbleBorder::Arrow arrow,
                     Browser* browser,
                     profiles::BubbleViewMode view_mode,
                     signin::GAIAServiceType service_type);
  virtual ~ProfileChooserView();

  // views::BubbleDelegateView:
  virtual void Init() OVERRIDE;
  virtual void WindowClosing() OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* sender, int event_flags) OVERRIDE;

  // views::StyledLabelListener implementation.
  virtual void StyledLabelLinkClicked(
      const gfx::Range& range, int event_flags) OVERRIDE;

  // views::TextfieldController:
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE;

  // AvatarMenuObserver:
  virtual void OnAvatarMenuChanged(AvatarMenu* avatar_menu) OVERRIDE;

  // OAuth2TokenService::Observer overrides.
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE;

  static ProfileChooserView* profile_bubble_;
  static bool close_on_deactivate_for_testing_;

  void ResetView();

  // Shows the bubble with the |view_to_display|.
  void ShowView(profiles::BubbleViewMode view_to_display,
                AvatarMenu* avatar_menu);

  // Creates the profile chooser view. |tutorial_shown| indicates if the "mirror
  // enabled" tutorial was shown or not in the last active view.
  views::View* CreateProfileChooserView(AvatarMenu* avatar_menu,
      profiles::TutorialMode last_tutorial_mode);

  // Creates the main profile card for the profile |avatar_item|. |is_guest|
  // is used to determine whether to show any Sign in/Sign out/Manage accounts
  // links.
  views::View* CreateCurrentProfileView(
      const AvatarMenu::Item& avatar_item,
      bool is_guest);
  views::View* CreateGuestProfileView();
  views::View* CreateOtherProfilesView(const Indexes& avatars_to_show);
  views::View* CreateOptionsView(bool enable_lock);
  views::View* CreateSupervisedUserDisclaimerView();

  // Account Management view for the profile |avatar_item|.
  views::View* CreateCurrentProfileAccountsView(
      const AvatarMenu::Item& avatar_item);
  void CreateAccountButton(views::GridLayout* layout,
                           const std::string& account,
                           bool is_primary_account,
                           bool reauth_required,
                           int width);

  // Creates a webview showing the gaia signin page.
  views::View* CreateGaiaSigninView();

  // Creates a view to confirm account removal for |account_id_to_remove_|.
  views::View* CreateAccountRemovalView();

  // Removes the currently selected account and attempts to restart Chrome.
  void RemoveAccount();

  // Creates a a tutorial card at the top prompting the user to try out the new
  // profile management UI.
  views::View* CreateNewProfileManagementPreviewView();

  // Creates a tutorial card shown when new profile management preview is
  // enabled. |current_avatar_item| indicates the current profile.
  // |tutorial_shown| indicates if the tutorial card is already shown in the
  // last active view.
  views::View* CreatePreviewEnabledTutorialView(
      const AvatarMenu::Item& current_avatar_item, bool tutorial_shown);

  // Creates a a tutorial card at the top prompting the user to send feedback
  // about the new profile management preview and/or to end preview.
  views::View* CreateSendPreviewFeedbackView();

  // Creates a tutorial card with the specified |title_text|, |context_text|,
  // and a bottom row with a right-aligned link using the specified |link_text|,
  // and a left aligned button using the specified |button_text|. The method
  // sets |link| to point to the newly created link, |button| to the newly
  // created button, and |tutorial_mode_| to the given |tutorial_mode|.
  views::View* CreateTutorialView(
      profiles::TutorialMode tutorial_mode,
      const base::string16& title_text,
      const base::string16& content_text,
      const base::string16& link_text,
      const base::string16& button_text,
      views::Link** link,
      views::LabelButton** button);

  views::View* CreateEndPreviewView();

  // Clean-up done after an action was performed in the ProfileChooser.
  void PostActionPerformed(ProfileMetrics::ProfileDesktopMenu action_performed);

  scoped_ptr<AvatarMenu> avatar_menu_;
  Browser* browser_;

  // Other profiles used in the "fast profile switcher" view.
  ButtonIndexes open_other_profile_indexes_map_;

  // Buttons associated with the current profile.
  AccountButtonIndexes delete_account_button_map_;
  AccountButtonIndexes reauth_account_button_map_;

  // Links and buttons displayed in the tutorial card.
  views::Link* tutorial_learn_more_link_;
  views::LabelButton* tutorial_ok_button_;
  views::LabelButton* tutorial_enable_new_profile_management_button_;
  views::Link* tutorial_end_preview_link_;
  views::LabelButton* tutorial_send_feedback_button_;

  // Links and buttons displayed in the active profile card.
  views::Link* manage_accounts_link_;
  views::LabelButton* signin_current_profile_link_;
  views::ImageButton* question_mark_button_;

  // The profile name and photo in the active profile card. Owned by the
  // views hierarchy.
  EditableProfilePhoto* current_profile_photo_;
  EditableProfileName* current_profile_name_;

  // Action buttons.
  views::LabelButton* users_button_;
  views::LabelButton* lock_button_;
  views::Link* add_account_link_;

  // Buttons displayed in the gaia signin view.
  views::ImageButton* gaia_signin_cancel_button_;

  // Links and buttons displayed in the account removal view.
  views::LabelButton* remove_account_button_;
  views::ImageButton* account_removal_cancel_button_;

  // Links and buttons displayed in the end-preview view.
  views::LabelButton* end_preview_and_relaunch_button_;
  views::ImageButton* end_preview_cancel_button_;

  // Records the account id to remove.
  std::string account_id_to_remove_;

  // Active view mode.
  profiles::BubbleViewMode view_mode_;

  // The current tutorial mode.
  profiles::TutorialMode tutorial_mode_;

  // The GAIA service type provided in the response header.
  signin::GAIAServiceType gaia_service_type_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_
