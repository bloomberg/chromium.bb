// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/browser/ui/views/profiles/signin_view_controller_delegate_views.h"
#include "chrome/browser/ui/views/profiles/user_manager_view.h"
#include "chrome/browser/ui/views/sync/dice_signin_button_view.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_widget_host_view.h"
#include "services/identity/public/cpp/accounts_mutator.h"
#include "services/identity/public/cpp/primary_account_mutator.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/clip_recorder.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// Helpers --------------------------------------------------------------------

constexpr int kFixedMenuWidthPreDice = 240;
constexpr int kFixedMenuWidthDice = 288;
constexpr int kIconSize = 16;

// Spacing between the edge of the user menu and the top/bottom or left/right of
// the menu items.
constexpr int kMenuEdgeMargin = 16;

// Number of times the Dice sign-in promo illustration should be shown.
constexpr int kDiceSigninPromoIllustrationShowCountMax = 10;

bool IsProfileChooser(profiles::BubbleViewMode mode) {
  return mode == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
}

BadgedProfilePhoto::BadgeType GetProfileBadgeType(Profile* profile) {
  if (profile->IsSupervised()) {
    return profile->IsChild() ? BadgedProfilePhoto::BADGE_TYPE_CHILD
                              : BadgedProfilePhoto::BADGE_TYPE_SUPERVISOR;
  }
  // |Profile::IsSyncAllowed| is needed to check whether sync is allowed by GPO
  // policy.
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile) &&
      profile->IsSyncAllowed() &&
      IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount()) {
    return BadgedProfilePhoto::BADGE_TYPE_SYNC_COMPLETE;
  }
  return BadgedProfilePhoto::BADGE_TYPE_NONE;
}

gfx::ImageSkia CreateVectorIcon(const gfx::VectorIcon& icon) {
  return gfx::CreateVectorIcon(
      icon, kIconSize,
      ui::NativeTheme::GetInstanceForNativeUi()->SystemDarkModeEnabled()
          ? gfx::kGoogleGrey500
          : gfx::kChromeIconGrey);
}

}  // namespace

// ProfileChooserView ---------------------------------------------------------

// static
ProfileChooserView* ProfileChooserView::profile_bubble_ = nullptr;
bool ProfileChooserView::close_on_deactivate_for_testing_ = true;

// static
void ProfileChooserView::ShowBubble(
    profiles::BubbleViewMode view_mode,
    const signin::ManageAccountsParams& manage_accounts_params,
    signin_metrics::AccessPoint access_point,
    views::Button* anchor_button,
    gfx::NativeView parent_window,
    const gfx::Rect& anchor_rect,
    Browser* browser,
    bool is_source_keyboard) {
  if (IsShowing())
    return;

  const bool is_incognito_profile =
      browser->profile()->GetProfileType() == Profile::INCOGNITO_PROFILE;
  DCHECK_EQ(is_incognito_profile,
            view_mode == profiles::BUBBLE_VIEW_MODE_INCOGNITO);

  profile_bubble_ = new ProfileChooserView(
      anchor_button, anchor_rect, parent_window, browser, view_mode,
      manage_accounts_params.service_type, access_point, is_source_keyboard);
  base::RecordAction(base::UserMetricsAction("ProfileChooser_Show"));
}

// static
bool ProfileChooserView::IsShowing() {
  return profile_bubble_ != nullptr;
}

// static
void ProfileChooserView::Hide() {
  if (IsShowing())
    profile_bubble_->GetWidget()->Close();
}

ProfileChooserView::ProfileChooserView(views::Button* anchor_button,
                                       const gfx::Rect& anchor_rect,
                                       gfx::NativeView parent_window,
                                       Browser* browser,
                                       profiles::BubbleViewMode view_mode,
                                       signin::GAIAServiceType service_type,
                                       signin_metrics::AccessPoint access_point,
                                       bool is_source_keyboard)
    : ProfileMenuViewBase(anchor_button, anchor_rect, parent_window, browser),
      view_mode_(view_mode),
      gaia_service_type_(service_type),
      access_point_(access_point),
      dice_enabled_(AccountConsistencyModeManager::IsDiceEnabledForProfile(
          browser->profile())) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PROFILE_CHOOSER);
  set_menu_width(dice_enabled_ ? kFixedMenuWidthDice : kFixedMenuWidthPreDice);
  ResetView();
  ShowMenu();
  if (is_source_keyboard)
    FocusFirstProfileButton();
}

ProfileChooserView::~ProfileChooserView() = default;

void ProfileChooserView::ResetView() {
  open_other_profile_indexes_map_.clear();
  sync_error_button_ = nullptr;
  signin_current_profile_button_ = nullptr;
  signin_with_gaia_account_button_ = nullptr;
  current_profile_card_ = nullptr;
  first_profile_button_ = nullptr;
  guest_profile_button_ = nullptr;
  users_button_ = nullptr;
  go_incognito_button_ = nullptr;
  lock_button_ = nullptr;
  close_all_windows_button_ = nullptr;
  add_account_link_ = nullptr;
  sync_to_another_account_button_ = nullptr;
  dice_signin_button_view_ = nullptr;
  passwords_button_ = nullptr;
  credit_cards_button_ = nullptr;
  addresses_button_ = nullptr;
  signout_button_ = nullptr;
}

