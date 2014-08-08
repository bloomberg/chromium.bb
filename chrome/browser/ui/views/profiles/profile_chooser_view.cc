// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/profiles/user_manager_view.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/mutable_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

// Helpers --------------------------------------------------------------------

const int kFixedMenuWidth = 250;
const int kButtonHeight = 32;
const int kFixedGaiaViewHeight = 400;
const int kFixedGaiaViewWidth = 360;
const int kFixedAccountRemovalViewWidth = 280;
const int kFixedSwitchUserViewWidth = 280;
const int kLargeImageSide = 88;

// The maximum number of times to show the welcome tutorial for an upgrade user.
const int kUpgradeWelcomeTutorialShowMax = 1;

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

views::Link* CreateLink(const base::string16& link_text,
                        views::LinkListener* listener) {
  views::Link* link_button = new views::Link(link_text);
  link_button->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  link_button->SetUnderline(false);
  link_button->set_listener(listener);
  return link_button;
}

gfx::ImageSkia CreateSquarePlaceholderImage(int size) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeA8(size, size));
  bitmap.eraseARGB(0, 0, 0, 0);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

bool HasAuthError(Profile* profile) {
  const SigninErrorController* error =
      profiles::GetSigninErrorController(profile);
  return error && error->HasError();
}

std::string GetAuthErrorAccountId(Profile* profile) {
  const SigninErrorController* error =
      profiles::GetSigninErrorController(profile);
  if (!error)
    return std::string();

  return error->error_account_id();
}

std::string GetAuthErrorUsername(Profile* profile) {
  const SigninErrorController* error =
      profiles::GetSigninErrorController(profile);
  if (!error)
    return std::string();

  return error->error_username();
}

// BackgroundColorHoverButton -------------------------------------------------

// A custom button that allows for setting a background color when hovered over.
class BackgroundColorHoverButton : public views::LabelButton {
 public:
  BackgroundColorHoverButton(views::ButtonListener* listener,
                             const base::string16& text,
                             const gfx::ImageSkia& icon)
      : views::LabelButton(listener, text) {
    SetBorder(views::Border::CreateEmptyBorder(
        0, views::kButtonHEdgeMarginNew, 0, views::kButtonHEdgeMarginNew));
    SetMinSize(gfx::Size(0,
        kButtonHeight + views::kRelatedControlVerticalSpacing));
    SetImage(STATE_NORMAL, icon);
  }

  virtual ~BackgroundColorHoverButton() {}

 private:
  // views::LabelButton:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    if ((state() == STATE_PRESSED) ||
        (state() == STATE_HOVERED) ||
        HasFocus()) {
      canvas->DrawColor(GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_ButtonHoverBackgroundColor));
    }
    LabelButton::OnPaint(canvas);
  }

  DISALLOW_COPY_AND_ASSIGN(BackgroundColorHoverButton);
};

// SizedContainer -------------------------------------------------

// A simple container view that takes an explicit preferred size.
class SizedContainer : public views::View {
 public:
  explicit SizedContainer(const gfx::Size& preferred_size)
      : preferred_size_(preferred_size) {}

  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return preferred_size_;
  }

 private:
  gfx::Size preferred_size_;
};

}  // namespace

// RightAlignedIconLabelButton -------------------------------------------------

// A custom LabelButton that has a centered text and right aligned icon.
class RightAlignedIconLabelButton : public views::LabelButton {
 public:
  RightAlignedIconLabelButton(views::ButtonListener* listener,
                              const base::string16& text)
      : views::LabelButton(listener, text) {
  }

 protected:
  virtual void Layout() OVERRIDE {
    // This layout trick keeps the text left-aligned and the icon right-aligned.
    SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    views::LabelButton::Layout();
    label()->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  }

  DISALLOW_COPY_AND_ASSIGN(RightAlignedIconLabelButton);
};

// EditableProfilePhoto -------------------------------------------------

// A custom Image control that shows a "change" button when moused over.
class EditableProfilePhoto : public views::ImageView {
 public:
  EditableProfilePhoto(views::ButtonListener* listener,
                       const gfx::Image& icon,
                       bool is_editing_allowed,
                       const gfx::Rect& bounds)
      : views::ImageView(),
        change_photo_button_(NULL) {
    gfx::Image image = profiles::GetSizedAvatarIcon(
        icon, true, kLargeImageSide, kLargeImageSide);
    SetImage(image.ToImageSkia());
    SetBoundsRect(bounds);

    // Calculate the circular mask that will be used to display the photo.
    circular_mask_.addCircle(SkIntToScalar(bounds.width() / 2),
                             SkIntToScalar(bounds.height() / 2),
                             SkIntToScalar(bounds.width() / 2));

    if (!is_editing_allowed)
      return;

    set_notify_enter_exit_on_child(true);

    // Button overlay that appears when hovering over the image.
    change_photo_button_ = new views::LabelButton(listener, base::string16());
    change_photo_button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    change_photo_button_->SetBorder(views::Border::NullBorder());

    const SkColor kBackgroundColor = SkColorSetARGB(65, 255, 255, 255);
    change_photo_button_->set_background(
        views::Background::CreateSolidBackground(kBackgroundColor));
    change_photo_button_->SetImage(views::LabelButton::STATE_NORMAL,
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_ICON_PROFILES_EDIT_CAMERA));

    change_photo_button_->SetSize(bounds.size());
    change_photo_button_->SetVisible(false);
    AddChildView(change_photo_button_);
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    // Display the profile picture as a circle.
    canvas->ClipPath(circular_mask_, true);
    views::ImageView::OnPaint(canvas);
  }

  virtual void PaintChildren(gfx::Canvas* canvas,
                             const views::CullSet& cull_set) OVERRIDE {
    // Display any children (the "change photo" overlay) as a circle.
    canvas->ClipPath(circular_mask_, true);
    View::PaintChildren(canvas, cull_set);
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

  gfx::Path circular_mask_;

  // Button that is shown when hovering over the image view. Can be NULL if
  // the photo isn't allowed to be edited (e.g. for guest profiles).
  views::LabelButton* change_photo_button_;

  DISALLOW_COPY_AND_ASSIGN(EditableProfilePhoto);
};

// EditableProfileName -------------------------------------------------

