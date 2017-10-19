// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/profiles/signin_view_controller_delegate_views.h"
#include "chrome/browser/ui/views/profiles/user_manager_view.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/sync/driver/sync_error_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/clip_recorder.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// Helpers --------------------------------------------------------------------

const int kButtonHeight = 32;
const int kFixedAccountRemovalViewWidth = 280;
const int kImageSide = 40;
const int kProfileBadgeSize = 24;
const int kFixedMenuWidth = 240;
const int kSupervisedIconBadgeSize = 22;

// The space between the right/bottom edge of the profile badge and the
// right/bottom edge of the profile icon.
const int kBadgeSpacing = 4;

// Spacing between the edge of the material design user menu and the
// top/bottom or left/right of the menu items.
const int kMenuEdgeMargin = 16;

const int kVerticalSpacing = 16;

bool IsProfileChooser(profiles::BubbleViewMode mode) {
  return mode == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
}

// DEPRECATED: New user menu components should use views::BoxLayout instead.
// Creates a GridLayout with a single column. This ensures that all the child
// views added get auto-expanded to fill the full width of the bubble.
views::GridLayout* CreateSingleColumnLayout(views::View* view, int width) {
  views::GridLayout* layout = views::GridLayout::CreateAndInstall(view);

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

bool HasAuthError(Profile* profile) {
  const SigninErrorController* error =
      SigninErrorControllerFactory::GetForProfile(profile);
  return error && error->HasError();
}

std::string GetAuthErrorAccountId(Profile* profile) {
  const SigninErrorController* error =
      SigninErrorControllerFactory::GetForProfile(profile);
  if (!error)
    return std::string();

  return error->error_account_id();
}

views::ImageButton* CreateBackButton(views::ButtonListener* listener) {
  views::ImageButton* back_button = new views::ImageButton(listener);
  back_button->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                 views::ImageButton::ALIGN_MIDDLE);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  back_button->SetImage(views::ImageButton::STATE_NORMAL,
                        rb->GetImageSkiaNamed(IDR_BACK));
  back_button->SetImage(views::ImageButton::STATE_HOVERED,
                        rb->GetImageSkiaNamed(IDR_BACK_H));
  back_button->SetImage(views::ImageButton::STATE_PRESSED,
                        rb->GetImageSkiaNamed(IDR_BACK_P));
  back_button->SetImage(views::ImageButton::STATE_DISABLED,
                        rb->GetImageSkiaNamed(IDR_BACK_D));
  back_button->SetFocusForPlatform();
  return back_button;
}

// BackgroundColorHoverButton -------------------------------------------------

// A custom button that allows for setting a background color when hovered over.
class BackgroundColorHoverButton : public views::LabelButton {
 public:
  BackgroundColorHoverButton(ProfileChooserView* profile_chooser_view,
                             const base::string16& text)
      : views::LabelButton(profile_chooser_view, text),
        title_(nullptr),
        subtitle_(nullptr) {
    DCHECK(profile_chooser_view);
    SetImageLabelSpacing(kMenuEdgeMargin - 2);
    SetBorder(views::CreateEmptyBorder(0, kMenuEdgeMargin, 0, kMenuEdgeMargin));
    SetFocusForPlatform();
    SetFocusPainter(nullptr);

    label()->SetHandlesTooltips(false);
  }

  BackgroundColorHoverButton(ProfileChooserView* profile_chooser_view,
                             const base::string16& text,
                             const gfx::ImageSkia& icon)
      : BackgroundColorHoverButton(profile_chooser_view, text) {
    SetMinSize(gfx::Size(
        icon.width(),
        kButtonHeight + ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_VERTICAL)));
    SetImage(STATE_NORMAL, icon);
  }

  // Overrides the main label associated with this button.  If unset,
  // label() will be used instead.  |label| should be drawn over this
  // button, but it is not necessary that it be a child view.
  void set_title(views::Label* label) { title_ = label; }

  // Sets a secondary label associated with this button.  |label|
  // should be drawn over this button, but it is not necessary that it
  // be a child view.
  void set_subtitle(views::Label* label) { subtitle_ = label; }

  ~BackgroundColorHoverButton() override {}

 private:
  // views::View:
  void OnFocus() override {
    LabelButton::OnFocus();
    UpdateColors();
  }

  void OnBlur() override {
    LabelButton::OnBlur();
    UpdateColors();
  }

  void OnNativeThemeChanged(const ui::NativeTheme* theme) override {
    LabelButton::OnNativeThemeChanged(theme);
    UpdateColors();
  }

  // views::Button:
  void StateChanged(ButtonState old_state) override {
    LabelButton::StateChanged(old_state);

    // As in a menu, focus follows the mouse (including blurring when the mouse
    // leaves the button). If we don't do this, the focused view and the hovered
    // view might both have the selection highlight.
    if (state() == STATE_HOVERED || state() == STATE_PRESSED)
      RequestFocus();
    else if (state() == STATE_NORMAL && HasFocus())
      GetFocusManager()->SetFocusedView(nullptr);

    UpdateColors();
  }

  void UpdateColors() {
    // TODO(tapted): This should use views::style::GetColor() to obtain grey
    // text where it's needed. But note |subtitle_| is used to draw an email,
    // which might not be grey in Harmony.
    bool is_selected = HasFocus();

    SetBackground(
        is_selected
            ? views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
                  ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor))
            : nullptr);

    SkColor text_color = GetNativeTheme()->GetSystemColor(
        is_selected ? ui::NativeTheme::kColorId_SelectedMenuItemForegroundColor
                    : ui::NativeTheme::kColorId_LabelEnabledColor);
    SetEnabledTextColors(text_color);
    if (title_)
      title_->SetEnabledColor(text_color);

    if (subtitle_) {
      subtitle_->SetEnabledColor(GetNativeTheme()->GetSystemColor(
          is_selected
              ? ui::NativeTheme::kColorId_DisabledMenuItemForegroundColor
              : ui::NativeTheme::kColorId_LabelDisabledColor));
    }
  }

  views::Label* title_;
  views::Label* subtitle_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundColorHoverButton);
};

}  // namespace

