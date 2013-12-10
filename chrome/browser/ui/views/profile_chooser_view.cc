// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profile_chooser_view.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/user_manager_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/native_theme/native_theme_aura.h"
#endif

namespace {

// Helpers --------------------------------------------------------------------

const int kMinMenuWidth = 250;
const int kButtonHeight = 29;

// Creates a GridLayout with a single column. This ensures that all the child
// views added get auto-expanded to fill the full width of the bubble.
views::GridLayout* CreateSingleColumnLayout(views::View* view) {
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  return layout;
}

// Creates a GridLayout with two columns.
views::GridLayout* CreateDoubleColumnLayout(views::View* view) {
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, views::kUnrelatedControlLargeHorizontalSpacing);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  return layout;
}

views::Link* CreateLink(const base::string16& link_text,
                        views::LinkListener* listener) {
  views::Link* link_button = new views::Link(link_text);
  link_button->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  link_button->SetUnderline(false);
  link_button->set_listener(listener);
  return link_button;
}


// BackgroundColorHoverButton -------------------------------------------------

// A custom button that allows for setting a background color when hovered over.
class BackgroundColorHoverButton : public views::TextButton {
 public:
  BackgroundColorHoverButton(views::ButtonListener* listener,
                             const base::string16& text,
                             const gfx::ImageSkia& normal_icon,
                             const gfx::ImageSkia& hover_icon);
  virtual ~BackgroundColorHoverButton();

 private:
  // views::TextButton:
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;

  void OnHighlightStateChanged();

  DISALLOW_COPY_AND_ASSIGN(BackgroundColorHoverButton);
};

BackgroundColorHoverButton::BackgroundColorHoverButton(
    views::ButtonListener* listener,
    const base::string16& text,
    const gfx::ImageSkia& normal_icon,
    const gfx::ImageSkia& hover_icon)
    : views::TextButton(listener, text) {
  scoped_ptr<views::TextButtonBorder> text_button_border(
      new views::TextButtonBorder());
  text_button_border->SetInsets(gfx::Insets(0, views::kButtonHEdgeMarginNew,
                                            0, views::kButtonHEdgeMarginNew));
  set_border(text_button_border.release());
  set_min_height(kButtonHeight);
  set_icon_text_spacing(views::kItemLabelSpacing);
  SetIcon(normal_icon);
  SetHoverIcon(hover_icon);
  SetPushedIcon(hover_icon);
  SetHoverColor(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_SelectedMenuItemForegroundColor));
  OnHighlightStateChanged();
}

BackgroundColorHoverButton::~BackgroundColorHoverButton() {
}

void BackgroundColorHoverButton::OnMouseEntered(const ui::MouseEvent& event) {
  views::TextButton::OnMouseEntered(event);
  OnHighlightStateChanged();
}

void BackgroundColorHoverButton::OnMouseExited(const ui::MouseEvent& event) {
  views::TextButton::OnMouseExited(event);
  OnHighlightStateChanged();
}

void BackgroundColorHoverButton::OnHighlightStateChanged() {
  bool is_highlighted = (state() == views::TextButton::STATE_PRESSED) ||
      (state() == views::TextButton::STATE_HOVERED) || HasFocus();
  set_background(views::Background::CreateSolidBackground(
      GetNativeTheme()->GetSystemColor(is_highlighted ?
          ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor :
          ui::NativeTheme::kColorId_MenuBackgroundColor)));
  SchedulePaint();
}

}  // namespace


// EditableProfilePhoto -------------------------------------------------