void ProfileChooserView::Init() {
  set_close_on_deactivate(close_on_deactivate_for_testing_);

  avatar_menu_.reset(new AvatarMenu(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage(),
      this, browser()));
  avatar_menu_->RebuildMenu();

  Profile* profile = browser()->profile();
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (identity_manager)
    identity_manager->AddObserver(this);

  ShowViewFromMode(view_mode_);
}

void ProfileChooserView::OnAvatarMenuChanged(
    AvatarMenu* avatar_menu) {
  if (IsProfileChooser(view_mode_)) {
    // Refresh the view with the new menu. We can't just update the local copy
    // as this may have been triggered by a sign out action, in which case
    // the view is being destroyed.
    ShowView(view_mode_, avatar_menu);
  }
}

void ProfileChooserView::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
      view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH) {
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER);
  }
}

void ProfileChooserView::ShowView(profiles::BubbleViewMode view_to_display,
                                  AvatarMenu* avatar_menu) {
  if (browser()->profile()->IsSupervised() &&
      view_to_display == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT) {
    LOG(WARNING) << "Supervised user attempted to add account";
    return;
  }

  if (browser()->profile()->IsOffTheRecord() &&
      !browser()->profile()->IsGuestSession() &&
      view_to_display != profiles::BUBBLE_VIEW_MODE_INCOGNITO) {
    LOG(WARNING) << "Expected incognito menu in incognito mode.";
    return;
  }

  ResetView();
  view_mode_ = view_to_display;
  int width_override = -1;

  views::View* sub_view = nullptr;
  switch (view_mode_) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH:
      // The modal sign-in view is shown in for bubble view modes.
      // See |SigninViewController::ShouldShowSigninForMode|.
      NOTREACHED();
      break;
    case profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER:
      sub_view = CreateProfileChooserView(avatar_menu);
      break;
    case profiles::BUBBLE_VIEW_MODE_INCOGNITO:
      sub_view = CreateIncognitoWindowCountView();
      break;
  }

  SetContentsView(sub_view, width_override);
}

void ProfileChooserView::ShowViewFromMode(profiles::BubbleViewMode mode) {
  if (SigninViewController::ShouldShowSigninForMode(mode)) {
    // Hides the user menu if it is currently shown. The user menu automatically
    // closes when it loses focus; however, on Windows, the signin modals do not
    // take away focus, thus we need to manually close the bubble.
    Hide();
    browser()->signin_view_controller()->ShowSignin(mode, browser(),
                                                    access_point_);
  } else {
    ShowView(mode, avatar_menu_.get());
  }
}

void ProfileChooserView::FocusFirstProfileButton() {
  if (first_profile_button_)
    first_profile_button_->RequestFocus();
}

void ProfileChooserView::WindowClosing() {
  DCHECK_EQ(profile_bubble_, this);
  if (anchor_button())
    anchor_button()->AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  profile_bubble_ = nullptr;
}

void ProfileChooserView::OnWidgetClosing(views::Widget* /*widget*/) {
  // Unsubscribe from everything early so that the updates do not reach the
  // bubble and change its state.
  avatar_menu_.reset();
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  if (identity_manager)
    identity_manager->RemoveObserver(this);
}

views::View* ProfileChooserView::GetInitiallyFocusedView() {
#if defined(OS_MACOSX)
  // On Mac, buttons are not focusable when full keyboard access is turned off,
  // causing views::Widget to fall back to focusing the first focusable View.
  // This behavior is not desired in the |ProfileChooserView| because of its
  // menu-like design using |HoverButtons|. Avoid this by returning null when
  // full keyboard access is off.
  if (!GetFocusManager() || !GetFocusManager()->keyboard_accessible())
    return nullptr;
#endif
  return signin_current_profile_button_;
}

base::string16 ProfileChooserView::GetAccessibleWindowTitle() const {
  return l10n_util::GetStringUTF16(
      view_mode_ == profiles::BUBBLE_VIEW_MODE_INCOGNITO
          ? IDS_INCOGNITO_BUBBLE_ACCESSIBLE_TITLE
          : IDS_PROFILES_PROFILE_BUBBLE_ACCESSIBLE_TITLE);
}