// EditableProfilePhoto -------------------------------------------------

// A custom Image control that shows a "change" button when moused over.
class EditableProfilePhoto : public views::LabelButton {
 public:
  EditableProfilePhoto(views::ButtonListener* listener,
                       const gfx::Image& icon,
                       bool is_editing_allowed,
                       Profile* profile)
      : views::LabelButton(listener, base::string16()),
        photo_overlay_(nullptr),
        profile_(profile) {
    set_can_process_events_within_subtree(false);
    gfx::Image image =
        profiles::GetSizedAvatarIcon(icon, true, kImageSide, kImageSide);
    SetImage(views::LabelButton::STATE_NORMAL, *image.ToImageSkia());
    SetBorder(views::NullBorder());
    SetMinSize(gfx::Size(GetPreferredSize().width() + kBadgeSpacing,
                         GetPreferredSize().height() + kBadgeSpacing +
                             ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 DISTANCE_RELATED_CONTROL_VERTICAL_SMALL)));

    SetEnabled(false);
  }

  void PaintChildren(const views::PaintInfo& paint_info) override {
    {
      // Display any children (the "change photo" overlay) as a circle.
      ui::ClipRecorder clip_recorder(paint_info.context());
      gfx::Rect clip_bounds = image()->GetMirroredBounds();
      gfx::Path clip_mask;
      clip_mask.addCircle(
          clip_bounds.x() + clip_bounds.width() / 2,
          clip_bounds.y() + clip_bounds.height() / 2,
          clip_bounds.width() / 2);
      clip_recorder.ClipPathWithAntiAliasing(clip_mask);
      View::PaintChildren(paint_info);
    }

    ui::PaintRecorder paint_recorder(
        paint_info.context(), gfx::Size(kProfileBadgeSize, kProfileBadgeSize));
    gfx::Canvas* canvas = paint_recorder.canvas();
    if (profile_->IsSupervised()) {
      gfx::Rect bounds(0, 0, kProfileBadgeSize, kProfileBadgeSize);
      int badge_offset = kImageSide + kBadgeSpacing - kProfileBadgeSize;
      gfx::Vector2d badge_offset_vector = gfx::Vector2d(
          GetMirroredXWithWidthInView(badge_offset, kProfileBadgeSize),
          badge_offset + ChromeLayoutProvider::Get()->GetDistanceMetric(
                             DISTANCE_RELATED_CONTROL_VERTICAL_SMALL));

      gfx::Point center_point = bounds.CenterPoint() + badge_offset_vector;

      // Paint the circular background.
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setColor(GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_BubbleBackground));
      canvas->DrawCircle(center_point, kProfileBadgeSize / 2, flags);

      const gfx::VectorIcon* icon = profile_->IsChild()
                                        ? &kAccountChildCircleIcon
                                        : &kSupervisorAccountCircleIcon;

      // Paint the badge icon.
      int offset = (kProfileBadgeSize - kSupervisedIconBadgeSize) / 2;
      canvas->Translate(badge_offset_vector + gfx::Vector2d(offset, offset));
      gfx::PaintVectorIcon(canvas, *icon, kSupervisedIconBadgeSize,
                           gfx::kChromeIconGrey);
    }
  }

 private:
  // views::Button:
  void StateChanged(ButtonState old_state) override {
    if (photo_overlay_) {
      photo_overlay_->SetVisible(
          state() == STATE_PRESSED || state() == STATE_HOVERED || HasFocus());
    }
  }

  void OnFocus() override {
    views::LabelButton::OnFocus();
    if (photo_overlay_)
      photo_overlay_->SetVisible(true);
  }

  void OnBlur() override {
    views::LabelButton::OnBlur();
    // Don't hide the overlay if it's being shown as a result of a mouseover.
    if (photo_overlay_ && state() != STATE_HOVERED)
      photo_overlay_->SetVisible(false);
  }

  // Image that is shown when hovering over the image button. Can be NULL if
  // the photo isn't allowed to be edited (e.g. for guest profiles).
  views::ImageView* photo_overlay_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(EditableProfilePhoto);
};

// A title card with one back button left aligned and one label center aligned.
class TitleCard : public views::View {
 public:
  TitleCard(const base::string16& message, views::ButtonListener* listener,
            views::ImageButton** back_button) {
    back_button_ = CreateBackButton(listener);
    *back_button = back_button_;

    title_label_ =
        new views::Label(message, views::style::CONTEXT_DIALOG_TITLE);
    title_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);