// A custom Image control that shows a "change" button when moused over.
class EditableProfilePhoto : public views::ImageView {
 public:
  EditableProfilePhoto(views::ButtonListener* listener,
                       const gfx::Image& icon,
                       bool is_editing_allowed)
      : views::ImageView(),
        change_photo_button_(NULL) {
    const int kLargeImageSide = 64;
    const SkColor kBackgroundColor = SkColorSetARGB(125, 0, 0, 0);
    const int kOverlayHeight = 20;

    gfx::Image image = profiles::GetSizedAvatarIconWithBorder(
        icon, true,
        kLargeImageSide + profiles::kAvatarIconPadding,
        kLargeImageSide + profiles::kAvatarIconPadding);
    SetImage(image.ToImageSkia());

    if (!is_editing_allowed)
      return;

    set_notify_enter_exit_on_child(true);

    // Button overlay that appears when hovering over the image.
    change_photo_button_ = new views::TextButton(listener,
        l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_CHANGE_PHOTO_BUTTON));
    change_photo_button_->set_alignment(views::TextButton::ALIGN_CENTER);
    change_photo_button_->set_border(NULL);
    change_photo_button_->SetEnabledColor(SK_ColorWHITE);
    change_photo_button_->SetHoverColor(SK_ColorWHITE);

    change_photo_button_->set_background(
        views::Background::CreateSolidBackground(kBackgroundColor));
    // Need to take in account the border padding on the avatar.
    change_photo_button_->SetBounds(
        profiles::kAvatarIconPadding,
        kLargeImageSide - kOverlayHeight,
        kLargeImageSide - profiles::kAvatarIconPadding,
        kOverlayHeight);
    change_photo_button_->SetVisible(false);
    AddChildView(change_photo_button_);
  }

  views::TextButton* change_photo_button() {
    return change_photo_button_;
  }

 private:
  // views::View:
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE {
    if (change_photo_button_)
      change_photo_button_->SetVisible(true);
  }

  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE {
    if (change_photo_button_)
      change_photo_button_->SetVisible(false);
  }

  // Button that is shown when hovering over the image view. Can be NULL if
  // the photo isn't allowed to be edited (e.g. for guest profiles).
  views::TextButton* change_photo_button_;

  DISALLOW_COPY_AND_ASSIGN(EditableProfilePhoto);
};


// EditableProfileName -------------------------------------------------

// A custom text control that turns into a textfield for editing when clicked.
class EditableProfileName : public views::TextButton,
                            public views::ButtonListener {
 public:
  EditableProfileName(views::TextfieldController* controller,
                      const base::string16& text,
                      bool is_editing_allowed)
      : views::TextButton(this, text),
        profile_name_textfield_(NULL) {
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    gfx::Font medium_font = rb->GetFont(ui::ResourceBundle::MediumFont);
    SetFont(medium_font);
    set_border(NULL);

    if (!is_editing_allowed)
      return;

    SetIcon(*rb->GetImageSkiaNamed(IDR_INFOBAR_AUTOFILL));
    set_icon_placement(views::TextButton::ICON_ON_RIGHT);

    // Textfield that overlaps the button.
    profile_name_textfield_ = new views::Textfield();
    profile_name_textfield_->SetController(controller);
    profile_name_textfield_->SetFont(medium_font);
    profile_name_textfield_->SetVisible(false);
    AddChildView(profile_name_textfield_);
  }

  views::Textfield* profile_name_textfield() {
    return profile_name_textfield_;
  }

  // Hide the editable textfield and show the button displaying the profile
  // name instead.
  void ShowReadOnlyView() {
    if (profile_name_textfield_)
      profile_name_textfield_->SetVisible(false);
  }

 private:
  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                            const ui::Event& event) OVERRIDE {
    if (profile_name_textfield_) {
      profile_name_textfield_->SetVisible(true);
      profile_name_textfield_->SetText(text());
      profile_name_textfield_->SelectAll(false);
      profile_name_textfield_->RequestFocus();
    }
  }

  // views::CustomButton:
  virtual bool OnKeyReleased(const ui::KeyEvent& event) OVERRIDE {
    // Override CustomButton's implementation, which presses the button when
    // you press space and clicks it when you release space, as the space can be
    // part of the new profile name typed in the textfield.
    return false;
  }

  // views::View:
  virtual void Layout() OVERRIDE {
    if (profile_name_textfield_)
      profile_name_textfield_->SetBounds(0, 0, width(), height());
    views::View::Layout();
  }

  // Button that is shown when hovering over the image view. Can be NULL if
  // the profile name isn't allowed to be edited (e.g. for guest profiles).
  views::Textfield* profile_name_textfield_;

  DISALLOW_COPY_AND_ASSIGN(EditableProfileName);
};