void ProfileChooserView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == passwords_button_) {
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_PasswordsClicked"));
    NavigateToManagePasswordsPage(
        browser(), password_manager::ManagePasswordsReferrer::kProfileChooser);
  } else if (sender == credit_cards_button_) {
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_PaymentsClicked"));
    chrome::ShowSettingsSubPage(browser(), chrome::kPaymentsSubPage);
  } else if (sender == addresses_button_) {
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_AddressesClicked"));
    chrome::ShowSettingsSubPage(browser(), chrome::kAddressesSubPage);
  } else if (sender == guest_profile_button_) {
    PrefService* service = g_browser_process->local_state();
    DCHECK(service);
    DCHECK(service->GetBoolean(prefs::kBrowserGuestModeEnabled));
    profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
    base::RecordAction(base::UserMetricsAction("ProfileChooser_GuestClicked"));
  } else if (sender == users_button_) {
    // If this is a guest session, close all the guest browser windows.
    if (browser()->profile()->IsGuestSession()) {
      profiles::CloseGuestProfileWindows();
    } else if (browser()->profile()->GetProfileType() ==
               Profile::INCOGNITO_PROFILE) {
      // Skipping before-unload trigger to give incognito mode users a chance to
      // close all incognito windows without needing to confirm closing the open
      // forms.
      BrowserList::CloseAllBrowsersWithIncognitoProfile(
          browser()->profile(), base::DoNothing(), base::DoNothing(),
          true /* skip_beforeunload */);
    } else {
      base::RecordAction(
          base::UserMetricsAction("ProfileChooser_ManageClicked"));
      UserManager::Show(base::FilePath(),
                        profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
    }
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_OPEN_USER_MANAGER);
  } else if (sender == go_incognito_button_) {
    DCHECK(ShouldShowGoIncognito());
    chrome::NewIncognitoWindow(browser()->profile());
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_GO_INCOGNITO);
  } else if (sender == lock_button_) {
    profiles::LockProfile(browser()->profile());
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_LOCK);
  } else if (sender == close_all_windows_button_) {
    profiles::CloseProfileWindows(browser()->profile());
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_CloseAllClicked"));
  } else if (sender == sync_error_button_) {
    sync_ui_util::AvatarSyncErrorType error =
        static_cast<sync_ui_util::AvatarSyncErrorType>(sender->id());
    switch (error) {
      case sync_ui_util::MANAGED_USER_UNRECOVERABLE_ERROR:
        chrome::ShowSettingsSubPage(browser(), chrome::kSignOutSubPage);
        break;
      case sync_ui_util::UNRECOVERABLE_ERROR:
        if (ProfileSyncServiceFactory::GetForProfile(browser()->profile())) {
          syncer::RecordSyncEvent(syncer::STOP_FROM_OPTIONS);
        }

        // GetPrimaryAccountMutator() might return nullptr on some platforms.
        if (auto* account_mutator =
                IdentityManagerFactory::GetForProfile(browser()->profile())
                    ->GetPrimaryAccountMutator()) {
          account_mutator->ClearPrimaryAccount(
              identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
              signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
              signin_metrics::SignoutDelete::IGNORE_METRIC);
          ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN);
        }
        break;
      case sync_ui_util::AUTH_ERROR:
        ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH);
        break;
      case sync_ui_util::UPGRADE_CLIENT_ERROR:
        chrome::OpenUpdateChromeDialog(browser());
        break;
      case sync_ui_util::PASSPHRASE_ERROR:
      case sync_ui_util::SETTINGS_UNCONFIRMED_ERROR:
        chrome::ShowSettingsSubPage(browser(), chrome::kSyncSetupSubPage);
        break;
      case sync_ui_util::NO_SYNC_ERROR:
        NOTREACHED();
        break;
    }
    base::RecordAction(
        base::UserMetricsAction("ProfileChooser_SignInAgainClicked"));
  } else if (sender == current_profile_card_) {
    if (dice_enabled_ &&
        IdentityManagerFactory::GetForProfile(browser()->profile())
            ->HasPrimaryAccount()) {
      chrome::ShowSettingsSubPage(browser(), chrome::kPeopleSubPage);
    } else {
      // Open settings to edit profile name and image. The profile doesn't need
      // to be authenticated to open this.
      avatar_menu_->EditProfile(avatar_menu_->GetActiveProfileIndex());
      PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_IMAGE);
      PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_NAME);
    }
  } else if (sender == signin_current_profile_button_) {
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN);
  } else if (sender == signin_with_gaia_account_button_) {
    DCHECK(dice_signin_button_view_->account());
    Hide();
    signin_ui_util::EnableSyncFromPromo(
        browser(), dice_signin_button_view_->account().value(), access_point_,
        true /* is_default_promo_account */);
  } else if (sender == sync_to_another_account_button_) {
    // Extract the promo accounts for the submenu, i.e. remove the first
    // one from the list because it is already shown in a separate button.
    std::vector<AccountInfo> accounts(dice_sync_promo_accounts_.begin() + 1,
                                      dice_sync_promo_accounts_.end());
    // Display the submenu to list |accounts|.
    // Using base::Unretained(this) is safe here because |dice_accounts_menu_|
    // is owned by |ProfileChooserView|, i.e. |this|.
    dice_accounts_menu_ = std::make_unique<DiceAccountsMenu>(
        accounts, base::BindOnce(&ProfileChooserView::EnableSync,
                                 base::Unretained(this)));
    // Add sign-out button.
    dice_accounts_menu_->SetSignOutButtonCallback(base::BindOnce(
        &ProfileChooserView::SignOutAllWebAccounts, base::Unretained(this)));
    dice_accounts_menu_->Show(sender, sync_to_another_account_button_);
  } else if (sender == signout_button_) {
    SignOutAllWebAccounts();
    base::RecordAction(base::UserMetricsAction("Signin_Signout_FromUserMenu"));
  } else {
    // Either one of the "other profiles", or one of the profile accounts
    // buttons was pressed.
    ButtonIndexes::const_iterator profile_match =
        open_other_profile_indexes_map_.find(sender);
    if (profile_match != open_other_profile_indexes_map_.end()) {
      avatar_menu_->SwitchToProfile(
          profile_match->second, ui::DispositionFromEventFlags(event.flags()) ==
                                     WindowOpenDisposition::NEW_WINDOW,
          ProfileMetrics::SWITCH_PROFILE_ICON);
      base::RecordAction(
          base::UserMetricsAction("ProfileChooser_ProfileClicked"));
      Hide();
    } else {
      NOTREACHED();
    }
  }
}