    AddChildView(back_button_);
    AddChildView(title_label_);
  }

  // Creates a new view that has the |title_card| with horizontal padding at the
  // top, an edge-to-edge separator below, and the specified |view| at the
  // bottom.
  static views::View* AddPaddedTitleCard(views::View* view,
                                         TitleCard* title_card,
                                         int width) {
    views::View* titled_view = new views::View();
    views::GridLayout* layout =
        views::GridLayout::CreateAndInstall(titled_view);

    ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
    const gfx::Insets dialog_insets =
        provider->GetInsetsMetric(views::INSETS_DIALOG);
    // Column set 0 is a single column layout with horizontal padding at left
    // and right, and column set 1 is a single column layout with no padding.
    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddPaddingColumn(1, dialog_insets.left());
    int available_width = width - dialog_insets.width();
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
        views::GridLayout::FIXED, available_width, available_width);
    columns->AddPaddingColumn(1, dialog_insets.right());
    layout->AddColumnSet(1)->AddColumn(views::GridLayout::FILL,
                                       views::GridLayout::FILL, 0,
                                       views::GridLayout::FIXED, width, width);

    layout->StartRowWithPadding(1, 0, 0, kVerticalSpacing);
    layout->AddView(title_card);
    layout->StartRowWithPadding(1, 1, 0, kVerticalSpacing);
    layout->AddView(new views::Separator());

    layout->StartRow(1, 1);
    layout->AddView(view);

    return titled_view;
  }

 private:
  void Layout() override {
    // The back button is left-aligned.
    const int back_button_width = back_button_->GetPreferredSize().width();
    back_button_->SetBounds(0, 0, back_button_width, height());

    // The title is in the same row as the button positioned with a minimum
    // amount of space between them.
    const int button_to_title_min_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            ChromeDistanceMetric::DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
    const int unavailable_leading_space =
        back_button_width + button_to_title_min_spacing;

    // Because the title is centered, the unavailable space to the left is also
    // unavailable to right of the title.
    const int unavailable_space = 2 * unavailable_leading_space;
    const int label_width = width() - unavailable_space;
    DCHECK_GT(label_width, 0);
    title_label_->SetBounds(unavailable_leading_space, 0, label_width,
                            height());
  }

  gfx::Size CalculatePreferredSize() const override {
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
ProfileChooserView* ProfileChooserView::profile_bubble_ = nullptr;
bool ProfileChooserView::close_on_deactivate_for_testing_ = true;

// static
void ProfileChooserView::ShowBubble(
    profiles::BubbleViewMode view_mode,
    const signin::ManageAccountsParams& manage_accounts_params,
    signin_metrics::AccessPoint access_point,
    views::View* anchor_view,
    Browser* browser,
    bool is_source_keyboard) {
  if (IsShowing())
    return;

  profile_bubble_ =
      new ProfileChooserView(anchor_view, browser, view_mode,
                             manage_accounts_params.service_type, access_point);
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(profile_bubble_);
  profile_bubble_->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  profile_bubble_->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);
  widget->Show();
  if (is_source_keyboard)
    profile_bubble_->FocusFirstProfileButton();
}

// static
bool ProfileChooserView::IsShowing() {
  return profile_bubble_ != NULL;
}

// static
views::Widget* ProfileChooserView::GetCurrentBubbleWidget() {
  return profile_bubble_ ? profile_bubble_->GetWidget() : nullptr;
}

// static
void ProfileChooserView::Hide() {
  if (IsShowing())
    profile_bubble_->GetWidget()->Close();
}

ProfileChooserView::ProfileChooserView(views::View* anchor_view,
                                       Browser* browser,
                                       profiles::BubbleViewMode view_mode,
                                       signin::GAIAServiceType service_type,
                                       signin_metrics::AccessPoint access_point)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      view_mode_(view_mode),
      gaia_service_type_(service_type),
      access_point_(access_point) {
  // The sign in webview will be clipped on the bottom corners without these
  // margins, see related bug <http://crbug.com/593203>.
  set_margins(gfx::Insets(0, 0, 2, 0));
  ResetView();
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PROFILE_CHOOSER);
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
  sync_error_signin_button_ = nullptr;
  sync_error_passphrase_button_ = nullptr;
  sync_error_settings_unconfirmed_button_ = nullptr;
  sync_error_upgrade_button_ = nullptr;
  sync_error_signin_again_button_ = nullptr;
  sync_error_signout_button_ = nullptr;
  manage_accounts_link_ = nullptr;
  manage_accounts_button_ = nullptr;
  signin_current_profile_button_ = nullptr;
  current_profile_card_ = nullptr;
  first_profile_button_ = nullptr;
  guest_profile_button_ = nullptr;
  users_button_ = nullptr;
  go_incognito_button_ = nullptr;
  lock_button_ = nullptr;
  close_all_windows_button_ = nullptr;
  add_account_link_ = nullptr;
  gaia_signin_cancel_button_ = nullptr;
  remove_account_button_ = nullptr;
  account_removal_cancel_button_ = nullptr;
}

void ProfileChooserView::Init() {
  set_close_on_deactivate(close_on_deactivate_for_testing_);

  avatar_menu_.reset(new AvatarMenu(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage(),
      this, browser_));
  avatar_menu_->RebuildMenu();

  ProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
  if (oauth2_token_service)
    oauth2_token_service->AddObserver(this);

  // If view mode is PROFILE_CHOOSER but there is an auth error, force
  // ACCOUNT_MANAGEMENT mode.
  if (IsProfileChooser(view_mode_) && HasAuthError(browser_->profile()) &&
      signin::IsAccountConsistencyMirrorEnabled() &&
      avatar_menu_->GetItemAt(avatar_menu_->GetActiveProfileIndex())
          .signed_in) {
    view_mode_ = profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT;
  }

  // The arrow keys can be used to tab between items.
  AddAccelerator(ui::Accelerator(ui::VKEY_DOWN, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_UP, ui::EF_NONE));

  ShowViewFromMode(view_mode_);
}

void ProfileChooserView::OnNativeThemeChanged(
    const ui::NativeTheme* native_theme) {
  views::BubbleDialogDelegateView::OnNativeThemeChanged(native_theme);
  SetBackground(views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground)));
}

void ProfileChooserView::OnAvatarMenuChanged(
    AvatarMenu* avatar_menu) {
  if (IsProfileChooser(view_mode_) ||
      view_mode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT) {
    // Refresh the view with the new menu. We can't just update the local copy
    // as this may have been triggered by a sign out action, in which case
    // the view is being destroyed.
    ShowView(view_mode_, avatar_menu);
  }
}