// ProfileChooserView ---------------------------------------------------------

// static
ProfileChooserView* ProfileChooserView::profile_bubble_ = NULL;
bool ProfileChooserView::close_on_deactivate_for_testing_ = true;

// static
void ProfileChooserView::ShowBubble(
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    views::BubbleBorder::BubbleAlignment border_alignment,
    const gfx::Rect& anchor_rect,
    Browser* browser) {
  if (IsShowing())
    // TODO(bcwhite): handle case where we should show on different window
    return;

  profile_bubble_ = new ProfileChooserView(
      anchor_view, arrow, anchor_rect, browser);
  views::BubbleDelegateView::CreateBubble(profile_bubble_);
  profile_bubble_->set_close_on_deactivate(close_on_deactivate_for_testing_);
  profile_bubble_->SetAlignment(border_alignment);
  profile_bubble_->GetWidget()->Show();
  profile_bubble_->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);
}

// static
bool ProfileChooserView::IsShowing() {
  return profile_bubble_ != NULL;
}

// static
void ProfileChooserView::Hide() {
  if (IsShowing())
    profile_bubble_->GetWidget()->Close();
}

ProfileChooserView::ProfileChooserView(views::View* anchor_view,
                                       views::BubbleBorder::Arrow arrow,
                                       const gfx::Rect& anchor_rect,
                                       Browser* browser)
    : BubbleDelegateView(anchor_view, arrow),
      browser_(browser),
      view_mode_(PROFILE_CHOOSER_VIEW) {
  // Reset the default margins inherited from the BubbleDelegateView.
  set_margins(gfx::Insets());

  ResetView();

  avatar_menu_.reset(new AvatarMenu(
      &g_browser_process->profile_manager()->GetProfileInfoCache(),
      this,
      browser_));
  avatar_menu_->RebuildMenu();

  ProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
  if (oauth2_token_service)
    oauth2_token_service->AddObserver(this);
}

ProfileChooserView::~ProfileChooserView() {
  ProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
  if (oauth2_token_service)
    oauth2_token_service->RemoveObserver(this);
}

void ProfileChooserView::ResetView() {
  manage_accounts_link_ = NULL;
  signout_current_profile_link_ = NULL;
  signin_current_profile_link_ = NULL;
  guest_button_ = NULL;
  end_guest_button_ = NULL;
  users_button_ = NULL;
  add_user_button_ = NULL;
  add_account_button_ = NULL;
  current_profile_photo_ = NULL;
  current_profile_name_ = NULL;
  open_other_profile_indexes_map_.clear();
  current_profile_accounts_map_.clear();
}

void ProfileChooserView::Init() {
  ShowView(PROFILE_CHOOSER_VIEW, avatar_menu_.get());
}

void ProfileChooserView::OnAvatarMenuChanged(
    AvatarMenu* avatar_menu) {
  // Refresh the view with the new menu. We can't just update the local copy
  // as this may have been triggered by a sign out action, in which case
  // the view is being destroyed.
  ShowView(PROFILE_CHOOSER_VIEW, avatar_menu);
}

void ProfileChooserView::OnRefreshTokenAvailable(
    const std::string& account_id) {
  // Refresh the account management view when a new account is added to the
  // profile.
  if (view_mode_ == ACCOUNT_MANAGEMENT_VIEW ||
      view_mode_ == GAIA_SIGNIN_VIEW ||
      view_mode_ == GAIA_ADD_ACCOUNT_VIEW) {
    ShowView(ACCOUNT_MANAGEMENT_VIEW, avatar_menu_.get());
  }
}

void ProfileChooserView::OnRefreshTokenRevoked(const std::string& account_id) {
  // Refresh the account management view when an account is removed from the
  // profile.
  if (view_mode_ == ACCOUNT_MANAGEMENT_VIEW)
    ShowView(ACCOUNT_MANAGEMENT_VIEW, avatar_menu_.get());
}

