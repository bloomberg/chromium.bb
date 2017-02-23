// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "content/public/browser/web_contents_delegate.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/controls/textfield/textfield_controller.h"

class EditableProfilePhoto;
class EditableProfileName;

namespace views {
class GridLayout;
class ImageButton;
class Link;
class LabelButton;
}

class Browser;

// This bubble view is displayed when the user clicks on the avatar button.
// It displays a list of profiles and allows users to switch between profiles.
class ProfileChooserView : public content::WebContentsDelegate,
                           public views::BubbleDialogDelegateView,
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
      profiles::TutorialMode tutorial_mode,
      const signin::ManageAccountsParams& manage_accounts_params,
      signin_metrics::AccessPoint access_point,
      views::View* anchor_view,
      Browser* browser,
      bool is_source_keyboard);
  static bool IsShowing();
  static void Hide();

  const Browser* browser() const { return browser_; }

 private:
  friend class ProfileChooserViewExtensionsTest;

  typedef std::vector<size_t> Indexes;
  typedef std::map<views::Button*, int> ButtonIndexes;
  typedef std::map<views::Button*, std::string> AccountButtonIndexes;

  ProfileChooserView(views::View* anchor_view,
                     Browser* browser,
                     profiles::BubbleViewMode view_mode,
                     profiles::TutorialMode tutorial_mode,
                     signin::GAIAServiceType service_type,
                     signin_metrics::AccessPoint access_point);
  ~ProfileChooserView() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  void OnNativeThemeChanged(const ui::NativeTheme* native_theme) override;
  void WindowClosing() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  views::View* GetInitiallyFocusedView() override;
  int GetDialogButtons() const override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(const content::ContextMenuParams& params) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* sender, int event_flags) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // AvatarMenuObserver:
  void OnAvatarMenuChanged(AvatarMenu* avatar_menu) override;

  // OAuth2TokenService::Observer overrides.
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;

  static ProfileChooserView* profile_bubble_;

  // We normally close the bubble any time it becomes inactive but this can lead
  // to flaky tests where unexpected UI events are triggering this behavior.
  // Tests set this to "false" for more consistent operation.
  static bool close_on_deactivate_for_testing_;

  void ResetView();

  // Shows the bubble with the |view_to_display|.
  void ShowView(profiles::BubbleViewMode view_to_display,
                AvatarMenu* avatar_menu);
  void ShowViewFromMode(profiles::BubbleViewMode mode);

  // Focuses the first profile button in the menu list.
  void FocusFirstProfileButton();

  // Creates the profile chooser view.
  views::View* CreateProfileChooserView(AvatarMenu* avatar_menu);

  // Populates |layout| with only a list of the profiles available to
  // switch to.
  void PopulateMinimalProfileChooserView(views::GridLayout* layout,
                                         AvatarMenu* avatar_menu);

  // Populates |layout| with all the elements of the Avatar Menu (current user
  // bubble, options buttons, tutorials).
  void PopulateCompleteProfileChooserView(views::GridLayout* layout,
                                          AvatarMenu* avatar_menu);

  // Creates the main profile card for the profile |avatar_item|. |is_guest|
  // is used to determine whether to show any Sign in/Sign out/Manage accounts
  // links.
  views::View* CreateCurrentProfileView(
      const AvatarMenu::Item& avatar_item,
      bool is_guest);
  views::View* CreateMaterialDesignCurrentProfileView(
      const AvatarMenu::Item& avatar_item,
      bool is_guest);
  views::View* CreateGuestProfileView();
  views::View* CreateOtherProfilesView(const Indexes& avatars_to_show);
  views::View* CreateOptionsView(bool display_lock, AvatarMenu* avatar_menu);
  views::View* CreateSupervisedUserDisclaimerView();

  // Account Management view for the profile |avatar_item|.
  views::View* CreateCurrentProfileAccountsView(
      const AvatarMenu::Item& avatar_item);
  void CreateAccountButton(views::GridLayout* layout,
                           const std::string& account_id,
                           bool is_primary_account,
                           bool reauth_required,
                           int width);

  // Creates a webview showing the gaia signin page.
  views::View* CreateGaiaSigninView(views::View** signin_content_view);

  // Creates a view to confirm account removal for |account_id_to_remove_|.
  views::View* CreateAccountRemovalView();

  // Removes the currently selected account and attempts to restart Chrome.
  void RemoveAccount();

  // Close the tutorial card.
  void DismissTutorial();

  // Creates a tutorial card to introduce an upgrade user to the new avatar
  // menu. |avatar_item| refers to the current profile.
  views::View* CreateWelcomeUpgradeTutorialView(
      const AvatarMenu::Item& avatar_item);

  // Creates a tutorial card to have the user confirm the last Chrome signin,
  // Chrome sync will be delayed until the user either dismisses the tutorial,
  // or configures sync through the "Settings" link.
  views::View* CreateSigninConfirmationView();

  // Creates a tutorial card to show the errors in the last Chrome signin.
  views::View* CreateSigninErrorView();

  // Creates a tutorial card telling the user about right-click user switching.
  views::View* CreateRightClickTutorialView();

  views::View* CreateTutorialViewIfNeeded(const AvatarMenu::Item& item);

  // Creates a tutorial card. If |stack_button| is true, places the button above
  // the link otherwise places both on the same row with the link left aligned
  // and button right aligned. The method sets |link| to point to the newly
  // create link, |button| to the newly created button, and |tutorial_mode_| to
  // the given |tutorial_mode|.
  views::View*  CreateTutorialView(
      profiles::TutorialMode tutorial_mode,
      const base::string16& title_text,
      const base::string16& content_text,
      const base::string16& link_text,
      const base::string16& button_text,
      bool stack_button,
      views::Link** link,
      views::LabelButton** button,
      views::ImageButton** close_button);

  // Creates a header for signin and sync error surfacing for the user menu.
  views::View* CreateSyncErrorViewIfNeeded();

  // Create a view that shows various options for an upgrade user who is not
  // the same person as the currently signed in user.
  views::View* CreateSwitchUserView();

  bool ShouldShowGoIncognito() const;

  // Clean-up done after an action was performed in the ProfileChooser.
  void PostActionPerformed(ProfileMetrics::ProfileDesktopMenu action_performed);

  std::unique_ptr<AvatarMenu> avatar_menu_;
  Browser* browser_;

  // Other profiles used in the "fast profile switcher" view.
  ButtonIndexes open_other_profile_indexes_map_;

  // Buttons associated with the current profile.
  AccountButtonIndexes delete_account_button_map_;
  AccountButtonIndexes reauth_account_button_map_;

  // Links and buttons displayed in the tutorial card.
  views::LabelButton* tutorial_sync_settings_ok_button_;
  views::Link* tutorial_sync_settings_link_;
  views::LabelButton* tutorial_see_whats_new_button_;
  views::Link* tutorial_not_you_link_;
  views::Link* tutorial_learn_more_link_;
  views::ImageButton* tutorial_close_button_;

  // Buttons in the signin/sync error header on top of the desktop user menu.
  views::LabelButton* sync_error_signin_button_;
  views::LabelButton* sync_error_passphrase_button_;
  views::LabelButton* sync_error_upgrade_button_;
  views::LabelButton* sync_error_signin_again_button_;
  views::LabelButton* sync_error_signout_button_;

  // Links and buttons displayed in the active profile card.
  views::Link* manage_accounts_link_;
  views::LabelButton* manage_accounts_button_;
  views::LabelButton* signin_current_profile_button_;
  views::LabelButton* auth_error_email_button_;

  // The profile name and photo in the active profile card in non-material-
  // design user menu. Owned by the views hierarchy.
  EditableProfilePhoto* current_profile_photo_;
  EditableProfileName* current_profile_name_;
  // For material design user menu, the active profile card owns the profile
  // name and photo.
  views::LabelButton* current_profile_card_;

  // Action buttons.
  views::LabelButton* first_profile_button_;
  views::LabelButton* guest_profile_button_;
  views::LabelButton* users_button_;
  views::LabelButton* go_incognito_button_;
  views::LabelButton* lock_button_;
  views::LabelButton* close_all_windows_button_;
  views::Link* add_account_link_;

  // Buttons displayed in the gaia signin view.
  views::ImageButton* gaia_signin_cancel_button_;

  // Links and buttons displayed in the account removal view.
  views::LabelButton* remove_account_button_;
  views::ImageButton* account_removal_cancel_button_;

  // Buttons in the switch user view.
  views::LabelButton* add_person_button_;
  views::LabelButton* disconnect_button_;
  views::ImageButton* switch_user_cancel_button_;

  // Records the account id to remove.
  std::string account_id_to_remove_;

  // Active view mode.
  profiles::BubbleViewMode view_mode_;

  // The current tutorial mode.
  profiles::TutorialMode tutorial_mode_;

  // The GAIA service type provided in the response header.
  signin::GAIAServiceType gaia_service_type_;

  // The current access point of sign in.
  const signin_metrics::AccessPoint access_point_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CHOOSER_VIEW_H_