void ProfileChooserView::OnRefreshTokenAvailable(
    const std::string& account_id) {
  if (view_mode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT ||
      view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
      view_mode_ == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH) {
    // The account management UI is only available through the
    // --account-consistency=mirror flag.
    ShowViewFromMode(signin::IsAccountConsistencyMirrorEnabled()
                         ? profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT
                         : profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER);
  }
}

void ProfileChooserView::OnRefreshTokenRevoked(const std::string& account_id) {
  // Refresh the account management view when an account is removed from the
  // profile.
  if (view_mode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT)
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT);
}

void ProfileChooserView::ShowView(profiles::BubbleViewMode view_to_display,
                                  AvatarMenu* avatar_menu) {
  // The account management view should only be displayed if the active profile
  // is signed in.
  if (view_to_display == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT) {
    DCHECK(signin::IsAccountConsistencyMirrorEnabled());
    const AvatarMenu::Item& active_item = avatar_menu->GetItemAt(
        avatar_menu->GetActiveProfileIndex());
    if (!active_item.signed_in) {
      // This is the case when the user selects the sign out option in the user
      // menu upon encountering unrecoverable errors. Afterwards, the profile
      // chooser view is shown instead of the account management view.
      view_to_display = profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER;
    }
  }

  if (browser_->profile()->IsSupervised() &&
      (view_to_display == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
       view_to_display == profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL)) {
    LOG(WARNING) << "Supervised user attempted to add/remove account";
    return;
  }

  ResetView();
  RemoveAllChildViews(true);
  view_mode_ = view_to_display;

  views::GridLayout* layout = nullptr;
  views::View* sub_view = nullptr;
  views::View* view_to_focus = nullptr;
  switch (view_mode_) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH:
      // The modal sign-in view is shown in for bubble view modes.
      // See |SigninViewController::ShouldShowModalSigninForMode|.
      NOTREACHED();
      break;
    case profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL:
      layout = CreateSingleColumnLayout(this, kFixedAccountRemovalViewWidth);
      sub_view = CreateAccountRemovalView();
      break;
    case profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT:
    case profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER:
      layout = CreateSingleColumnLayout(this, kFixedMenuWidth);
      sub_view = CreateProfileChooserView(avatar_menu);
      break;
  }

  layout->StartRow(1, 0);
  layout->AddView(sub_view);
  Layout();
  if (GetBubbleFrameView())
    SizeToContents();
  if (view_to_focus)
    view_to_focus->RequestFocus();
}

void ProfileChooserView::ShowViewFromMode(profiles::BubbleViewMode mode) {
  if (SigninViewController::ShouldShowModalSigninForMode(mode)) {
    // Hides the user menu if it is currently shown. The user menu automatically
    // closes when it loses focus; however, on Windows, the signin modals do not
    // take away focus, thus we need to manually close the bubble.
    Hide();
    browser_->signin_view_controller()->ShowModalSignin(mode, browser_,
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
  profile_bubble_ = NULL;
}

bool ProfileChooserView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() != ui::VKEY_DOWN &&
      accelerator.key_code() != ui::VKEY_UP)
    return BubbleDialogDelegateView::AcceleratorPressed(accelerator);

  // Move the focus up or down.
  GetFocusManager()->AdvanceFocus(accelerator.key_code() != ui::VKEY_DOWN);
  return true;
}

views::View* ProfileChooserView::GetInitiallyFocusedView() {
  return signin_current_profile_button_;
}

int ProfileChooserView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool ProfileChooserView::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Suppresses the context menu because some features, such as inspecting
  // elements, are not appropriate in a bubble.
  return true;
}

void ProfileChooserView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == guest_profile_button_) {
    PrefService* service = g_browser_process->local_state();
    DCHECK(service);
    DCHECK(service->GetBoolean(prefs::kBrowserGuestModeEnabled));
    profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
  } else if (sender == users_button_) {
    // If this is a guest session, close all the guest browser windows.
    if (browser_->profile()->IsGuestSession()) {
      profiles::CloseGuestProfileWindows();
    } else {
      UserManager::Show(base::FilePath(),
                        profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
    }
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_OPEN_USER_MANAGER);
  } else if (sender == go_incognito_button_) {
    DCHECK(ShouldShowGoIncognito());
    chrome::NewIncognitoWindow(browser_);
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_GO_INCOGNITO);
  } else if (sender == lock_button_) {
    profiles::LockProfile(browser_->profile());
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_LOCK);
  } else if (sender == close_all_windows_button_) {
    profiles::CloseProfileWindows(browser_->profile());
  } else if (sender == sync_error_signin_button_) {
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH);
  } else if (sender == sync_error_passphrase_button_ ||
             sender == sync_error_settings_unconfirmed_button_) {
    chrome::ShowSettingsSubPage(browser_, chrome::kSyncSetupSubPage);
  } else if (sender == sync_error_upgrade_button_) {
    chrome::OpenUpdateChromeDialog(browser_);
  } else if (sender == sync_error_signin_again_button_) {
    if (ProfileSyncServiceFactory::GetForProfile(browser_->profile()))
      browser_sync::ProfileSyncService::SyncEvent(
          browser_sync::ProfileSyncService::STOP_FROM_OPTIONS);
    SigninManagerFactory::GetForProfile(browser_->profile())
        ->SignOut(signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
                  signin_metrics::SignoutDelete::IGNORE_METRIC);
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN);
  } else if (sender == sync_error_signout_button_) {
    chrome::ShowSettingsSubPage(browser_, chrome::kSignOutSubPage);
  } else if (sender == remove_account_button_) {
    RemoveAccount();
  } else if (sender == account_removal_cancel_button_) {
    account_id_to_remove_.clear();
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT);
  } else if (sender == gaia_signin_cancel_button_) {
    // The account management view is only available with the
    // --account-consistency=mirror flag.
    bool account_management_available =
        SigninManagerFactory::GetForProfile(browser_->profile())
            ->IsAuthenticated() &&
        signin::IsAccountConsistencyMirrorEnabled();
    ShowViewFromMode(account_management_available ?
        profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT :
        profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER);
  } else if (sender == current_profile_card_) {
    avatar_menu_->EditProfile(avatar_menu_->GetActiveProfileIndex());
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_IMAGE);
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_NAME);
  } else if (sender == manage_accounts_button_) {
    // This button can either mean show/hide the account management view,
    // depending on which view it is displayed.
    ShowViewFromMode(view_mode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT
                         ? profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER
                         : profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT);
  } else if (sender == signin_current_profile_button_) {
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN);
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
    } else {
      // This was a profile accounts button.
      AccountButtonIndexes::const_iterator account_match =
          delete_account_button_map_.find(sender);
      if (account_match != delete_account_button_map_.end()) {
        account_id_to_remove_ = account_match->second;
        ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL);
      } else {
        account_match = reauth_account_button_map_.find(sender);
        DCHECK(account_match != reauth_account_button_map_.end());
        ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH);
      }
    }
  }
}