void ProfileChooserView::ShowView(BubbleViewMode view_to_display,
                                  AvatarMenu* avatar_menu) {
  // The account management view should only be displayed if the active profile
  // is signed in.
  if (view_to_display == ACCOUNT_MANAGEMENT_VIEW) {
    const AvatarMenu::Item& active_item = avatar_menu->GetItemAt(
        avatar_menu->GetActiveProfileIndex());
    DCHECK(active_item.signed_in);
  }

  ResetView();
  RemoveAllChildViews(true);
  view_mode_ = view_to_display;

  views::GridLayout* layout = CreateSingleColumnLayout(this);
  layout->set_minimum_size(gfx::Size(kMinMenuWidth, 0));

  if (view_to_display == GAIA_SIGNIN_VIEW ||
      view_to_display == GAIA_ADD_ACCOUNT_VIEW) {
    // Minimum size for embedded sign in pages as defined in Gaia.
    const int kMinGaiaViewWidth = 320;
    const int kMinGaiaViewHeight = 440;
    Profile* profile = browser_->profile();
    views::WebView* web_view = new views::WebView(profile);
    signin::Source source = (view_to_display == GAIA_SIGNIN_VIEW) ?
        signin::SOURCE_AVATAR_BUBBLE_SIGN_IN :
        signin::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT;
    GURL url(signin::GetPromoURL(
        source, false /* auto_close */, true /* is_constrained */));
    web_view->LoadInitialURL(url);
    layout->StartRow(1, 0);
    layout->AddView(web_view);
    layout->set_minimum_size(
        gfx::Size(kMinGaiaViewWidth, kMinGaiaViewHeight));
    Layout();
    if (GetBubbleFrameView())
      SizeToContents();
    return;
  }

  // Separate items into active and alternatives.
  Indexes other_profiles;
  bool is_guest_view = true;
  views::View* current_profile_view = NULL;
  views::View* current_profile_accounts = NULL;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(i);
    if (item.active) {
      if (view_to_display == PROFILE_CHOOSER_VIEW) {
        current_profile_view = CreateCurrentProfileView(item, false);
      } else {
        current_profile_view = CreateCurrentProfileEditableView(item);
        current_profile_accounts = CreateCurrentProfileAccountsView(item);
      }
      is_guest_view = false;
    } else {
      other_profiles.push_back(i);
    }
  }

  if (!current_profile_view)  // Guest windows don't have an active profile.
    current_profile_view = CreateGuestProfileView();

  layout->StartRow(1, 0);
  layout->AddView(current_profile_view);

  if (view_to_display == PROFILE_CHOOSER_VIEW) {
    layout->StartRow(1, 0);
    layout->AddView(CreateOtherProfilesView(other_profiles));
  } else {
    DCHECK(current_profile_accounts);
    layout->StartRow(0, 0);
    layout->AddView(new views::Separator(views::Separator::HORIZONTAL));
    layout->StartRow(1, 0);
    layout->AddView(current_profile_accounts);
  }

  layout->StartRow(0, 0);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

  // Action buttons.
  views::View* option_buttons_view = CreateOptionsView(is_guest_view);
  layout->StartRow(0, 0);
  layout->AddView(option_buttons_view);

  Layout();
  if (GetBubbleFrameView())
    SizeToContents();
}

void ProfileChooserView::WindowClosing() {
  DCHECK_EQ(profile_bubble_, this);
  profile_bubble_ = NULL;
}