// A custom text control that turns into a textfield for editing when clicked.
class EditableProfileName : public RightAlignedIconLabelButton,
                            public views::ButtonListener {
 public:
  EditableProfileName(views::TextfieldController* controller,
                      const base::string16& text,
                      bool is_editing_allowed)
      : RightAlignedIconLabelButton(this, text),
        profile_name_textfield_(NULL) {
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    const gfx::FontList& medium_font_list =
        rb->GetFontList(ui::ResourceBundle::MediumFont);
    SetFontList(medium_font_list);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);

    if (!is_editing_allowed) {
      SetBorder(views::Border::CreateEmptyBorder(2, 0, 2, 0));
      return;
    }

    // Show an "edit" pencil icon when hovering over. In the default state,
    // we need to create an empty placeholder of the correct size, so that
    // the text doesn't jump around when the hovered icon appears.
    gfx::ImageSkia hover_image =
        *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_EDIT_HOVER);
    SetImage(STATE_NORMAL, CreateSquarePlaceholderImage(hover_image.width()));
    SetImage(STATE_HOVERED, hover_image);
    SetImage(STATE_PRESSED,
             *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_EDIT_PRESSED));
    // To center the text, we need to offest it by the width of the icon we
    // are adding and its padding. We need to also add a small top/bottom
    // padding to account for the textfield's border.
    const int kIconTextLabelButtonSpacing = 5;
    SetBorder(views::Border::CreateEmptyBorder(
        2, hover_image.width() + kIconTextLabelButtonSpacing, 2, 0));

    // Textfield that overlaps the button.
    profile_name_textfield_ = new views::Textfield();
    profile_name_textfield_->set_controller(controller);
    profile_name_textfield_->SetFontList(medium_font_list);
    profile_name_textfield_->SetHorizontalAlignment(gfx::ALIGN_CENTER);

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
    RightAlignedIconLabelButton::Layout();
  }

  // Textfield that is shown when editing the profile name. Can be NULL if
  // the profile name isn't allowed to be edited (e.g. for guest profiles).
  views::Textfield* profile_name_textfield_;

  DISALLOW_COPY_AND_ASSIGN(EditableProfileName);
};

// A title card with one back button right aligned and one label center aligned.
class TitleCard : public views::View {
 public:
  TitleCard(const base::string16& message, views::ButtonListener* listener,
            views::ImageButton** back_button) {
    back_button_ = new views::ImageButton(listener);
    back_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                    views::ImageButton::ALIGN_MIDDLE);
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    back_button_->SetImage(views::ImageButton::STATE_NORMAL,
                           rb->GetImageSkiaNamed(IDR_BACK));
    back_button_->SetImage(views::ImageButton::STATE_HOVERED,
                           rb->GetImageSkiaNamed(IDR_BACK_H));
    back_button_->SetImage(views::ImageButton::STATE_PRESSED,
                           rb->GetImageSkiaNamed(IDR_BACK_P));
    back_button_->SetImage(views::ImageButton::STATE_DISABLED,
                           rb->GetImageSkiaNamed(IDR_BACK_D));
    *back_button = back_button_;

    title_label_ = new views::Label(message);
    title_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    const gfx::FontList& medium_font_list =
        rb->GetFontList(ui::ResourceBundle::MediumFont);
    title_label_->SetFontList(medium_font_list);

    AddChildView(back_button_);
    AddChildView(title_label_);
  }

  // Creates a new view that has the |title_card| with padding at the top, an
  // edge-to-edge separator below, and the specified |view| at the bottom.
  static views::View* AddPaddedTitleCard(views::View* view,
                                         TitleCard* title_card,
                                         int width) {
    views::View* titled_view = new views::View();
    views::GridLayout* layout = new views::GridLayout(titled_view);
    titled_view->SetLayoutManager(layout);

    // Column set 0 is a single column layout with horizontal padding at left
    // and right, and column set 1 is a single column layout with no padding.
    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddPaddingColumn(1, views::kButtonHEdgeMarginNew);
    int available_width = width - 2 * views::kButtonHEdgeMarginNew;
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
        views::GridLayout::FIXED, available_width, available_width);
    columns->AddPaddingColumn(1, views::kButtonHEdgeMarginNew);
    layout->AddColumnSet(1)->AddColumn(views::GridLayout::FILL,
        views::GridLayout::FILL, 0,views::GridLayout::FIXED, width, width);

    layout->StartRowWithPadding(1, 0, 0, views::kButtonVEdgeMarginNew);
    layout->AddView(title_card);
    layout->StartRowWithPadding(1, 1, 0, views::kRelatedControlVerticalSpacing);
    layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

    layout->StartRow(1, 1);
    layout->AddView(view);

    return titled_view;
  }

 private:
  virtual void Layout() OVERRIDE{
    back_button_->SetBounds(
        0, 0, back_button_->GetPreferredSize().width(), height());
    title_label_->SetBoundsRect(GetContentsBounds());
  }

  virtual gfx::Size GetPreferredSize() const OVERRIDE{
    int height = std::max(title_label_->GetPreferredSize().height(),
        back_button_->GetPreferredSize().height());
    return gfx::Size(width(), height);
  }

  views::ImageButton* back_button_;
  views::Label* title_label_;

  DISALLOW_COPY_AND_ASSIGN(TitleCard);
};

// ProfileChooserView ---------------------------------------------------------

// static
ProfileChooserView* ProfileChooserView::profile_bubble_ = NULL;
bool ProfileChooserView::close_on_deactivate_for_testing_ = true;