void ProfileChooserView::LinkClicked(views::Link* sender, int /*event_flags*/) {
  if (sender == add_account_link_) {
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT);
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_ADD_ACCT);
  }
}

views::View* ProfileChooserView::CreateIncognitoWindowCountView() {
  // TODO(https://crbug.com/896235): Refactor to merge this view with other
  // views.
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view, menu_width());

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  int content_list_vert_spacing =
      provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_SINGLE);

  int incognito_window_count =
      BrowserList::GetIncognitoSessionsActiveForProfile(browser()->profile());
  auto incognito_icon = std::make_unique<views::ImageView>();
  // The icon color is set to match the menu text, which guarantees sufficient
  // contrast and a consistent visual appearance..
  const SkColor icon_color = provider->GetTypographyProvider().GetColor(
      *this, views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
  incognito_icon->SetImage(
      gfx::CreateVectorIcon(kIncognitoProfileIcon, icon_color));

  // TODO(https://crbug.com/915120): This Button is never clickable. Replace
  // by an alternative list item.
  HoverButton* profile_card = new HoverButton(
      nullptr, std::move(incognito_icon),
      l10n_util::GetStringUTF16(IDS_INCOGNITO_PROFILE_MENU_TITLE),
      incognito_window_count > 1
          ? l10n_util::GetPluralStringFUTF16(IDS_INCOGNITO_WINDOW_COUNT_MESSAGE,
                                             incognito_window_count)
          : base::string16());
  profile_card->SetEnabled(false);

  layout->StartRowWithPadding(1.0, 0, views::GridLayout::kFixedSize,
                              content_list_vert_spacing);
  layout->AddView(profile_card);

  layout->StartRowWithPadding(1.0, 0, views::GridLayout::kFixedSize,
                              content_list_vert_spacing);
  layout->AddView(new views::Separator());

  layout->AddPaddingRow(1.0, content_list_vert_spacing);
  users_button_ = new HoverButton(
      this, gfx::CreateVectorIcon(kCloseAllIcon, 16, gfx::kChromeIconGrey),
      l10n_util::GetStringUTF16(IDS_INCOGNITO_PROFILE_MENU_CLOSE_BUTTON));

  layout->StartRowWithPadding(1.0, 0, views::GridLayout::kFixedSize,
                              content_list_vert_spacing);
  layout->AddView(users_button_);

  const int small_vertical_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  layout->AddPaddingRow(1.0, small_vertical_spacing);

  return view;
}

views::View* ProfileChooserView::CreateProfileChooserView(
    AvatarMenu* avatar_menu) {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view, menu_width());
  // Separate items into active and alternatives.
  Indexes other_profiles;
  views::View* sync_error_view = nullptr;
  views::View* current_profile_view = nullptr;
  views::View* current_profile_accounts = nullptr;
  views::View* option_buttons_view = nullptr;
  views::View* autofill_home_view = nullptr;
  bool current_profile_signed_in = false;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(i);
    if (item.active) {
      option_buttons_view = CreateOptionsView(
          item.signed_in && profiles::IsLockAvailable(browser()->profile()),
          avatar_menu);
      current_profile_view = CreateCurrentProfileView(item, false);
      autofill_home_view = CreateAutofillHomeView();
      current_profile_signed_in = item.signed_in;
      sync_error_view = CreateSyncErrorViewIfNeeded(item);
    } else {
      other_profiles.push_back(i);
    }
  }

  if (sync_error_view) {
    layout->StartRow(1.0, 0);
    layout->AddView(sync_error_view);
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    layout->AddView(new views::Separator());
  }

  if (!current_profile_view) {
    // Guest windows don't have an active profile.
    current_profile_view = CreateGuestProfileView();
    option_buttons_view = CreateOptionsView(false, avatar_menu);
  }

  if (!(dice_enabled_ && sync_error_view)) {
    layout->StartRow(1.0, 0);
    layout->AddView(current_profile_view);
  }

  if (!IsProfileChooser(view_mode_)) {
    DCHECK(current_profile_accounts);
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    layout->AddView(new views::Separator());
    layout->StartRow(1.0, 0);
    layout->AddView(current_profile_accounts);
  }

  if (browser()->profile()->IsSupervised()) {
    layout->StartRow(1.0, 0);
    layout->AddView(CreateSupervisedUserDisclaimerView());
  }

  if (autofill_home_view) {
    const int content_list_vert_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
    if (!current_profile_signed_in) {
      // If the user is signed in then the autofill data is a part of the
      // account logically. Otherwise, use a separator.
      layout->StartRow(0, 0);
      layout->AddView(new views::Separator());
      layout->AddPaddingRow(1.0, content_list_vert_spacing);
    }
    layout->StartRow(0, 0);
    layout->AddView(autofill_home_view);
    layout->AddPaddingRow(1.0, content_list_vert_spacing);
  }

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(new views::Separator());

  if (option_buttons_view) {
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    layout->AddView(option_buttons_view);
  }
  return view;
}