void ProfileChooserView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  // Disable button after clicking so that it doesn't get clicked twice and
  // start a second action... which can crash Chrome.  But don't disable if it
  // has no parent (like in tests) because that will also crash.
  if (sender->parent())
    sender->SetEnabled(false);

  if (sender == guest_button_) {
    profiles::SwitchToGuestProfile(browser_->host_desktop_type(),
                                   profiles::ProfileSwitchingDoneCallback());
  } else if (sender == end_guest_button_) {
    profiles::CloseGuestProfileWindows();
  } else if (sender == users_button_) {
    // Only non-guest users appear in the User Manager.
    base::FilePath profile_path;
    if (!end_guest_button_) {
      size_t active_index = avatar_menu_->GetActiveProfileIndex();
      profile_path = avatar_menu_->GetItemAt(active_index).profile_path;
    }
    chrome::ShowUserManager(profile_path);
  } else if (sender == add_user_button_) {
    profiles::CreateAndSwitchToNewProfile(
        browser_->host_desktop_type(),
        profiles::ProfileSwitchingDoneCallback());
  } else if (sender == add_account_button_) {
    ShowView(GAIA_ADD_ACCOUNT_VIEW, avatar_menu_.get());
  } else if (sender == current_profile_photo_->change_photo_button()) {
    avatar_menu_->EditProfile(avatar_menu_->GetActiveProfileIndex());
  } else {
    // One of the "other profiles" buttons was pressed.
    ButtonIndexes::const_iterator match =
        open_other_profile_indexes_map_.find(sender);
    DCHECK(match != open_other_profile_indexes_map_.end());
    avatar_menu_->SwitchToProfile(
        match->second,
        ui::DispositionFromEventFlags(event.flags()) == NEW_WINDOW);
  }
}

void ProfileChooserView::OnMenuButtonClicked(views::View* source,
                                             const gfx::Point& point) {
  AccountButtonIndexes::const_iterator match =
      current_profile_accounts_map_.find(source);
  DCHECK(match != current_profile_accounts_map_.end());

  ProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
  if (oauth2_token_service)
    oauth2_token_service->RevokeCredentials(match->second);
}

void ProfileChooserView::LinkClicked(views::Link* sender, int event_flags) {
  if (sender == manage_accounts_link_) {
    // ShowView() will DCHECK if this view is displayed for non signed-in users.
    ShowView(ACCOUNT_MANAGEMENT_VIEW, avatar_menu_.get());
  } else if (sender == signout_current_profile_link_) {
    profiles::LockProfile(browser_->profile());
  } else {
    DCHECK(sender == signin_current_profile_link_);
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableInlineSignin)) {
      ShowView(GAIA_SIGNIN_VIEW, avatar_menu_.get());
    } else {
      GURL page = signin::GetPromoURL(signin::SOURCE_MENU, false);
      chrome::ShowSingletonTab(browser_, page);
    }
  }
}

bool ProfileChooserView::HandleKeyEvent(views::Textfield* sender,
                                        const ui::KeyEvent& key_event) {
  views::Textfield* name_textfield =
      current_profile_name_->profile_name_textfield();
  DCHECK(sender == name_textfield);

  if (key_event.key_code() == ui::VKEY_RETURN ||
      key_event.key_code() == ui::VKEY_TAB) {
    // Pressing Tab/Enter commits the new profile name, unless it's empty.
    base::string16 new_profile_name = name_textfield->text();
    if (new_profile_name.empty())
      return true;

    const AvatarMenu::Item& active_item = avatar_menu_->GetItemAt(
        avatar_menu_->GetActiveProfileIndex());
    Profile* profile = g_browser_process->profile_manager()->GetProfile(
        active_item.profile_path);
    DCHECK(profile);

    if (profile->IsManaged())
      return true;

    profiles::UpdateProfileName(profile, new_profile_name);
    current_profile_name_->ShowReadOnlyView();
    return true;
  }
  return false;
}