void ProfileChooserView::RemoveAccount() {
  DCHECK(!account_id_to_remove_.empty());
  ProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(browser_->profile());
  if (oauth2_token_service) {
    oauth2_token_service->RevokeCredentials(account_id_to_remove_);
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_REMOVE_ACCT);
  }
  account_id_to_remove_.clear();

  ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT);
}

void ProfileChooserView::LinkClicked(views::Link* sender, int event_flags) {
  if (sender == manage_accounts_link_) {
    // This link can either mean show/hide the account management view,
    // depending on which view it is displayed. ShowView() will DCHECK if
    // the account management view is displayed for non signed-in users.
    ShowViewFromMode(
        view_mode_ == profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT ?
            profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER :
            profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT);
  } else if (sender == add_account_link_) {
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT);
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_ADD_ACCT);
  }
}

void ProfileChooserView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                const gfx::Range& range,
                                                int event_flags) {
  chrome::ShowSettings(browser_);
}

views::View* ProfileChooserView::CreateProfileChooserView(
    AvatarMenu* avatar_menu) {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view, kFixedMenuWidth);
  // Separate items into active and alternatives.
  Indexes other_profiles;
  views::View* sync_error_view = NULL;
  views::View* current_profile_view = NULL;
  views::View* current_profile_accounts = NULL;
  views::View* option_buttons_view = NULL;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(i);
    if (item.active) {
      option_buttons_view = CreateOptionsView(
          item.signed_in && profiles::IsLockAvailable(browser_->profile()),
          avatar_menu);
      current_profile_view = CreateCurrentProfileView(item, false);
      if (!IsProfileChooser(view_mode_))
        current_profile_accounts = CreateCurrentProfileAccountsView(item);
      sync_error_view = CreateSyncErrorViewIfNeeded();
    } else {
      other_profiles.push_back(i);
    }
  }

  if (sync_error_view) {
    layout->StartRow(1, 0);
    layout->AddView(sync_error_view);
    layout->StartRow(0, 0);
    layout->AddView(new views::Separator());
  }

  if (!current_profile_view) {
    // Guest windows don't have an active profile.
    current_profile_view = CreateGuestProfileView();
    option_buttons_view = CreateOptionsView(false, avatar_menu);
  }

  layout->StartRow(1, 0);
  layout->AddView(current_profile_view);

  if (!IsProfileChooser(view_mode_)) {
    DCHECK(current_profile_accounts);
    layout->StartRow(0, 0);
    layout->AddView(new views::Separator());
    layout->StartRow(1, 0);
    layout->AddView(current_profile_accounts);
  }

  if (browser_->profile()->IsSupervised()) {
    layout->StartRow(1, 0);
    layout->AddView(CreateSupervisedUserDisclaimerView());
  }

  layout->StartRow(0, 0);
  layout->AddView(new views::Separator());

  if (option_buttons_view) {
    layout->StartRow(0, 0);
    layout->AddView(option_buttons_view);
  }
  return view;
}