// static
void ProfileChooserView::ShowBubble(
    profiles::BubbleViewMode view_mode,
    const signin::ManageAccountsParams& manage_accounts_params,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    views::BubbleBorder::BubbleAlignment border_alignment,
    Browser* browser) {
  if (IsShowing())
    return;

  profile_bubble_ = new ProfileChooserView(anchor_view, arrow, browser,
      view_mode, manage_accounts_params.service_type);
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
                                       Browser* browser,
                                       profiles::BubbleViewMode view_mode,
                                       signin::GAIAServiceType service_type)
    : BubbleDelegateView(anchor_view, arrow),
      browser_(browser),
      view_mode_(view_mode),
      tutorial_mode_(profiles::TUTORIAL_MODE_NONE),
      gaia_service_type_(service_type) {
  // Reset the default margins inherited from the BubbleDelegateView.
  // Add a small bottom inset so that the bubble's rounded corners show up.
  set_margins(gfx::Insets(0, 0, 1, 0));
  set_background(views::Background::CreateSolidBackground(
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_DialogBackground)));
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
  open_other_profile_indexes_map_.clear();
  delete_account_button_map_.clear();
  reauth_account_button_map_.clear();
  manage_accounts_link_ = NULL;
  signin_current_profile_link_ = NULL;
  auth_error_email_button_ = NULL;
  current_profile_photo_ = NULL;
  current_profile_name_ = NULL;
  users_button_ = NULL;
  go_incognito_button_ = NULL;
  lock_button_ = NULL;
  add_account_link_ = NULL;
  gaia_signin_cancel_button_ = NULL;
  remove_account_button_ = NULL;
  account_removal_cancel_button_ = NULL;
  add_person_button_ = NULL;
  disconnect_button_ = NULL;
  switch_user_cancel_button_ = NULL;
  tutorial_sync_settings_ok_button_ = NULL;
  tutorial_sync_settings_link_ = NULL;
  tutorial_see_whats_new_button_ = NULL;
  tutorial_not_you_link_ = NULL;
  tutorial_mode_ = profiles::TUTORIAL_MODE_NONE;
}

void ProfileChooserView::Init() {
  // If view mode is PROFILE_CHOOSER but there is an auth error, force
  // ACCOUNT_MANAGEMENT mode.
  if (view_mode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER &&
      HasAuthError(browser_->profile()) &&
      switches::IsEnableAccountConsistency() &&
      avatar_menu_->GetItemAt(avatar_menu_->GetActiveProfileIndex()).
          signed_in) {
    view_mode_ = profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT;
  }

  ShowView(view_mode_, avatar_menu_.get());
}

void ProfileChooserView::OnAvatarMenuChanged(
    AvatarMenu* avatar_menu) {
  // Do not refresh the avatar menu if the user is on a signin related view.
  if (view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN ||
      view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
      view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH) {
    return;
  }

  // Refresh the view with the new menu. We can't just update the local copy
  // as this may have been triggered by a sign out action, in which case
  // the view is being destroyed.
  ShowView(profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER, avatar_menu);
}

void ProfileChooserView::OnRefreshTokenAvailable(
    const std::string& account_id) {
  if (view_mode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT ||
      view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN ||
      view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
      view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH) {
    if (view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN)
      tutorial_mode_ = profiles::TUTORIAL_MODE_CONFIRM_SIGNIN;
    // The account management UI is only available through the
    // --enable-account-consistency flag.
    ShowView(switches::IsEnableAccountConsistency() ?
        profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT :
        profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER, avatar_menu_.get());
  }
}

void ProfileChooserView::OnRefreshTokenRevoked(const std::string& account_id) {
  // Refresh the account management view when an account is removed from the
  // profile.
  if (view_mode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT)
    ShowView(profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT, avatar_menu_.get());
}

void ProfileChooserView::ShowView(profiles::BubbleViewMode view_to_display,
                                  AvatarMenu* avatar_menu) {
  // The account management view should only be displayed if the active profile
  // is signed in.
  const AvatarMenu::Item& active_item = avatar_menu->GetItemAt(
      avatar_menu->GetActiveProfileIndex());
  if (view_to_display == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT) {
    DCHECK(switches::IsEnableAccountConsistency());
    DCHECK(active_item.signed_in);
  }

  if (browser_->profile()->IsSupervised() &&
      (view_to_display == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
       view_to_display == profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL)) {
    LOG(WARNING) << "Supervised user attempted to add/remove account";
    return;
  }

  // Records the last tutorial mode.
  profiles::TutorialMode last_tutorial_mode = tutorial_mode_;
  ResetView();
  RemoveAllChildViews(true);
  view_mode_ = view_to_display;

  views::GridLayout* layout;
  views::View* sub_view;
  switch (view_mode_) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH:
      layout = CreateSingleColumnLayout(this, kFixedGaiaViewWidth);
      sub_view = CreateGaiaSigninView();
      break;
    case profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL:
      layout = CreateSingleColumnLayout(this, kFixedAccountRemovalViewWidth);
      sub_view = CreateAccountRemovalView();
      break;
    case profiles::BUBBLE_VIEW_MODE_SWITCH_USER:
      layout = CreateSingleColumnLayout(this, kFixedSwitchUserViewWidth);
      sub_view = CreateSwitchUserView(active_item);
      break;
    default:
      layout = CreateSingleColumnLayout(this, kFixedMenuWidth);
      sub_view = CreateProfileChooserView(avatar_menu, last_tutorial_mode);
  }
  layout->StartRow(1, 0);
  layout->AddView(sub_view);
  Layout();
  if (GetBubbleFrameView())
    SizeToContents();
}

void ProfileChooserView::WindowClosing() {
  DCHECK_EQ(profile_bubble_, this);
  profile_bubble_ = NULL;

  if (tutorial_mode_ == profiles::TUTORIAL_MODE_CONFIRM_SIGNIN) {
    LoginUIServiceFactory::GetForProfile(browser_->profile())->
        SyncConfirmationUIClosed(false /* configure_sync_first */);
  }
}