views::View* ProfileChooserView::CreateSyncErrorViewIfNeeded(
    const AvatarMenu::Item& avatar_item) {
  int content_string_id, button_string_id;
  sync_ui_util::AvatarSyncErrorType error =
      sync_ui_util::GetMessagesForAvatarSyncError(
          browser()->profile(), &content_string_id, &button_string_id);
  if (error == sync_ui_util::NO_SYNC_ERROR)
    return nullptr;

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  if (dice_enabled_)
    return CreateDiceSyncErrorView(avatar_item, error, button_string_id);

  // Sets an overall horizontal layout.
  views::View* view = new views::View();
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(kMenuEdgeMargin),
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  view->SetLayoutManager(std::move(layout));

  // Adds the sync problem icon.
  views::ImageView* sync_problem_icon = new views::ImageView();
  sync_problem_icon->SetImage(
      gfx::CreateVectorIcon(kSyncProblemIcon, kIconSize, gfx::kGoogleRed700));
  view->AddChildView(sync_problem_icon);

  // Adds a vertical view to organize the error title, message, and button.
  views::View* vertical_view = new views::View();
  const int small_vertical_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  auto vertical_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(), small_vertical_spacing);
  vertical_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  vertical_view->SetLayoutManager(std::move(vertical_layout));

  // Adds the title.
  views::Label* title_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_SYNC_ERROR_USER_MENU_TITLE));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetEnabledColor(gfx::kGoogleRed700);
  vertical_view->AddChildView(title_label);

  // Adds body content.
  views::Label* content_label =
      new views::Label(l10n_util::GetStringUTF16(content_string_id));
  content_label->SetMultiLine(true);
  content_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  vertical_view->AddChildView(content_label);

  // Adds an action button if an action exists.
  if (button_string_id) {
    // Adds a padding row between error title/content and the button.
    auto* padding = new views::View;
    padding->SetPreferredSize(gfx::Size(
        0,
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
    vertical_view->AddChildView(padding);

    sync_error_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
        this, l10n_util::GetStringUTF16(button_string_id));
    // Track the error type so that the correct action can be taken in
    // ButtonPressed().
    sync_error_button_->set_id(error);
    vertical_view->AddChildView(sync_error_button_);
    view->SetBorder(views::CreateEmptyBorder(0, 0, small_vertical_spacing, 0));
  }

  view->AddChildView(vertical_view);
  return view;
}

views::View* ProfileChooserView::CreateDiceSyncErrorView(
    const AvatarMenu::Item& avatar_item,
    sync_ui_util::AvatarSyncErrorType error,
    int button_string_id) {
  // Creates a view containing an error hover button displaying the current
  // profile (only selectable when sync is paused or disabled) and when sync is
  // not disabled there is a blue button to resolve the error.
  views::View* view = new views::View();
  const int current_profile_vertical_margin =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING);
  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      gfx::Insets(current_profile_vertical_margin, 0),
      current_profile_vertical_margin));

  const bool show_sync_paused_ui = error == sync_ui_util::AUTH_ERROR;
  const bool sync_disabled = !browser()->profile()->IsSyncAllowed();
  // Add profile hover button.
  auto current_profile_photo = std::make_unique<BadgedProfilePhoto>(
      show_sync_paused_ui
          ? BadgedProfilePhoto::BADGE_TYPE_SYNC_PAUSED
          : sync_disabled ? BadgedProfilePhoto::BADGE_TYPE_SYNC_DISABLED
                          : BadgedProfilePhoto::BADGE_TYPE_SYNC_ERROR,
      avatar_item.icon);
  HoverButton* current_profile = new HoverButton(
      this, std::move(current_profile_photo),
      l10n_util::GetStringUTF16(
          show_sync_paused_ui
              ? IDS_PROFILES_DICE_SYNC_PAUSED_TITLE
              : sync_disabled ? IDS_PROFILES_DICE_SYNC_DISABLED_TITLE
                              : IDS_SYNC_ERROR_USER_MENU_TITLE),
      avatar_item.username);

  if (!show_sync_paused_ui && !sync_disabled) {
    current_profile->SetStyle(HoverButton::STYLE_ERROR);
    current_profile->SetEnabled(false);
  }

  view->AddChildView(current_profile);
  current_profile_card_ = current_profile;

  if (sync_disabled)
    return view;

  // Add blue button.
  sync_error_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      this, l10n_util::GetStringUTF16(button_string_id));
  sync_error_button_->set_id(error);
  base::RecordAction(
      base::UserMetricsAction("ProfileChooser_SignInAgainDisplayed"));
  // Add horizontal and bottom margin to blue button.
  views::View* padded_view = new views::View();
  padded_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  padded_view->SetBorder(views::CreateEmptyBorder(
      0, kMenuEdgeMargin, kMenuEdgeMargin - current_profile_vertical_margin,
      kMenuEdgeMargin));
  padded_view->AddChildView(sync_error_button_);
  view->AddChildView(padded_view);
  return view;
}