views::View* ProfileChooserView::CreateSyncErrorViewIfNeeded() {
  int content_string_id, button_string_id;
  views::LabelButton** button_out = nullptr;
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(browser_->profile());
  sync_ui_util::AvatarSyncErrorType error =
      sync_ui_util::GetMessagesForAvatarSyncError(
          browser_->profile(), *signin_manager, &content_string_id,
          &button_string_id);
  switch (error) {
    case sync_ui_util::MANAGED_USER_UNRECOVERABLE_ERROR:
      button_out = &sync_error_signout_button_;
      break;
    case sync_ui_util::UNRECOVERABLE_ERROR:
      button_out = &sync_error_signin_again_button_;
      break;
    case sync_ui_util::SUPERVISED_USER_AUTH_ERROR:
      button_out = nullptr;
      break;
    case sync_ui_util::AUTH_ERROR:
      button_out = &sync_error_signin_button_;
      break;
    case sync_ui_util::UPGRADE_CLIENT_ERROR:
      button_out = &sync_error_upgrade_button_;
      break;
    case sync_ui_util::PASSPHRASE_ERROR:
      button_out = &sync_error_passphrase_button_;
      break;
    case sync_ui_util::SETTINGS_UNCONFIRMED_ERROR:
      button_out = &sync_error_settings_unconfirmed_button_;
      break;
    case sync_ui_util::NO_SYNC_ERROR:
      return nullptr;
    default:
      NOTREACHED();
  }

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  // Sets an overall horizontal layout.
  views::View* view = new views::View();
  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, gfx::Insets(kMenuEdgeMargin),
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  view->SetLayoutManager(layout);

  // Adds the sync problem icon.
  views::ImageView* sync_problem_icon = new views::ImageView();
  sync_problem_icon->SetImage(
      gfx::CreateVectorIcon(kSyncProblemIcon, 20, gfx::kGoogleRed700));
  view->AddChildView(sync_problem_icon);

  // Adds a vertical view to organize the error title, message, and button.
  views::View* vertical_view = new views::View();
  const int small_vertical_spacing = provider->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  views::BoxLayout* vertical_layout = new views::BoxLayout(
      views::BoxLayout::kVertical, gfx::Insets(), small_vertical_spacing);
  vertical_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  vertical_view->SetLayoutManager(vertical_layout);

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
    // If the button string is specified, then the button itself needs to be
    // already initialized.
    DCHECK(button_out);
    // Adds a padding row between error title/content and the button.
    auto* padding = new views::View;
    padding->SetPreferredSize(gfx::Size(
        0,
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
    vertical_view->AddChildView(padding);

    *button_out = views::MdTextButton::CreateSecondaryUiBlueButton(
        this, l10n_util::GetStringUTF16(button_string_id));
    vertical_view->AddChildView(*button_out);
    view->SetBorder(views::CreateEmptyBorder(0, 0, small_vertical_spacing, 0));
  }

  view->AddChildView(vertical_view);
  return view;
}

views::View* ProfileChooserView::CreateCurrentProfileView(
    const AvatarMenu::Item& avatar_item,
    bool is_guest) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  views::View* view = new views::View();
  const int vertical_spacing_small =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  view->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, gfx::Insets(vertical_spacing_small, 0), 0));

  // Container for the profile photo and avatar/user name.
  BackgroundColorHoverButton* current_profile_card =
      new BackgroundColorHoverButton(this, base::string16());
  views::GridLayout* grid_layout =
      views::GridLayout::CreateAndInstall(current_profile_card);
  views::ColumnSet* columns = grid_layout->AddColumnSet(0);
  // BackgroundColorHoverButton has already accounted for the left and right
  // margins.
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, kMenuEdgeMargin - kBadgeSpacing);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  grid_layout->AddPaddingRow(0, 0);
  const int num_labels =
      (avatar_item.signed_in && !signin::IsAccountConsistencyMirrorEnabled())
          ? 2
          : 1;
  int profile_card_height =
      kImageSide + 2 * (kBadgeSpacing + vertical_spacing_small);
  const int line_height = profile_card_height / num_labels;
  grid_layout->StartRow(0, 0, line_height);
  current_profile_card_ = current_profile_card;

  // Profile picture, left-aligned.
  EditableProfilePhoto* current_profile_photo = new EditableProfilePhoto(
      this, avatar_item.icon, !is_guest, browser_->profile());

  // Profile name, left-aligned to the right of profile icon.
  views::Label* current_profile_name = new views::Label(
      profiles::GetAvatarNameForProfile(browser_->profile()->GetPath()));
  current_profile_card->set_title(current_profile_name);
  current_profile_name->SetAutoColorReadabilityEnabled(false);
  current_profile_name->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
          1, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::MEDIUM));
  current_profile_name->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // The grid layout contains one row if not signed in or account consistency is
  // enabled. It contains 2 rows if signed in and account consistency is
  // disabled (the second row is for the email label). For the second case the
  // profile photo has to be over 2 rows.
  grid_layout->AddView(current_profile_photo, 1, num_labels);
  grid_layout->AddView(current_profile_name, 1, 1, views::GridLayout::LEADING,
                       (num_labels == 2) ? views::GridLayout::TRAILING
                                         : views::GridLayout::CENTER);
  current_profile_card_->SetMinSize(gfx::Size(
      kFixedMenuWidth, current_profile_photo->GetPreferredSize().height() +
                           2 * vertical_spacing_small));
  view->AddChildView(current_profile_card_);

  if (is_guest) {
    current_profile_card_->SetEnabled(false);
    return view;
  }

  const base::string16 profile_name =
      profiles::GetAvatarNameForProfile(browser_->profile()->GetPath());

  // The available links depend on the type of profile that is active.
  if (avatar_item.signed_in) {
    if (signin::IsAccountConsistencyMirrorEnabled()) {
      base::string16 button_text = l10n_util::GetStringUTF16(
          IsProfileChooser(view_mode_)
              ? IDS_PROFILES_PROFILE_MANAGE_ACCOUNTS_BUTTON
              : IDS_PROFILES_PROFILE_HIDE_MANAGE_ACCOUNTS_BUTTON);
      manage_accounts_button_ =
          new BackgroundColorHoverButton(this, button_text);
      manage_accounts_button_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      manage_accounts_button_->SetMinSize(
          gfx::Size(kFixedMenuWidth, kButtonHeight));
      view->AddChildView(manage_accounts_button_);
    } else {
      views::Label* email_label = new views::Label(avatar_item.username);
      current_profile_card->set_subtitle(email_label);
      email_label->SetAutoColorReadabilityEnabled(false);
      email_label->SetElideBehavior(gfx::ELIDE_EMAIL);
      email_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      grid_layout->StartRow(1, 0);
      // Skip first column for the profile icon.
      grid_layout->SkipColumns(1);
      grid_layout->AddView(email_label, 1, 1, views::GridLayout::LEADING,
                           views::GridLayout::LEADING);
    }

    current_profile_card_->SetAccessibleName(
        l10n_util::GetStringFUTF16(
            IDS_PROFILES_EDIT_SIGNED_IN_PROFILE_ACCESSIBLE_NAME,
            profile_name,
            avatar_item.username));
    return view;
  }

  SigninManagerBase* signin_manager = SigninManagerFactory::GetForProfile(
      browser_->profile()->GetOriginalProfile());
  if (signin_manager->IsSigninAllowed()) {
    views::View* extra_links_view = new views::View();
    views::BoxLayout* extra_links_layout = new views::BoxLayout(
        views::BoxLayout::kVertical,
        gfx::Insets(provider->GetDistanceMetric(
                        views::DISTANCE_RELATED_CONTROL_VERTICAL),
                    kMenuEdgeMargin),
        kMenuEdgeMargin);
    extra_links_layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
    extra_links_view->SetLayoutManager(extra_links_layout);
    views::Label* promo =
        new views::Label(l10n_util::GetStringUTF16(IDS_PROFILES_SIGNIN_PROMO));
    promo->SetMultiLine(true);
    promo->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    extra_links_view->AddChildView(promo);

    signin_current_profile_button_ =
        views::MdTextButton::CreateSecondaryUiBlueButton(
            this, l10n_util::GetStringFUTF16(
                      IDS_SYNC_START_SYNC_BUTTON_LABEL,
                      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
    extra_links_view->AddChildView(signin_current_profile_button_);
    base::RecordAction(
        base::UserMetricsAction("Signin_Impression_FromAvatarBubbleSignin"));
    extra_links_view->SetBorder(
        views::CreateEmptyBorder(0, 0, vertical_spacing_small, 0));
    view->AddChildView(extra_links_view);
  }

  current_profile_card_->SetAccessibleName(
      l10n_util::GetStringFUTF16(
          IDS_PROFILES_EDIT_PROFILE_ACCESSIBLE_NAME, profile_name));
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
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view, kFixedMenuWidth);

  const bool is_guest = browser_->profile()->IsGuestSession();
  const int kIconSize = 20;
  // Add the user switching buttons
  const int kProfileIconSize = 18;
  layout->StartRowWithPadding(1, 0, 0, vertical_spacing);
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(i);
    if (!item.active) {
      gfx::Image image = profiles::GetSizedAvatarIcon(
          item.icon, true, kProfileIconSize, kProfileIconSize,
          profiles::SHAPE_CIRCLE);
      views::LabelButton* button = new BackgroundColorHoverButton(
          this, profiles::GetProfileSwitcherTextForItem(item),
          *image.ToImageSkia());
      button->SetImageLabelSpacing(kMenuEdgeMargin);
      open_other_profile_indexes_map_[button] = i;

      if (!first_profile_button_)
        first_profile_button_ = button;
      layout->StartRow(1, 0);
      layout->AddView(button);
    }
  }

  // Add the "Guest" button for browsing as guest
  if (!is_guest && !browser_->profile()->IsSupervised()) {
    PrefService* service = g_browser_process->local_state();
    DCHECK(service);
    if (service->GetBoolean(prefs::kBrowserGuestModeEnabled)) {
      guest_profile_button_ = new BackgroundColorHoverButton(
          this, l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME),
          gfx::CreateVectorIcon(kAccountCircleIcon, kIconSize,
                                gfx::kChromeIconGrey));
      layout->StartRow(1, 0);
      layout->AddView(guest_profile_button_);
    }
  }

  base::string16 text = l10n_util::GetStringUTF16(
      is_guest ? IDS_PROFILES_EXIT_GUEST : IDS_PROFILES_MANAGE_USERS_BUTTON);
  const gfx::VectorIcon& settings_icon =
      is_guest ? kCloseAllIcon : kSettingsIcon;
  users_button_ = new BackgroundColorHoverButton(
      this, text,
      gfx::CreateVectorIcon(settings_icon, kIconSize, gfx::kChromeIconGrey));

  layout->StartRow(1, 0);
  layout->AddView(users_button_);

  if (display_lock) {
    lock_button_ = new BackgroundColorHoverButton(
        this, l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON),
        gfx::CreateVectorIcon(vector_icons::kLockIcon, kIconSize,
                              gfx::kChromeIconGrey));
    layout->StartRow(1, 0);
    layout->AddView(lock_button_);
  } else if (!is_guest) {
    close_all_windows_button_ = new BackgroundColorHoverButton(
        this, l10n_util::GetStringUTF16(IDS_PROFILES_CLOSE_ALL_WINDOWS_BUTTON),
        gfx::CreateVectorIcon(kCloseAllIcon, kIconSize, gfx::kChromeIconGrey));
    layout->StartRow(1, 0);
    layout->AddView(close_all_windows_button_);
  }

  layout->StartRowWithPadding(1, 0, 0, vertical_spacing);
  return view;
}