void ProfileChooserView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  // Disable button after clicking so that it doesn't get clicked twice and
  // start a second action... which can crash Chrome.  But don't disable if it
  // has no parent (like in tests) because that will also crash.
  if (sender->parent())
    sender->SetEnabled(false);

  if (sender == users_button_) {
    profiles::ShowUserManagerMaybeWithTutorial(browser_->profile());
    // If this is a guest session, also close all the guest browser windows.
    if (browser_->profile()->IsGuestSession())
      profiles::CloseGuestProfileWindows();
  } else if (sender == go_incognito_button_) {
    DCHECK(ShouldShowGoIncognito());
    chrome::NewIncognitoWindow(browser_);
  } else if (sender == lock_button_) {
    profiles::LockProfile(browser_->profile());
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_LOCK);
  } else if (sender == auth_error_email_button_) {
    ShowView(profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH, avatar_menu_.get());
  } else if (sender == tutorial_sync_settings_ok_button_) {
    LoginUIServiceFactory::GetForProfile(browser_->profile())->
        SyncConfirmationUIClosed(false /* configure_sync_first */);
    tutorial_mode_ = profiles::TUTORIAL_MODE_NONE;
    ShowView(profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER, avatar_menu_.get());
  } else if (sender == tutorial_see_whats_new_button_) {
    chrome::ShowUserManagerWithTutorial(
        profiles::USER_MANAGER_TUTORIAL_OVERVIEW);
  } else if (sender == remove_account_button_) {
    RemoveAccount();
  } else if (sender == account_removal_cancel_button_) {
    account_id_to_remove_.clear();
    ShowView(profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT, avatar_menu_.get());
  } else if (sender == gaia_signin_cancel_button_) {
    std::string primary_account =
        SigninManagerFactory::GetForProfile(browser_->profile())->
        GetAuthenticatedUsername();
    // The account management view is only available with the
    // --enable-account-consistency flag.
    bool account_management_available = !primary_account.empty() &&
        switches::IsEnableAccountConsistency();
    ShowView(account_management_available ?
        profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT :
        profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER, avatar_menu_.get());
  } else if (current_profile_photo_ &&
             sender == current_profile_photo_->change_photo_button()) {
    avatar_menu_->EditProfile(avatar_menu_->GetActiveProfileIndex());
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_IMAGE);
  } else if (sender == signin_current_profile_link_) {
    ShowView(profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN, avatar_menu_.get());
  } else if (sender == add_person_button_) {
    profiles::ShowUserManagerMaybeWithTutorial(browser_->profile());
  } else if (sender == disconnect_button_) {
    chrome::ShowSettings(browser_);
  } else if (sender == switch_user_cancel_button_) {
    ShowView(profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER, avatar_menu_.get());
  } else {
    // Either one of the "other profiles", or one of the profile accounts
    // buttons was pressed.
    ButtonIndexes::const_iterator profile_match =
        open_other_profile_indexes_map_.find(sender);
    if (profile_match != open_other_profile_indexes_map_.end()) {
      avatar_menu_->SwitchToProfile(
          profile_match->second,
          ui::DispositionFromEventFlags(event.flags()) == NEW_WINDOW,
          ProfileMetrics::SWITCH_PROFILE_ICON);
    } else {
      // This was a profile accounts button.
      AccountButtonIndexes::const_iterator account_match =
          delete_account_button_map_.find(sender);
      if (account_match != delete_account_button_map_.end()) {
        account_id_to_remove_ = account_match->second;
        ShowView(profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL,
            avatar_menu_.get());
      } else {
        account_match = reauth_account_button_map_.find(sender);
        DCHECK(account_match != reauth_account_button_map_.end());
        ShowView(profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH, avatar_menu_.get());
      }
    }
  }
}

void ProfileChooserView::RemoveAccount() {
  DCHECK(!account_id_to_remove_.empty());
  MutableProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetPlatformSpecificForProfile(
      browser_->profile());
  if (oauth2_token_service) {
    oauth2_token_service->RevokeCredentials(account_id_to_remove_);
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_REMOVE_ACCT);
  }
  account_id_to_remove_.clear();

  ShowView(profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT, avatar_menu_.get());
}

void ProfileChooserView::LinkClicked(views::Link* sender, int event_flags) {
  if (sender == manage_accounts_link_) {
    // This link can either mean show/hide the account management view,
    // depending on which view it is displayed. ShowView() will DCHECK if
    // the account management view is displayed for non signed-in users.
    ShowView(
        view_mode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT ?
            profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER :
            profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT,
        avatar_menu_.get());
  } else if (sender == add_account_link_) {
    ShowView(profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT, avatar_menu_.get());
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_ADD_ACCT);
  } else if (sender == tutorial_sync_settings_link_) {
    LoginUIServiceFactory::GetForProfile(browser_->profile())->
        SyncConfirmationUIClosed(true /* configure_sync_first */);
    tutorial_mode_ = profiles::TUTORIAL_MODE_NONE;
  } else {
    DCHECK(sender == tutorial_not_you_link_);
    ShowView(profiles::BUBBLE_VIEW_MODE_SWITCH_USER, avatar_menu_.get());
  }
}

void ProfileChooserView::StyledLabelLinkClicked(
    const gfx::Range& range, int event_flags) {
  chrome::ShowSettings(browser_);
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

    if (profile->IsSupervised())
      return true;

    profiles::UpdateProfileName(profile, new_profile_name);
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_NAME);
    current_profile_name_->ShowReadOnlyView();
    return true;
  }
  return false;
}

views::View* ProfileChooserView::CreateProfileChooserView(
    AvatarMenu* avatar_menu,
    profiles::TutorialMode last_tutorial_mode) {
  // TODO(guohui, noms): the view should be customized based on whether new
  // profile management preview is enabled or not.

  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view, kFixedMenuWidth);
  // Separate items into active and alternatives.
  Indexes other_profiles;
  views::View* tutorial_view = NULL;
  views::View* current_profile_view = NULL;
  views::View* current_profile_accounts = NULL;
  views::View* option_buttons_view = NULL;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(i);
    if (item.active) {
      option_buttons_view = CreateOptionsView(
          switches::IsNewProfileManagement() && item.signed_in);
      current_profile_view = CreateCurrentProfileView(item, false);
      if (view_mode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER) {
        switch (last_tutorial_mode) {
          case profiles::TUTORIAL_MODE_NONE:
          case profiles::TUTORIAL_MODE_WELCOME_UPGRADE:
            tutorial_view = CreateWelcomeUpgradeTutorialViewIfNeeded(
                last_tutorial_mode == profiles::TUTORIAL_MODE_WELCOME_UPGRADE,
                item);
            break;
          case profiles::TUTORIAL_MODE_CONFIRM_SIGNIN:
            tutorial_view = CreateSigninConfirmationView();
            break;
          case profiles::TUTORIAL_MODE_SHOW_ERROR:
            // TODO(guohui): not implemented yet.
            NOTREACHED();
        }
      } else {
        current_profile_accounts = CreateCurrentProfileAccountsView(item);
      }
    } else {
      other_profiles.push_back(i);
    }
  }

  if (tutorial_view) {
    // TODO(mlerman): update UMA stats for the new tutorial.
    layout->StartRow(1, 0);
    layout->AddView(tutorial_view);
  }

  if (!current_profile_view) {
    // Guest windows don't have an active profile.
    current_profile_view = CreateGuestProfileView();
    option_buttons_view = CreateOptionsView(false);
  }

  layout->StartRow(1, 0);
  layout->AddView(current_profile_view);

  if (view_mode_ != profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER) {
    DCHECK(current_profile_accounts);
    layout->StartRow(0, 0);
    layout->AddView(new views::Separator(views::Separator::HORIZONTAL));
    layout->StartRow(1, 0);
    layout->AddView(current_profile_accounts);
  }

  if (browser_->profile()->IsSupervised()) {
    layout->StartRow(0, 0);
    layout->AddView(new views::Separator(views::Separator::HORIZONTAL));
    layout->StartRow(1, 0);
    layout->AddView(CreateSupervisedUserDisclaimerView());
  }

  if (view_mode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER) {
    layout->StartRow(1, 0);
    if (switches::IsFastUserSwitching())
      layout->AddView(CreateOtherProfilesView(other_profiles));
  }

  layout->StartRow(0, 0);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

  if (option_buttons_view) {
    layout->StartRow(0, 0);
    layout->AddView(option_buttons_view);
  }

  return view;
}

