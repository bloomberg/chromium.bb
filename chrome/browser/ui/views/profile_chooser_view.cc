// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profile_chooser_view.h"

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/user_manager_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/mutable_profile_oauth2_token_service.h"
#include "components/signin/core/profile_oauth2_token_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

// Helpers --------------------------------------------------------------------

const int kFixedMenuWidth = 250;
const int kButtonHeight = 29;
const int kProfileAvatarTutorialShowMax = 5;

// Creates a GridLayout with a single column. This ensures that all the child
// views added get auto-expanded to fill the full width of the bubble.
views::GridLayout* CreateSingleColumnLayout(views::View* view, int width) {
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                     views::GridLayout::FIXED, width, width);
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
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
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
class BackgroundColorHoverButton : public views::LabelButton {
 public:
  BackgroundColorHoverButton(views::ButtonListener* listener,
                             const base::string16& text,
                             const gfx::ImageSkia& normal_icon,
                             const gfx::ImageSkia& hover_icon);
  virtual ~BackgroundColorHoverButton();

 private:
  // views::LabelButton:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(BackgroundColorHoverButton);
};

BackgroundColorHoverButton::BackgroundColorHoverButton(
    views::ButtonListener* listener,
    const base::string16& text,
    const gfx::ImageSkia& normal_icon,
    const gfx::ImageSkia& hover_icon)
    : views::LabelButton(listener, text) {
  SetBorder(views::Border::CreateEmptyBorder(0, views::kButtonHEdgeMarginNew,
                                             0, views::kButtonHEdgeMarginNew));
  set_min_size(gfx::Size(0, kButtonHeight));
  SetImage(STATE_NORMAL, normal_icon);
  SetImage(STATE_HOVERED, hover_icon);
  SetImage(STATE_PRESSED, hover_icon);
  SetTextColor(STATE_HOVERED, GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_SelectedMenuItemForegroundColor));
}

BackgroundColorHoverButton::~BackgroundColorHoverButton() {}

void BackgroundColorHoverButton::OnPaint(gfx::Canvas* canvas) {
  if ((state() == STATE_PRESSED) || (state() == STATE_HOVERED) || HasFocus()) {
    canvas->DrawColor(GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor));
  }
  LabelButton::OnPaint(canvas);
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
    gfx::Image image = profiles::GetSizedAvatarIconWithBorder(
        icon, true,
        kLargeImageSide + profiles::kAvatarIconPadding,
        kLargeImageSide + profiles::kAvatarIconPadding);
    SetImage(image.ToImageSkia());

    if (!is_editing_allowed)
      return;

    set_notify_enter_exit_on_child(true);

    // Button overlay that appears when hovering over the image.
    change_photo_button_ = new views::LabelButton(listener,
        l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_CHANGE_PHOTO_BUTTON));
    change_photo_button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    change_photo_button_->SetBorder(views::Border::NullBorder());
    const SkColor color = SK_ColorWHITE;
    change_photo_button_->SetTextColor(views::Button::STATE_NORMAL, color);
    change_photo_button_->SetTextColor(views::Button::STATE_HOVERED, color);

    const SkColor kBackgroundColor = SkColorSetARGB(125, 0, 0, 0);
    change_photo_button_->set_background(
        views::Background::CreateSolidBackground(kBackgroundColor));
    // Need to take into account the border padding on the avatar.
    const int kOverlayHeight = 20;
    change_photo_button_->SetBounds(
        profiles::kAvatarIconPadding,
        kLargeImageSide - kOverlayHeight,
        kLargeImageSide - profiles::kAvatarIconPadding,
        kOverlayHeight);
    change_photo_button_->SetVisible(false);
    AddChildView(change_photo_button_);
  }

  views::LabelButton* change_photo_button() { return change_photo_button_; }

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
  views::LabelButton* change_photo_button_;

  DISALLOW_COPY_AND_ASSIGN(EditableProfilePhoto);
};


// EditableProfileName -------------------------------------------------