views::View* ProfileChooserView::CreateSupervisedUserDisclaimerView() {
  views::View* view = new views::View();
  int horizontal_margin = kMenuEdgeMargin;
  views::GridLayout* layout =
      CreateSingleColumnLayout(view, kFixedMenuWidth - 2 * horizontal_margin);
  view->SetBorder(views::CreateEmptyBorder(0, horizontal_margin,
                                           kMenuEdgeMargin, horizontal_margin));

  views::Label* disclaimer = new views::Label(
      avatar_menu_->GetSupervisedUserInformation(), CONTEXT_DEPRECATED_SMALL);
  disclaimer->SetMultiLine(true);
  disclaimer->SetAllowCharacterBreak(true);
  disclaimer->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRow(1, 0);
  layout->AddView(disclaimer);

  return view;
}

views::View* ProfileChooserView::CreateCurrentProfileAccountsView(
    const AvatarMenu::Item& avatar_item) {
  DCHECK(avatar_item.signed_in);
  views::View* view = new views::View();
  view->SetBackground(views::CreateSolidBackground(
      profiles::kAvatarBubbleAccountsBackgroundColor));
  views::GridLayout* layout = CreateSingleColumnLayout(view, kFixedMenuWidth);

  Profile* profile = browser_->profile();
  std::string primary_account =
      SigninManagerFactory::GetForProfile(profile)->GetAuthenticatedAccountId();
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

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  if (!profile->IsSupervised()) {
    layout->AddPaddingRow(0, vertical_spacing);

    add_account_link_ = CreateLink(l10n_util::GetStringFUTF16(
        IDS_PROFILES_PROFILE_ADD_ACCOUNT_BUTTON, avatar_item.name), this);
    add_account_link_->SetBorder(views::CreateEmptyBorder(
        0, provider->GetInsetsMetric(views::INSETS_DIALOG).left(),
        vertical_spacing, 0));
    layout->StartRow(1, 0);
    layout->AddView(add_account_link_);
  }

  return view;
}