views::View* ProfileChooserView::CreateTutorialView(
    profiles::TutorialMode tutorial_mode,
    const base::string16& title_text,
    const base::string16& content_text,
    const base::string16& link_text,
    const base::string16& button_text,
    views::Link** link,
    views::LabelButton** button) {
  tutorial_mode_ = tutorial_mode;

  views::View* view = new views::View();
  view->set_background(views::Background::CreateSolidBackground(
      profiles::kAvatarTutorialBackgroundColor));
  views::GridLayout* layout = CreateSingleColumnLayout(view,
      kFixedMenuWidth - 2 * views::kButtonHEdgeMarginNew);
  layout->SetInsets(views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew);

  // Adds title.
  views::Label* title_label = new views::Label(title_text);
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetAutoColorReadabilityEnabled(false);
  title_label->SetEnabledColor(SK_ColorWHITE);
  title_label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));
  layout->StartRow(1, 0);
  layout->AddView(title_label);

  // Adds body content.
  views::Label* content_label = new views::Label(content_text);
  content_label->SetMultiLine(true);
  content_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  content_label->SetAutoColorReadabilityEnabled(false);
  content_label->SetEnabledColor(profiles::kAvatarTutorialContentTextColor);
  layout->StartRowWithPadding(1, 0, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(content_label);

  // Adds links and buttons.
  views::ColumnSet* button_columns = layout->AddColumnSet(1);
  button_columns->AddColumn(views::GridLayout::LEADING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  button_columns->AddPaddingColumn(
      1, views::kUnrelatedControlHorizontalSpacing);
  button_columns->AddColumn(views::GridLayout::TRAILING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);

  *link = CreateLink(link_text, this);
  (*link)->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  (*link)->SetAutoColorReadabilityEnabled(false);
  (*link)->SetEnabledColor(SK_ColorWHITE);
  layout->StartRowWithPadding(1, 1, 0, views::kUnrelatedControlVerticalSpacing);
  layout->AddView(*link);

  *button = new views::LabelButton(this, button_text);
  (*button)->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  (*button)->SetStyle(views::Button::STYLE_BUTTON);
  layout->AddView(*button);

  return view;
}

views::View* ProfileChooserView::CreateCurrentProfileView(
    const AvatarMenu::Item& avatar_item,
    bool is_guest) {
  views::View* view = new views::View();
  int column_width = kFixedMenuWidth - 2 * views::kButtonHEdgeMarginNew;
  views::GridLayout* layout = CreateSingleColumnLayout(view, column_width);
  layout->SetInsets(views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew,
                    views::kUnrelatedControlVerticalSpacing,
                    views::kButtonHEdgeMarginNew);

  // Profile icon, centered.
  int x_offset = (column_width - kLargeImageSide) / 2;
  current_profile_photo_ = new EditableProfilePhoto(
      this, avatar_item.icon, !is_guest,
      gfx::Rect(x_offset, 0, kLargeImageSide, kLargeImageSide));
  SizedContainer* profile_icon_container =
      new SizedContainer(gfx::Size(column_width, kLargeImageSide));
  profile_icon_container->AddChildView(current_profile_photo_);

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  if (browser_->profile()->IsSupervised()) {
    views::ImageView* supervised_icon = new views::ImageView();
    supervised_icon->SetImage(
        rb->GetImageSkiaNamed(IDR_ICON_PROFILES_MENU_SUPERVISED));
    gfx::Size preferred_size = supervised_icon->GetPreferredSize();
    gfx::Rect parent_bounds = current_profile_photo_->bounds();
    supervised_icon->SetBounds(
        parent_bounds.right() - preferred_size.width(),
        parent_bounds.bottom() - preferred_size.height(),
        preferred_size.width(),
        preferred_size.height());
    profile_icon_container->AddChildView(supervised_icon);
  }

  layout->StartRow(1, 0);
  layout->AddView(profile_icon_container);

  // Profile name, centered.
  bool editing_allowed = !is_guest && !browser_->profile()->IsSupervised();
  current_profile_name_ = new EditableProfileName(
      this,
      profiles::GetAvatarNameForProfile(browser_->profile()->GetPath()),
      editing_allowed);
  layout->StartRowWithPadding(1, 0, 0,
                              views::kRelatedControlSmallVerticalSpacing);
  layout->StartRow(1, 0);
  layout->AddView(current_profile_name_);

  if (is_guest)
    return view;

  // The available links depend on the type of profile that is active.
  if (avatar_item.signed_in) {
    layout->StartRow(1, 0);
    if (switches::IsEnableAccountConsistency()) {
      base::string16 link_title = l10n_util::GetStringUTF16(
          view_mode_ == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER ?
              IDS_PROFILES_PROFILE_MANAGE_ACCOUNTS_BUTTON :
              IDS_PROFILES_PROFILE_HIDE_MANAGE_ACCOUNTS_BUTTON);
      manage_accounts_link_ = CreateLink(link_title, this);
      manage_accounts_link_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
      layout->AddView(manage_accounts_link_);
    } else {
      // Badge the email address if there's an authentication error.
      if (HasAuthError(browser_->profile())) {
        const gfx::ImageSkia warning_image = *rb->GetImageNamed(
            IDR_ICON_PROFILES_ACCOUNT_BUTTON_ERROR).ToImageSkia();
        auth_error_email_button_ =
            new RightAlignedIconLabelButton(this, avatar_item.sync_state);
        auth_error_email_button_->SetElideBehavior(gfx::ELIDE_EMAIL);
        auth_error_email_button_->SetBorder(views::Border::NullBorder());
        auth_error_email_button_->SetImage(
            views::LabelButton::STATE_NORMAL, warning_image);
        auth_error_email_button_->SetTextColor(
            views::LabelButton::STATE_NORMAL,
            views::Link::GetDefaultEnabledColor());
        layout->AddView(auth_error_email_button_);
      } else {
        views::Label* email_label = new views::Label(avatar_item.sync_state);
        email_label->SetElideBehavior(gfx::ELIDE_EMAIL);
        layout->AddView(email_label);
      }
    }
  } else {
    SigninManagerBase* signin_manager = SigninManagerFactory::GetForProfile(
        browser_->profile()->GetOriginalProfile());
    if (signin_manager->IsSigninAllowed()) {
      views::Label* promo = new views::Label(
          l10n_util::GetStringUTF16(IDS_PROFILES_SIGNIN_PROMO));
      promo->SetMultiLine(true);
      promo->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      layout->StartRowWithPadding(1, 0, 0,
                                  views::kRelatedControlSmallVerticalSpacing);
      layout->StartRow(1, 0);
      layout->AddView(promo);

      signin_current_profile_link_ = new views::BlueButton(
        this, l10n_util::GetStringFUTF16(IDS_SYNC_START_SYNC_BUTTON_LABEL,
            l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
      layout->StartRowWithPadding(1, 0, 0,
                                  views::kRelatedControlVerticalSpacing);
      layout->StartRow(1, 0);
      layout->AddView(signin_current_profile_link_);
    }
  }

  return view;
}

views::View* ProfileChooserView::CreateGuestProfileView() {
  gfx::Image guest_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
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
  views::GridLayout* layout = CreateSingleColumnLayout(view, kFixedMenuWidth);

  int num_avatars_to_show = avatars_to_show.size();
  for (int i = 0; i < num_avatars_to_show; ++i) {
    const size_t index = avatars_to_show[i];
    const AvatarMenu::Item& item = avatar_menu_->GetItemAt(index);
    const int kSmallImageSide = 32;

    gfx::Image image = profiles::GetSizedAvatarIcon(
        item.icon, true, kSmallImageSide, kSmallImageSide);

    views::LabelButton* button = new BackgroundColorHoverButton(
        this,
        item.name,
        *image.ToImageSkia());
    open_other_profile_indexes_map_[button] = index;

    layout->StartRow(1, 0);
    layout->AddView(new views::Separator(views::Separator::HORIZONTAL));
    layout->StartRow(1, 0);
    layout->AddView(button);
  }

  return view;
}

views::View* ProfileChooserView::CreateOptionsView(bool enable_lock) {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view, kFixedMenuWidth);

  base::string16 text = browser_->profile()->IsGuestSession() ?
      l10n_util::GetStringUTF16(IDS_PROFILES_EXIT_GUEST) :
      l10n_util::GetStringUTF16(IDS_PROFILES_SWITCH_USERS_BUTTON);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  users_button_ = new BackgroundColorHoverButton(
      this,
      text,
      *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_MENU_AVATAR));
  layout->StartRow(1, 0);
  layout->AddView(users_button_);

  if (ShouldShowGoIncognito()) {
    layout->StartRow(1, 0);
    layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

    // TODO(noms): Use the correct incognito icon when it's available.
    go_incognito_button_ = new BackgroundColorHoverButton(
        this,
        l10n_util::GetStringUTF16(IDS_PROFILES_GO_INCOGNITO_BUTTON),
        *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_MENU_AVATAR));
    layout->StartRow(1, 0);
    layout->AddView(go_incognito_button_);
  }

  if (enable_lock) {
    layout->StartRow(1, 0);
    layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

    lock_button_ = new BackgroundColorHoverButton(
        this,
        l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON),
        *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_MENU_LOCK));
    layout->StartRow(1, 0);
    layout->AddView(lock_button_);
  }
  return view;
}