views::View* ProfileChooserView::CreateCurrentProfileView(
    const AvatarMenu::Item& avatar_item,
    bool is_guest) {
  Profile* profile = browser()->profile();
  const bool sync_disabled = !profile->IsSyncAllowed();
  if (!is_guest && sync_disabled)
    return CreateDiceSyncErrorView(avatar_item, sync_ui_util::NO_SYNC_ERROR, 0);

  if (!avatar_item.signed_in && dice_enabled_ &&
      SyncPromoUI::ShouldShowSyncPromo(profile)) {
    return CreateDiceSigninView();
  }

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  views::View* view = new views::View();
  int content_list_vert_spacing =
      provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_SINGLE);
  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(content_list_vert_spacing, 0),
      0));

  auto current_profile_photo = std::make_unique<BadgedProfilePhoto>(
      GetProfileBadgeType(profile), avatar_item.icon);
  const base::string16 profile_name =
      profiles::GetAvatarNameForProfile(profile->GetPath());

  // Show the profile name by itself if not signed in or account consistency is
  // disabled. Otherwise, show the email attached to the profile.
  bool show_email = !is_guest && avatar_item.signed_in;
  const base::string16 hover_button_title =
      dice_enabled_ && profile->IsSyncAllowed() && show_email
          ? l10n_util::GetStringUTF16(IDS_PROFILES_SYNC_COMPLETE_TITLE)
          : profile_name;
  HoverButton* profile_card = new HoverButton(
      this, std::move(current_profile_photo), hover_button_title,
      show_email ? avatar_item.username : base::string16());
  // TODO(crbug.com/815047): Sometimes, |avatar_item.username| is empty when
  // |show_email| is true, which should never happen. This causes a crash when
  // setting the elision behavior, so until this bug is fixed, avoid the crash
  // by checking that the username is not empty.
  if (show_email && !avatar_item.username.empty())
    profile_card->SetSubtitleElideBehavior(gfx::ELIDE_EMAIL);
  current_profile_card_ = profile_card;
  view->AddChildView(current_profile_card_);

  if (is_guest) {
    current_profile_card_->SetEnabled(false);
    return view;
  }

  // The available links depend on the type of profile that is active.
  if (avatar_item.signed_in) {
    current_profile_card_->SetAccessibleName(
        l10n_util::GetStringFUTF16(
            IDS_PROFILES_EDIT_SIGNED_IN_PROFILE_ACCESSIBLE_NAME,
            profile_name,
            avatar_item.username));
    return view;
  }

  bool is_signin_allowed =
      profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed);
  if (!dice_enabled_ && is_signin_allowed) {
    views::View* extra_links_view = new views::View();
    extra_links_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical,
        gfx::Insets(provider->GetDistanceMetric(
                        views::DISTANCE_RELATED_CONTROL_VERTICAL),
                    kMenuEdgeMargin),
        kMenuEdgeMargin));
    views::Label* promo =
        new views::Label(l10n_util::GetStringUTF16(IDS_PROFILES_SIGNIN_PROMO));
    promo->SetMultiLine(true);
    promo->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    // Provide a hint to the layout manager by giving the promo text a maximum
    // width. This ensures it has the correct number of lines when determining
    // the initial Widget size.
    promo->SetMaximumWidth(menu_width());
    extra_links_view->AddChildView(promo);

    signin_current_profile_button_ =
        views::MdTextButton::CreateSecondaryUiBlueButton(
            this, l10n_util::GetStringFUTF16(
                      IDS_SYNC_START_SYNC_BUTTON_LABEL,
                      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
    extra_links_view->AddChildView(signin_current_profile_button_);
    signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
        signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
    extra_links_view->SetBorder(views::CreateEmptyBorder(
        0, 0,
        provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL),
        0));
    view->AddChildView(extra_links_view);
  }

  current_profile_card_->SetAccessibleName(
      l10n_util::GetStringFUTF16(
          IDS_PROFILES_EDIT_PROFILE_ACCESSIBLE_NAME, profile_name));
  return view;
}