views::View* ProfileChooserView::CreateCurrentProfileView(
    const AvatarMenu::Item& avatar_item,
    bool is_guest) {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateDoubleColumnLayout(view);
  layout->SetInsets(views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew);

  current_profile_photo_ =
      new EditableProfilePhoto(this, avatar_item.icon, !is_guest);
  view->SetBoundsRect(current_profile_photo_->bounds());
  current_profile_name_ =
      new EditableProfileName(this, avatar_item.name, !is_guest);

  layout->StartRow(1, 0);
  layout->AddView(current_profile_photo_, 1, 3);
  layout->AddView(current_profile_name_);

  if (is_guest) {
    layout->StartRow(1, 0);
    layout->SkipColumns(1);
    layout->StartRow(1, 0);
    layout->SkipColumns(1);
  } else if (avatar_item.signed_in) {
    manage_accounts_link_ = CreateLink(
        l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_MANAGE_ACCOUNTS_BUTTON),
        this);
    signout_current_profile_link_ = CreateLink(
        l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON), this);
    layout->StartRow(1, 0);
    layout->SkipColumns(1);
    layout->AddView(signout_current_profile_link_);
    layout->StartRow(1, 0);
    layout->SkipColumns(1);
    layout->AddView(manage_accounts_link_);
  } else {
    signin_current_profile_link_ = CreateLink(
        l10n_util::GetStringFUTF16(
            IDS_SYNC_START_SYNC_BUTTON_LABEL,
            l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)),
        this);
    layout->StartRow(1, 0);
    layout->SkipColumns(1);
    layout->AddView(signin_current_profile_link_);
    layout->StartRow(1, 0);
    layout->SkipColumns(1);
  }

  return view;
}

views::View* ProfileChooserView::CreateCurrentProfileEditableView(
    const AvatarMenu::Item& avatar_item) {
  DCHECK(avatar_item.signed_in);
  views::View* view = new views::View();
  views::GridLayout* layout = CreateDoubleColumnLayout(view);
  layout->SetInsets(views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew);

  current_profile_photo_ =
      new EditableProfilePhoto(this, avatar_item.icon, true);
  view->SetBoundsRect(current_profile_photo_->bounds());
  current_profile_name_ =
      new EditableProfileName(this, avatar_item.name, true);

  layout->StartRow(1, 0);
  layout->AddView(current_profile_photo_, 1, 3);
  layout->AddView(current_profile_name_);

  layout->StartRow(1, 0);
  layout->SkipColumns(1);

  layout->StartRow(1, 0);
  layout->SkipColumns(1);
  return view;
}

views::View* ProfileChooserView::CreateGuestProfileView() {
  gfx::Image guest_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(IDR_LOGIN_GUEST);
  AvatarMenu::Item guest_avatar_item(0, 0, guest_icon);
  guest_avatar_item.active = true;
  guest_avatar_item.name = l10n_util::GetStringUTF16(
      IDS_PROFILES_GUEST_PROFILE_NAME);
  guest_avatar_item.signed_in = false;

  return CreateCurrentProfileView(guest_avatar_item, true);
}

views::View* ProfileChooserView::CreateOtherProfilesView(
    const Indexes& avatars_to_show) {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view);
  layout->SetInsets(0, views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew, views::kButtonHEdgeMarginNew);
  int num_avatars_to_show = avatars_to_show.size();
  for (int i = 0; i < num_avatars_to_show; ++i) {
    const size_t index = avatars_to_show[i];
    const AvatarMenu::Item& item = avatar_menu_->GetItemAt(index);
    const int kSmallImageSide = 32;

    gfx::Image image = profiles::GetSizedAvatarIconWithBorder(
        item.icon, true,
        kSmallImageSide + profiles::kAvatarIconPadding,
        kSmallImageSide + profiles::kAvatarIconPadding);

    views::TextButton* button = new views::TextButton(this, item.name);
    open_other_profile_indexes_map_[button] = index;
    button->SetIcon(*image.ToImageSkia());
    button->set_icon_text_spacing(views::kItemLabelSpacing);
    button->SetFont(ui::ResourceBundle::GetSharedInstance().GetFont(
        ui::ResourceBundle::MediumFont));
    button->set_border(NULL);

    layout->StartRow(1, 0);
    layout->AddView(button);

    // The last avatar in the list does not need any bottom padding.
    if (i < num_avatars_to_show - 1)
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }

  return view;
}