views::View* ProfileChooserView::CreateSupervisedUserDisclaimerView() {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(
      view, kFixedMenuWidth - 2 * views::kButtonHEdgeMarginNew);
  layout->SetInsets(views::kRelatedControlVerticalSpacing,
                    views::kButtonHEdgeMarginNew,
                    views::kRelatedControlVerticalSpacing,
                    views::kButtonHEdgeMarginNew);
  views::Label* disclaimer = new views::Label(
      avatar_menu_->GetSupervisedUserInformation());
  disclaimer->SetMultiLine(true);
  disclaimer->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  disclaimer->SetFontList(rb->GetFontList(ui::ResourceBundle::SmallFont));
  layout->StartRow(1, 0);
  layout->AddView(disclaimer);

  return view;
}

views::View* ProfileChooserView::CreateCurrentProfileAccountsView(
    const AvatarMenu::Item& avatar_item) {
  DCHECK(avatar_item.signed_in);
  views::View* view = new views::View();
  view->set_background(views::Background::CreateSolidBackground(
      profiles::kAvatarBubbleAccountsBackgroundColor));
  views::GridLayout* layout = CreateSingleColumnLayout(view, kFixedMenuWidth);

  Profile* profile = browser_->profile();
  std::string primary_account =
      SigninManagerFactory::GetForProfile(profile)->GetAuthenticatedUsername();
  DCHECK(!primary_account.empty());
  std::vector<std::string>accounts =
      profiles::GetSecondaryAccountsForProfile(profile, primary_account);

  // Get state of authentication error, if any.
  std::string error_account_id = GetAuthErrorAccountId(profile);

  // The primary account should always be listed first.
  // TODO(rogerta): we still need to further differentiate the primary account
  // from the others in the UI, so more work is likely required here:
  // crbug.com/311124.
  CreateAccountButton(layout, primary_account, true,
                      error_account_id == primary_account, kFixedMenuWidth);
  for (size_t i = 0; i < accounts.size(); ++i)
    CreateAccountButton(layout, accounts[i], false,
                        error_account_id == accounts[i], kFixedMenuWidth);

  if (!profile->IsSupervised()) {
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    add_account_link_ = CreateLink(l10n_util::GetStringFUTF16(
        IDS_PROFILES_PROFILE_ADD_ACCOUNT_BUTTON, avatar_item.name), this);
    add_account_link_->SetBorder(views::Border::CreateEmptyBorder(
        0, views::kButtonVEdgeMarginNew,
        views::kRelatedControlVerticalSpacing, 0));
    layout->StartRow(1, 0);
    layout->AddView(add_account_link_);
  }

  return view;
}