views::View* ProfileChooserView::CreateDiceSigninView() {
  IncrementDiceSigninPromoShowCount();
  // Fetch signed in GAIA web accounts.
  dice_sync_promo_accounts_ =
      signin_ui_util::GetAccountsForDicePromos(browser()->profile());

  // Create a view that holds an illustration, a promo text and a button to turn
  // on Sync. The promo illustration is only shown the first 10 times per
  // profile.
  int promotext_top_spacing = 16;
  const int additional_bottom_spacing =
      dice_sync_promo_accounts_.empty() ? 0 : 8;
  views::View* view = new views::View();
  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      gfx::Insets(0, 0, additional_bottom_spacing, 0)));

  const bool promo_account_available = !dice_sync_promo_accounts_.empty();

  // Log sign-in impressions user metrics.
  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
  signin_metrics::RecordSigninImpressionWithAccountUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      promo_account_available);

  if (!promo_account_available) {
    // Show promo illustration+text when there is no promo account.
    if (GetDiceSigninPromoShowCount() <=
        kDiceSigninPromoIllustrationShowCountMax) {
      // Add the illustration.
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      views::ImageView* illustration = new NonAccessibleImageView();
      illustration->SetImage(
          *rb.GetNativeImageNamed(IDR_PROFILES_DICE_TURN_ON_SYNC)
               .ToImageSkia());
      view->AddChildView(illustration);
      // Adjust the spacing between illustration and promo text.
      promotext_top_spacing = 24;
    }
    // Add the promo text.
    views::Label* promo = new views::Label(
        l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SYNC_PROMO));
    promo->SetMultiLine(true);
    promo->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    promo->SetMaximumWidth(menu_width() - 2 * kMenuEdgeMargin);
    promo->SetBorder(views::CreateEmptyBorder(
        promotext_top_spacing, kMenuEdgeMargin, 0, kMenuEdgeMargin));
    view->AddChildView(promo);

    // Create a sign-in button without account information.
    dice_signin_button_view_ = new DiceSigninButtonView(this);
    dice_signin_button_view_->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(kMenuEdgeMargin)));
    view->AddChildView(dice_signin_button_view_);
    signin_current_profile_button_ = dice_signin_button_view_->signin_button();
    return view;
  }
  // Create a button to sign in the first account of
  // |dice_sync_promo_accounts_|.
  AccountInfo dice_promo_default_account = dice_sync_promo_accounts_[0];
  gfx::Image account_icon = dice_promo_default_account.account_image;
  if (account_icon.IsEmpty()) {
    account_icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }
  dice_signin_button_view_ =
      new DiceSigninButtonView(dice_promo_default_account, account_icon, this,
                               /*show_drop_down_arrow=*/false);
  signin_with_gaia_account_button_ = dice_signin_button_view_->signin_button();

  views::View* promo_button_container = new views::View();
  const int content_list_vert_spacing =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  const int bottom_spacing = kMenuEdgeMargin - content_list_vert_spacing;
  promo_button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      gfx::Insets(kMenuEdgeMargin, kMenuEdgeMargin, bottom_spacing,
                  kMenuEdgeMargin),
      content_list_vert_spacing));
  promo_button_container->AddChildView(dice_signin_button_view_);

  // Add sign out button.
  signout_button_ = views::MdTextButton::Create(
      this, l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT));
  promo_button_container->AddChildView(signout_button_);

  view->AddChildView(promo_button_container);

  return view;
}

views::View* ProfileChooserView::CreateGuestProfileView() {
  gfx::Image guest_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
  AvatarMenu::Item guest_avatar_item(0, base::FilePath(), guest_icon);
  guest_avatar_item.active = true;
  guest_avatar_item.name = l10n_util::GetStringUTF16(
      IDS_PROFILES_GUEST_PROFILE_NAME);
  guest_avatar_item.signed_in = false;

  return CreateCurrentProfileView(guest_avatar_item, true);
}

views::View* ProfileChooserView::CreateOptionsView(bool display_lock,
                                                   AvatarMenu* avatar_menu) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int content_list_vert_spacing =
      provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);

  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view, menu_width());

  const bool is_guest = browser()->profile()->IsGuestSession();
  // Add the user switching buttons.
  // Order them such that the active user profile comes first (for Dice).
  layout->StartRowWithPadding(1.0, 0, views::GridLayout::kFixedSize,
                              content_list_vert_spacing);
  std::vector<size_t> ordered_item_indices;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    if (avatar_menu->GetItemAt(i).active)
      ordered_item_indices.insert(ordered_item_indices.begin(), i);
    else
      ordered_item_indices.push_back(i);
  }
  for (size_t i : ordered_item_indices) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(i);
    if (!item.active) {
      gfx::Image image = profiles::GetSizedAvatarIcon(
          item.icon, true, kIconSize, kIconSize, profiles::SHAPE_CIRCLE);
      views::LabelButton* button =
          new HoverButton(this, *image.ToImageSkia(),
                          profiles::GetProfileSwitcherTextForItem(item));
      open_other_profile_indexes_map_[button] = i;

      if (!first_profile_button_)
        first_profile_button_ = button;
      layout->StartRow(1.0, 0);
      layout->AddView(button);
    }
  }

  UMA_HISTOGRAM_BOOLEAN("ProfileChooser.HasProfilesShown",
                        first_profile_button_);

  // Add the "Guest" button for browsing as guest
  if (!is_guest && !browser()->profile()->IsSupervised()) {
    PrefService* service = g_browser_process->local_state();
    DCHECK(service);
    if (service->GetBoolean(prefs::kBrowserGuestModeEnabled)) {
      guest_profile_button_ = new HoverButton(
          this, CreateVectorIcon(kUserMenuGuestIcon),
          l10n_util::GetStringUTF16(IDS_PROFILES_OPEN_GUEST_PROFILE_BUTTON));
      layout->StartRow(1.0, 0);
      layout->AddView(guest_profile_button_);
    }
  }

  base::string16 text = l10n_util::GetStringUTF16(
      is_guest ? IDS_PROFILES_EXIT_GUEST : IDS_PROFILES_MANAGE_USERS_BUTTON);
  const gfx::VectorIcon& settings_icon =
      is_guest ? kCloseAllIcon : kSettingsIcon;
  users_button_ = new HoverButton(this, CreateVectorIcon(settings_icon), text);

  layout->StartRow(1.0, 0);
  layout->AddView(users_button_);

  if (display_lock) {
    lock_button_ = new HoverButton(
        this,
        gfx::CreateVectorIcon(vector_icons::kLockIcon, kIconSize,
                              gfx::kChromeIconGrey),
        l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON));
    layout->StartRow(1.0, 0);
    layout->AddView(lock_button_);
  } else if (!is_guest) {
    AvatarMenu::Item active_avatar_item =
        avatar_menu->GetItemAt(ordered_item_indices[0]);
    close_all_windows_button_ = new HoverButton(
        this, CreateVectorIcon(kCloseAllIcon),
        avatar_menu->GetNumberOfItems() >= 2
            ? l10n_util::GetStringFUTF16(IDS_PROFILES_EXIT_PROFILE_BUTTON,
                                         active_avatar_item.name)
            : l10n_util::GetStringUTF16(IDS_PROFILES_CLOSE_ALL_WINDOWS_BUTTON));
    layout->StartRow(1.0, 0);
    layout->AddView(close_all_windows_button_);
  }

  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        content_list_vert_spacing);
  return view;
}