views::View* ProfileChooserView::CreateOptionsView(bool is_guest_view) {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view);
  // The horizontal padding will be set by each button individually, so that
  // in the hovered state the button spans the entire parent view.
  layout->SetInsets(views::kRelatedControlVerticalSpacing, 0,
                    views::kRelatedControlVerticalSpacing, 0);

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();

  layout->StartRow(1, 0);
  if (is_guest_view) {
    end_guest_button_ = new BackgroundColorHoverButton(
        this,
        l10n_util::GetStringUTF16(IDS_PROFILES_EXIT_GUEST_BUTTON),
        *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_BROWSE_GUEST),
        *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_BROWSE_GUEST_WHITE));
    layout->AddView(end_guest_button_);
  } else {
    guest_button_ = new BackgroundColorHoverButton(
        this,
        l10n_util::GetStringUTF16(IDS_PROFILES_GUEST_BUTTON),
        *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_BROWSE_GUEST),
        *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_BROWSE_GUEST_WHITE));
    layout->AddView(guest_button_);
  }

  add_user_button_ = new BackgroundColorHoverButton(
      this,
      l10n_util::GetStringUTF16(IDS_PROFILES_ADD_PERSON_BUTTON),
      *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_ADD_USER),
      *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_ADD_USER_WHITE));
  layout->StartRow(1, 0);
  layout->AddView(add_user_button_);

  users_button_ = new BackgroundColorHoverButton(
      this,
      l10n_util::GetStringUTF16(IDS_PROFILES_ALL_PEOPLE_BUTTON),
      *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_ADD_USER),
      *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_ADD_USER_WHITE));
  layout->StartRow(1, 0);
  layout->AddView(users_button_);

  return view;
}

views::View* ProfileChooserView::CreateCurrentProfileAccountsView(
    const AvatarMenu::Item& avatar_item) {
  DCHECK(avatar_item.signed_in);
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view);
  layout->SetInsets(views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew);

  Profile* profile = browser_->profile();
  std::string primary_account =
      SigninManagerFactory::GetForProfile(profile)->GetAuthenticatedUsername();
  DCHECK(!primary_account.empty());
  std::vector<std::string> accounts(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->GetAccounts());
  DCHECK_EQ(1, std::count_if(accounts.begin(), accounts.end(),
                             std::bind1st(std::equal_to<std::string>(),
                                          primary_account)));

  // The primary account should always be listed first.  However, the vector
  // returned by ProfileOAuth2TokenService::GetAccounts() will contain the
  // primary account too.  Ignore it when it appears later.
  // TODO(rogerta): we still need to further differentiate the primary account
  // from the others, so more work is likely required here: crbug.com/311124.
  CreateAccountButton(layout, primary_account, true);
  for (size_t i = 0; i < accounts.size(); ++i) {
    if (primary_account != accounts[i])
      CreateAccountButton(layout, accounts[i], false);
  }

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  add_account_button_ = new views::BlueButton(
      this,
      l10n_util::GetStringFUTF16(IDS_PROFILES_PROFILE_ADD_ACCOUNT_BUTTON,
                                 avatar_item.name));
  layout->StartRow(1, 0);
  layout->AddView(add_account_button_);
  return view;
}

void ProfileChooserView::CreateAccountButton(views::GridLayout* layout,
                                             const std::string& account,
                                             bool is_primary_account) {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  // Use a MenuButtonListener and not a regular ButtonListener to be
  // able to distinguish between the unnamed "other profile" buttons and the
  // unnamed "multiple accounts" buttons.
  views::MenuButton* email_button = new views::MenuButton(
      NULL,
      gfx::ElideEmail(UTF8ToUTF16(account),
                      rb->GetFontList(ui::ResourceBundle::BaseFont),
                      width()),
      is_primary_account ? NULL : this,  // Cannot delete the primary account.
      !is_primary_account);
  email_button->SetFont(rb->GetFont(ui::ResourceBundle::BaseFont));
  email_button->set_border(views::Border::CreateEmptyBorder(0, 0, 0, 0));
  if (!is_primary_account) {
    email_button->set_menu_marker(
        rb->GetImageNamed(IDR_CLOSE_1).ToImageSkia());
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }
  layout->StartRow(1, 0);
  layout->AddView(email_button);

  // Save the original email address, as the button text could be elided.
  current_profile_accounts_map_[email_button] = account;
}