void ProfileChooserView::CreateAccountButton(views::GridLayout* layout,
                                             const std::string& account,
                                             bool is_primary_account,
                                             bool reauth_required,
                                             int width) {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia* delete_default_image =
      rb->GetImageNamed(IDR_CLOSE_1).ToImageSkia();
  const int kDeleteButtonWidth = delete_default_image->width();
  const gfx::ImageSkia warning_default_image = reauth_required ?
      *rb->GetImageNamed(IDR_ICON_PROFILES_ACCOUNT_BUTTON_ERROR).ToImageSkia() :
      gfx::ImageSkia();
  const int kWarningButtonWidth = reauth_required ?
      warning_default_image.width() + views::kRelatedButtonHSpacing : 0;
  int available_width = width - 2 * views::kButtonHEdgeMarginNew
      - kDeleteButtonWidth - kWarningButtonWidth;
  views::LabelButton* email_button = new BackgroundColorHoverButton(
      reauth_required ? this : NULL,
      base::UTF8ToUTF16(account),
      warning_default_image);
  email_button->SetElideBehavior(gfx::ELIDE_EMAIL);
  email_button->SetMinSize(gfx::Size(0, kButtonHeight));
  email_button->SetMaxSize(gfx::Size(available_width, kButtonHeight));
  layout->StartRow(1, 0);
  layout->AddView(email_button);

  if (reauth_required)
    reauth_account_button_map_[email_button] = account;

  // Delete button.
  if (!browser_->profile()->IsSupervised()) {
    views::ImageButton* delete_button = new views::ImageButton(this);
    delete_button->SetImageAlignment(views::ImageButton::ALIGN_RIGHT,
                                     views::ImageButton::ALIGN_MIDDLE);
    delete_button->SetImage(views::ImageButton::STATE_NORMAL,
                            delete_default_image);
    delete_button->SetImage(views::ImageButton::STATE_HOVERED,
                            rb->GetImageSkiaNamed(IDR_CLOSE_1_H));
    delete_button->SetImage(views::ImageButton::STATE_PRESSED,
                            rb->GetImageSkiaNamed(IDR_CLOSE_1_P));
    delete_button->SetBounds(
        width - views::kButtonHEdgeMarginNew - kDeleteButtonWidth,
        0, kDeleteButtonWidth, kButtonHeight);

    email_button->set_notify_enter_exit_on_child(true);
    email_button->AddChildView(delete_button);

    // Save the original email address, as the button text could be elided.
    delete_account_button_map_[delete_button] = account;
  }
}

views::View* ProfileChooserView::CreateGaiaSigninView() {
  GURL url;
  int message_id;

  switch (view_mode_) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
      url = signin::GetPromoURL(signin::SOURCE_AVATAR_BUBBLE_SIGN_IN,
                                false /* auto_close */,
                                true /* is_constrained */);
      message_id = IDS_PROFILES_GAIA_SIGNIN_TITLE;
      break;
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
      url = signin::GetPromoURL(signin::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT,
                                false /* auto_close */,
                                true /* is_constrained */);
      message_id = IDS_PROFILES_GAIA_ADD_ACCOUNT_TITLE;
      break;
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH: {
      DCHECK(HasAuthError(browser_->profile()));
      url = signin::GetReauthURL(browser_->profile(),
                                 GetAuthErrorUsername(browser_->profile()));
      message_id = IDS_PROFILES_GAIA_REAUTH_TITLE;
      break;
    }
    default:
      NOTREACHED() << "Called with invalid mode=" << view_mode_;
      return NULL;
  }

  // Adds Gaia signin webview
  Profile* profile = browser_->profile();
  views::WebView* web_view = new views::WebView(profile);
  web_view->LoadInitialURL(url);
  web_view->SetPreferredSize(
      gfx::Size(kFixedGaiaViewWidth, kFixedGaiaViewHeight));

  TitleCard* title_card = new TitleCard(l10n_util::GetStringUTF16(message_id),
                                        this,
                                        &gaia_signin_cancel_button_);
  return TitleCard::AddPaddedTitleCard(
      web_view, title_card, kFixedGaiaViewWidth);
}

views::View* ProfileChooserView::CreateAccountRemovalView() {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(
      view, kFixedAccountRemovalViewWidth - 2 * views::kButtonHEdgeMarginNew);
  layout->SetInsets(0,
                    views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew);

  const std::string& primary_account = SigninManagerFactory::GetForProfile(
      browser_->profile())->GetAuthenticatedUsername();
  bool is_primary_account = primary_account == account_id_to_remove_;

  // Adds main text.
  layout->StartRowWithPadding(1, 0, 0, views::kUnrelatedControlVerticalSpacing);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  const gfx::FontList& small_font_list =
      rb->GetFontList(ui::ResourceBundle::SmallFont);

  if (is_primary_account) {
    std::vector<size_t> offsets;
    const base::string16 settings_text =
        l10n_util::GetStringUTF16(IDS_PROFILES_SETTINGS_LINK);
    const base::string16 primary_account_removal_text =
        l10n_util::GetStringFUTF16(IDS_PROFILES_PRIMARY_ACCOUNT_REMOVAL_TEXT,
            base::UTF8ToUTF16(account_id_to_remove_), settings_text, &offsets);
    views::StyledLabel* primary_account_removal_label =
        new views::StyledLabel(primary_account_removal_text, this);
    primary_account_removal_label->AddStyleRange(
        gfx::Range(offsets[1], offsets[1] + settings_text.size()),
        views::StyledLabel::RangeStyleInfo::CreateForLink());
    primary_account_removal_label->SetBaseFontList(small_font_list);
    layout->AddView(primary_account_removal_label);
  } else {
    views::Label* content_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_PROFILES_ACCOUNT_REMOVAL_TEXT));
    content_label->SetMultiLine(true);
    content_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    content_label->SetFontList(small_font_list);
    layout->AddView(content_label);
  }

  // Adds button.
  if (!is_primary_account) {
    remove_account_button_ = new views::BlueButton(
        this, l10n_util::GetStringUTF16(IDS_PROFILES_ACCOUNT_REMOVAL_BUTTON));
    remove_account_button_->SetHorizontalAlignment(
        gfx::ALIGN_CENTER);
    layout->StartRowWithPadding(
        1, 0, 0, views::kUnrelatedControlVerticalSpacing);
    layout->AddView(remove_account_button_);
  } else {
    layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
  }

  TitleCard* title_card = new TitleCard(
      l10n_util::GetStringUTF16(IDS_PROFILES_ACCOUNT_REMOVAL_TITLE),
      this, &account_removal_cancel_button_);
  return TitleCard::AddPaddedTitleCard(view, title_card,
      kFixedAccountRemovalViewWidth);
}