views::View* ProfileChooserView::CreateSupervisedUserDisclaimerView() {
  views::View* view = new views::View();
  int horizontal_margin = kMenuEdgeMargin;
  views::GridLayout* layout =
      CreateSingleColumnLayout(view, menu_width() - 2 * horizontal_margin);
  view->SetBorder(views::CreateEmptyBorder(0, horizontal_margin,
                                           kMenuEdgeMargin, horizontal_margin));

  views::Label* disclaimer = new views::Label(
      avatar_menu_->GetSupervisedUserInformation(), CONTEXT_BODY_TEXT_SMALL);
  disclaimer->SetMultiLine(true);
  disclaimer->SetAllowCharacterBreak(true);
  disclaimer->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRow(1.0, 0);
  layout->AddView(disclaimer);

  return view;
}

views::View* ProfileChooserView::CreateAutofillHomeView() {
  if (browser()->profile()->IsGuestSession())
    return nullptr;

  views::View* view = new views::View();
  view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  // Passwords.
  passwords_button_ =
      new HoverButton(this, CreateVectorIcon(kKeyIcon),
                      l10n_util::GetStringUTF16(IDS_PROFILES_PASSWORDS_LINK));
  view->AddChildView(passwords_button_);

  // Credit cards.
  credit_cards_button_ = new HoverButton(
      this, CreateVectorIcon(kCreditCardIcon),
      l10n_util::GetStringUTF16(IDS_PROFILES_CREDIT_CARDS_LINK));
  view->AddChildView(credit_cards_button_);

  // Addresses.
  addresses_button_ =
      new HoverButton(this, CreateVectorIcon(vector_icons::kLocationOnIcon),
                      l10n_util::GetStringUTF16(IDS_PROFILES_ADDRESSES_LINK));
  view->AddChildView(addresses_button_);
  return view;
}

bool ProfileChooserView::ShouldShowGoIncognito() const {
  bool incognito_available =
      IncognitoModePrefs::GetAvailability(browser()->profile()->GetPrefs()) !=
      IncognitoModePrefs::DISABLED;
  return incognito_available && !browser()->profile()->IsGuestSession();
}

void ProfileChooserView::PostActionPerformed(
    ProfileMetrics::ProfileDesktopMenu action_performed) {
  ProfileMetrics::LogProfileDesktopMenu(action_performed, gaia_service_type_);
  gaia_service_type_ = signin::GAIA_SERVICE_TYPE_NONE;
}

void ProfileChooserView::EnableSync(
    const base::Optional<AccountInfo>& account) {
  Hide();
  if (account)
    signin_ui_util::EnableSyncFromPromo(browser(), account.value(),
                                        access_point_,
                                        false /* is_default_promo_account */);
  else
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN);
}

void ProfileChooserView::SignOutAllWebAccounts() {
  Hide();
  IdentityManagerFactory::GetForProfile(browser()->profile())
      ->GetAccountsMutator()
      ->RemoveAllAccounts(signin_metrics::SourceForRefreshTokenOperation::
                              kUserMenu_SignOutAllAccounts);
}

int ProfileChooserView::GetDiceSigninPromoShowCount() const {
  return browser()->profile()->GetPrefs()->GetInteger(
      prefs::kDiceSigninUserMenuPromoCount);
}

void ProfileChooserView::IncrementDiceSigninPromoShowCount() {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDiceSigninUserMenuPromoCount, GetDiceSigninPromoShowCount() + 1);
}