void ProfileChooserView::CreateAccountButton(views::GridLayout* layout,
                                             const std::string& account_id,
                                             bool is_primary_account,
                                             bool reauth_required,
                                             int width) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  std::string email = signin_ui_util::GetDisplayEmail(browser_->profile(),
                                                      account_id);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia* delete_default_image =
      rb->GetImageNamed(IDR_CLOSE_1).ToImageSkia();
  const int kDeleteButtonWidth = delete_default_image->width();
  gfx::ImageSkia warning_default_image;
  int warning_button_width = 0;
  if (reauth_required) {
    const int kIconSize = 18;
    warning_default_image = gfx::CreateVectorIcon(
        vector_icons::kWarningIcon, kIconSize, gfx::kChromeIconGrey);
    warning_button_width =
        kIconSize +
        provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  }

  const gfx::Insets dialog_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG);

  int available_width = width - dialog_insets.width() -
      kDeleteButtonWidth - warning_button_width;
  views::LabelButton* email_button = new BackgroundColorHoverButton(
      this, base::UTF8ToUTF16(email), warning_default_image);
  email_button->SetEnabled(reauth_required);
  email_button->SetElideBehavior(gfx::ELIDE_EMAIL);
  email_button->SetMinSize(gfx::Size(0, kButtonHeight));
  email_button->SetMaxSize(gfx::Size(available_width, kButtonHeight));
  layout->StartRow(1, 0);
  layout->AddView(email_button);

  if (reauth_required)
    reauth_account_button_map_[email_button] = account_id;

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
        width - provider->GetInsetsMetric(views::INSETS_DIALOG).right() -
            kDeleteButtonWidth,
        0, kDeleteButtonWidth, kButtonHeight);

    email_button->set_notify_enter_exit_on_child(true);
    email_button->AddChildView(delete_button);

    // Save the original email address, as the button text could be elided.
    delete_account_button_map_[delete_button] = account_id;
  }
}

views::View* ProfileChooserView::CreateAccountRemovalView() {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  const gfx::Insets dialog_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG);

  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(
      view, kFixedAccountRemovalViewWidth - dialog_insets.width());

  view->SetBorder(
      views::CreateEmptyBorder(0, dialog_insets.left(),
                               dialog_insets.bottom(), dialog_insets.right()));

  const std::string& primary_account = SigninManagerFactory::GetForProfile(
      browser_->profile())->GetAuthenticatedAccountId();
  bool is_primary_account = primary_account == account_id_to_remove_;

  const int unrelated_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);

  // Adds main text.
  layout->StartRowWithPadding(1, 0, 0, unrelated_vertical_spacing);

  if (is_primary_account) {
    std::string email = signin_ui_util::GetDisplayEmail(browser_->profile(),
                                                        account_id_to_remove_);
    std::vector<size_t> offsets;
    const base::string16 settings_text =
        l10n_util::GetStringUTF16(IDS_PROFILES_SETTINGS_LINK);
    const base::string16 primary_account_removal_text =
        l10n_util::GetStringFUTF16(IDS_PROFILES_PRIMARY_ACCOUNT_REMOVAL_TEXT,
            base::UTF8ToUTF16(email), settings_text, &offsets);
    views::StyledLabel* primary_account_removal_label =
        new views::StyledLabel(primary_account_removal_text, this);
    primary_account_removal_label->AddStyleRange(
        gfx::Range(offsets[1], offsets[1] + settings_text.size()),
        views::StyledLabel::RangeStyleInfo::CreateForLink());
    primary_account_removal_label->SetTextContext(CONTEXT_DEPRECATED_SMALL);
    layout->AddView(primary_account_removal_label);
  } else {
    views::Label* content_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_PROFILES_ACCOUNT_REMOVAL_TEXT),
        CONTEXT_DEPRECATED_SMALL);
    content_label->SetMultiLine(true);
    content_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(content_label);
  }

  // Adds button.
  if (!is_primary_account) {
    remove_account_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
        this, l10n_util::GetStringUTF16(IDS_PROFILES_ACCOUNT_REMOVAL_BUTTON));
    remove_account_button_->SetHorizontalAlignment(
        gfx::ALIGN_CENTER);
    layout->StartRowWithPadding(1, 0, 0, unrelated_vertical_spacing);
    layout->AddView(remove_account_button_);
  } else {
    layout->AddPaddingRow(0, unrelated_vertical_spacing);
  }

  TitleCard* title_card = new TitleCard(
      l10n_util::GetStringUTF16(IDS_PROFILES_ACCOUNT_REMOVAL_TITLE),
      this, &account_removal_cancel_button_);
  return TitleCard::AddPaddedTitleCard(view, title_card,
      kFixedAccountRemovalViewWidth);
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