views::View* ProfileChooserView::CreateWelcomeUpgradeTutorialViewIfNeeded(
    bool tutorial_shown, const AvatarMenu::Item& avatar_item){
  if (first_run::IsChromeFirstRun())
    return NULL;

  Profile* profile = browser_->profile();
  if (!avatar_item.signed_in) {
    profile->GetPrefs()->SetInteger(
        prefs::kProfileAvatarTutorialShown, kUpgradeWelcomeTutorialShowMax + 1);
    return NULL;
  }

  const int show_count = profile->GetPrefs()->GetInteger(
      prefs::kProfileAvatarTutorialShown);
  // Do not show the tutorial if user has dismissed it.
  if (show_count > kUpgradeWelcomeTutorialShowMax)
    return NULL;

  if (!tutorial_shown) {
    if (show_count == kUpgradeWelcomeTutorialShowMax)
      return NULL;
    profile->GetPrefs()->SetInteger(
        prefs::kProfileAvatarTutorialShown, show_count + 1);
  }

  return CreateTutorialView(
      profiles::TUTORIAL_MODE_WELCOME_UPGRADE,
      l10n_util::GetStringFUTF16(
          IDS_PROFILES_WELCOME_UPGRADE_TUTORIAL_TITLE, avatar_item.name),
      l10n_util::GetStringUTF16(
          IDS_PROFILES_WELCOME_UPGRADE_TUTORIAL_CONTENT_TEXT),
      l10n_util::GetStringFUTF16(
          IDS_PROFILES_NOT_YOU, avatar_item.name),
      l10n_util::GetStringUTF16(IDS_PROFILES_TUTORIAL_WHATS_NEW_BUTTON),
      &tutorial_not_you_link_,
      &tutorial_see_whats_new_button_);
}

views::View* ProfileChooserView::CreateSigninConfirmationView(){
  return CreateTutorialView(
      profiles::TUTORIAL_MODE_CONFIRM_SIGNIN,
      l10n_util::GetStringUTF16(IDS_PROFILES_CONFIRM_SIGNIN_TUTORIAL_TITLE),
      l10n_util::GetStringUTF16(
          IDS_PROFILES_CONFIRM_SIGNIN_TUTORIAL_CONTENT_TEXT),
      l10n_util::GetStringUTF16(IDS_PROFILES_SYNC_SETTINGS_LINK),
      l10n_util::GetStringUTF16(IDS_PROFILES_TUTORIAL_OK_BUTTON),
      &tutorial_sync_settings_link_,
      &tutorial_sync_settings_ok_button_);
}

views::View* ProfileChooserView::CreateSwitchUserView(
    const AvatarMenu::Item& avatar_item) {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(
      view, kFixedSwitchUserViewWidth);
  views::ColumnSet* columns = layout->AddColumnSet(1);
  columns->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  int label_width =
      kFixedSwitchUserViewWidth - 2 * views::kButtonHEdgeMarginNew;
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                     views::GridLayout::FIXED, label_width, label_width);
  columns->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);

  // Adds main text.
  layout->StartRowWithPadding(1, 1, 0, views::kUnrelatedControlVerticalSpacing);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  const gfx::FontList& small_font_list =
      rb->GetFontList(ui::ResourceBundle::SmallFont);
  views::Label* content_label = new views::Label(
      l10n_util::GetStringFUTF16(
          IDS_PROFILES_NOT_YOU_CONTENT_TEXT, avatar_item.name));
  content_label->SetMultiLine(true);
  content_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  content_label->SetFontList(small_font_list);
  layout->AddView(content_label);

  // Adds "Add person" button.
  layout->StartRowWithPadding(1, 0, 0, views::kUnrelatedControlVerticalSpacing);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

  add_person_button_ = new BackgroundColorHoverButton(
      this,
      l10n_util::GetStringUTF16(IDS_PROFILES_ADD_PERSON_BUTTON),
      *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_MENU_AVATAR));
  layout->StartRow(1, 0);
  layout->AddView(add_person_button_);

  // Adds "Disconnect your Google Account" button.
  layout->StartRow(1, 0);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

  disconnect_button_ = new BackgroundColorHoverButton(
      this,
      l10n_util::GetStringUTF16(IDS_PROFILES_DISCONNECT_BUTTON),
      *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_MENU_AVATAR));
  layout->StartRow(1, 0);
  layout->AddView(disconnect_button_);

  TitleCard* title_card = new TitleCard(
      l10n_util::GetStringFUTF16(IDS_PROFILES_NOT_YOU, avatar_item.name),
      this, &switch_user_cancel_button_);
  return TitleCard::AddPaddedTitleCard(view, title_card,
      kFixedSwitchUserViewWidth);
}

bool ProfileChooserView::ShouldShowGoIncognito() const {
  bool incognito_available =
      IncognitoModePrefs::GetAvailability(browser_->profile()->GetPrefs()) !=
          IncognitoModePrefs::DISABLED;
  return incognito_available && !browser_->profile()->IsGuestSession();
}

void ProfileChooserView::PostActionPerformed(
    ProfileMetrics::ProfileDesktopMenu action_performed) {
  ProfileMetrics::LogProfileDesktopMenu(action_performed, gaia_service_type_);
  gaia_service_type_ = signin::GAIA_SERVICE_TYPE_NONE;
}