// A custom text control that turns into a textfield for editing when clicked.
class EditableProfileName : public views::LabelButton,
                            public views::ButtonListener {
 public:
  EditableProfileName(views::TextfieldController* controller,
                      const base::string16& text,
                      bool is_editing_allowed)
      : views::LabelButton(this, text),
        profile_name_textfield_(NULL) {
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    const gfx::FontList& medium_font_list =
        rb->GetFontList(ui::ResourceBundle::MediumFont);
    SetFontList(medium_font_list);
    SetBorder(views::Border::NullBorder());

    if (!is_editing_allowed)
      return;

    SetImage(STATE_HOVERED,
             *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_EDIT_HOVER));
    SetImage(STATE_PRESSED,
             *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_EDIT_PRESSED));

    // Textfield that overlaps the button.
    profile_name_textfield_ = new views::Textfield();
    profile_name_textfield_->set_controller(controller);
    profile_name_textfield_->SetFontList(medium_font_list);
    profile_name_textfield_->SetVisible(false);
    AddChildView(profile_name_textfield_);
  }

  views::Textfield* profile_name_textfield() {
    return profile_name_textfield_;
  }

  // Hide the editable textfield to show the profile name button instead.
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
      profile_name_textfield_->SetText(GetText());
      profile_name_textfield_->SelectAll(false);
      profile_name_textfield_->RequestFocus();
    }
  }

  // views::LabelButton:
  virtual bool OnKeyReleased(const ui::KeyEvent& event) OVERRIDE {
    // Override CustomButton's implementation, which presses the button when
    // you press space and clicks it when you release space, as the space can be
    // part of the new profile name typed in the textfield.
    return false;
  }

  virtual void Layout() OVERRIDE {
    if (profile_name_textfield_)
      profile_name_textfield_->SetBounds(0, 0, width(), height());
    // This layout trick keeps the text left-aligned and the icon right-aligned.
    SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    views::LabelButton::Layout();
    label()->SetHorizontalAlignment(gfx::ALIGN_LEFT);
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
      view_mode_(PROFILE_CHOOSER_VIEW),
      tutorial_showing_(false) {
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
  tutorial_ok_button_ = NULL;
  tutorial_learn_more_link_ = NULL;
  open_other_profile_indexes_map_.clear();
  current_profile_accounts_map_.clear();
  tutorial_showing_ = false;
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

  // Records if the tutorial card is currently shown before resetting the view.
  bool tutorial_shown = tutorial_showing_;
  ResetView();
  RemoveAllChildViews(true);
  view_mode_ = view_to_display;

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
    views::GridLayout* layout =
        CreateSingleColumnLayout(this, kMinGaiaViewWidth);
    layout->StartRow(1, 0);
    layout->AddView(web_view);
    layout->set_minimum_size(
        gfx::Size(kMinGaiaViewWidth, kMinGaiaViewHeight));
    Layout();
    if (GetBubbleFrameView())
      SizeToContents();
    return;
  }

  views::GridLayout* layout = CreateSingleColumnLayout(this, kFixedMenuWidth);
  // Separate items into active and alternatives.
  Indexes other_profiles;
  bool is_guest_view = true;
  views::View* tutorial_view = NULL;
  views::View* current_profile_view = NULL;
  views::View* current_profile_accounts = NULL;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(i);
    if (item.active) {
      if (view_to_display == PROFILE_CHOOSER_VIEW) {
        tutorial_view = CreateTutorialView(item, tutorial_shown);
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

  if (tutorial_view) {
    layout->StartRow(1, 0);
    layout->AddView(tutorial_view);
    layout->StartRow(0, 0);
    layout->AddView(new views::Separator(views::Separator::HORIZONTAL));
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
        profiles::ProfileSwitchingDoneCallback(),
        ProfileMetrics::ADD_NEW_USER_ICON);
  } else if (sender == add_account_button_) {
    ShowView(GAIA_ADD_ACCOUNT_VIEW, avatar_menu_.get());
  } else if (sender == current_profile_photo_->change_photo_button()) {
    avatar_menu_->EditProfile(avatar_menu_->GetActiveProfileIndex());
  } else if (sender == tutorial_ok_button_) {
    // If the user manually dismissed the tutorial, never show it again by
    // setting the number of times shown to the maximum plus 1, so that later we
    // could distinguish between the dismiss case and the case when the tutorial
    // is indeed shown for the maximum number of times.
    browser_->profile()->GetPrefs()->SetInteger(
        prefs::kProfileAvatarTutorialShown, kProfileAvatarTutorialShowMax + 1);
    ShowView(PROFILE_CHOOSER_VIEW, avatar_menu_.get());
  } else {
    // One of the "other profiles" buttons was pressed.
    ButtonIndexes::const_iterator match =
        open_other_profile_indexes_map_.find(sender);
    DCHECK(match != open_other_profile_indexes_map_.end());
    avatar_menu_->SwitchToProfile(
        match->second,
        ui::DispositionFromEventFlags(event.flags()) == NEW_WINDOW,
        ProfileMetrics::SWITCH_PROFILE_ICON);
  }
}

void ProfileChooserView::OnMenuButtonClicked(views::View* source,
                                             const gfx::Point& point) {
  AccountButtonIndexes::const_iterator match =
      current_profile_accounts_map_.find(source);
  DCHECK(match != current_profile_accounts_map_.end());

  MutableProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetPlatformSpecificForProfile(
          browser_->profile());
  if (oauth2_token_service)
    oauth2_token_service->RevokeCredentials(match->second);
}

void ProfileChooserView::LinkClicked(views::Link* sender, int event_flags) {
  if (sender == manage_accounts_link_) {
    // ShowView() will DCHECK if this view is displayed for non signed-in users.
    ShowView(ACCOUNT_MANAGEMENT_VIEW, avatar_menu_.get());
  } else if (sender == signout_current_profile_link_) {
    profiles::LockProfile(browser_->profile());
  } else if (sender == tutorial_learn_more_link_) {
    // TODO(guohui): update |learn_more_url| once it is decided.
    const GURL lear_more_url("https://support.google.com/chrome/?hl=en#to");
    chrome::NavigateParams params(
        browser_->profile(),
        lear_more_url,
        content::PAGE_TRANSITION_LINK);
    params.disposition = NEW_FOREGROUND_TAB;
    chrome::Navigate(&params);
  } else {
    DCHECK(sender == signin_current_profile_link_);
    ShowView(GAIA_SIGNIN_VIEW, avatar_menu_.get());
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

views::View* ProfileChooserView::CreateTutorialView(
    const AvatarMenu::Item& current_avatar_item, bool tutorial_shown) {
  if (!current_avatar_item.signed_in)
    return NULL;

  Profile* profile = browser_->profile();
  const int show_count = profile->GetPrefs()->GetInteger(
      prefs::kProfileAvatarTutorialShown);
  // Do not show the tutorial if user has dismissed it.
  if (show_count > kProfileAvatarTutorialShowMax)
    return NULL;

  if (!tutorial_shown) {
    if (show_count == kProfileAvatarTutorialShowMax)
      return NULL;
    profile->GetPrefs()->SetInteger(
        prefs::kProfileAvatarTutorialShown, show_count + 1);
  }
  tutorial_showing_ = true;

  views::View* view = new views::View();
  ui::NativeTheme* theme = GetNativeTheme();
  view->set_background(
      views::Background::CreateSolidBackground(theme->GetSystemColor(
          ui::NativeTheme::kColorId_DialogBackground)));

  views::GridLayout* layout = CreateSingleColumnLayout(view,
      kFixedMenuWidth - 2 * views::kButtonHEdgeMarginNew);
  layout->SetInsets(views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew);

  // Adds title.
  base::string16 name = profiles::GetAvatarNameForProfile(profile);
  views::Label* title_label = new views::Label(
      l10n_util::GetStringFUTF16(IDS_PROFILES_SIGNIN_TUTORIAL_TITLE, name));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  const SkColor kTitleTextColor = SkColorSetRGB(0x53, 0x8c, 0xea);
  title_label->SetEnabledColor(kTitleTextColor);
  layout->StartRow(1, 0);
  layout->AddView(title_label);

  // Adds body header.
  views::Label* content_header_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_PROFILES_SIGNIN_TUTORIAL_CONTENT_HEADER));
  content_header_label->SetMultiLine(true);
  content_header_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  const gfx::FontList& small_font_list =
      rb->GetFontList(ui::ResourceBundle::SmallFont);
  content_header_label->SetFontList(small_font_list);
  layout->StartRowWithPadding(1, 0, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(content_header_label);

  // Adds body content consisting of three bulleted lines.
  views::View* bullet_row = new views::View();
  views::GridLayout* bullet_layout = new views::GridLayout(bullet_row);
  views::ColumnSet* bullet_columns = bullet_layout->AddColumnSet(0);
  const int kTextHorizIndentation = 10;
  bullet_columns->AddPaddingColumn(0, kTextHorizIndentation);
  bullet_columns->AddColumn(views::GridLayout::LEADING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  bullet_row->SetLayoutManager(bullet_layout);

  views::Label* bullet_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_PROFILES_SIGNIN_TUTORIAL_CONTENT_TEXT));
  bullet_label->SetMultiLine(true);
  bullet_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  bullet_label->SetFontList(small_font_list);
  bullet_layout->StartRow(1, 0);
  bullet_layout->AddView(bullet_label);

  layout->StartRowWithPadding(1, 0, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(bullet_row);

  // Adds links and buttons at the bottom.
  views::View* button_row = new views::View();
  views::GridLayout* button_layout = new views::GridLayout(button_row);
  views::ColumnSet* button_columns = button_layout->AddColumnSet(0);
  button_columns->AddColumn(views::GridLayout::LEADING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  button_columns->AddPaddingColumn(
      1, views::kUnrelatedControlHorizontalSpacing);
  button_columns->AddColumn(views::GridLayout::TRAILING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  button_row->SetLayoutManager(button_layout);

  tutorial_learn_more_link_ = CreateLink(
      l10n_util::GetStringUTF16(IDS_LEARN_MORE), this);
  tutorial_learn_more_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  button_layout->StartRow(1, 0);
  button_layout->AddView(tutorial_learn_more_link_);

  tutorial_ok_button_ = new views::BlueButton(this, l10n_util::GetStringUTF16(
      IDS_PROFILES_SIGNIN_TUTORIAL_OK_BUTTON));
  tutorial_ok_button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button_layout->AddView(tutorial_ok_button_);

  layout->StartRowWithPadding(1, 0, 0, views::kUnrelatedControlVerticalSpacing);
  layout->AddView(button_row);

  return view;
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
  current_profile_name_ = new EditableProfileName(
      this, profiles::GetAvatarNameForProfile(browser_->profile()), !is_guest);
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
  current_profile_name_ = new EditableProfileName(
      this, profiles::GetAvatarNameForProfile(browser_->profile()), true);

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
  views::GridLayout* layout = CreateSingleColumnLayout(
      view, kFixedMenuWidth - 2 * views::kButtonHEdgeMarginNew);
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

    views::LabelButton* button = new views::LabelButton(this, item.name);
    open_other_profile_indexes_map_[button] = index;
    button->SetImage(views::Button::STATE_NORMAL, *image.ToImageSkia());
    button->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
        ui::ResourceBundle::MediumFont));
    button->SetBorder(views::Border::NullBorder());

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
  views::GridLayout* layout = CreateSingleColumnLayout(view, kFixedMenuWidth);
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
  int column_width = kFixedMenuWidth - 2 * views::kButtonHEdgeMarginNew;
  views::GridLayout* layout = CreateSingleColumnLayout(view, column_width);
  layout->SetInsets(views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew);

  Profile* profile = browser_->profile();
  std::string primary_account =
      SigninManagerFactory::GetForProfile(profile)->GetAuthenticatedUsername();
  DCHECK(!primary_account.empty());
  std::vector<std::string>accounts =
      profiles::GetSecondaryAccountsForProfile(profile, primary_account);

  // The primary account should always be listed first.
  // TODO(rogerta): we still need to further differentiate the primary account
  // from the others in the UI, so more work is likely required here:
  // crbug.com/311124.
  CreateAccountButton(layout, primary_account, true, column_width);
  for (size_t i = 0; i < accounts.size(); ++i)
    CreateAccountButton(layout, accounts[i], false, column_width);
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
                                             bool is_primary_account,
                                             int width) {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia* menu_marker =
      rb->GetImageNamed(IDR_CLOSE_1).ToImageSkia();
  // Use a MenuButtonListener and not a regular ButtonListener to be
  // able to distinguish between the unnamed "other profile" buttons and the
  // unnamed "multiple accounts" buttons.
  views::MenuButton* email_button = new views::MenuButton(
      NULL,
      gfx::ElideEmail(base::UTF8ToUTF16(account),
                      rb->GetFontList(ui::ResourceBundle::BaseFont),
                      width - menu_marker->width()),
      is_primary_account ? NULL : this,  // Cannot delete the primary account.
      !is_primary_account);
  email_button->SetBorder(views::Border::CreateEmptyBorder(0, 0, 0, 0));
  if (!is_primary_account) {
    email_button->set_menu_marker(menu_marker);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }
  layout->StartRow(1, 0);
  layout->AddView(email_button);

  // Save the original email address, as the button text could be elided.
  current_profile_accounts_map_[email_button] = account;
}
