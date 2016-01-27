// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"

#include "base/macros.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/views/profiles/signin_view_controller.h"
#include "chrome/browser/ui/views/profiles/user_manager_view.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/render_widget_host_view.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/clip_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

// Helpers --------------------------------------------------------------------

const int kFixedMenuWidth = 250;
const int kButtonHeight = 32;
const int kPasswordCombinedFixedGaiaViewWidth = 360;
const int kFixedGaiaViewWidth = 448;
const int kFixedAccountRemovalViewWidth = 280;
const int kFixedSwitchUserViewWidth = 320;
const int kLargeImageSide = 88;

const int kVerticalSpacing = 16;

const int kTitleViewNativeWidgetOffset = 8;

bool IsProfileChooser(profiles::BubbleViewMode mode) {
  return mode == profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER ||
      mode == profiles::BUBBLE_VIEW_MODE_FAST_PROFILE_CHOOSER;
}

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
  back_button->SetFocusable(true);
  return back_button;
}

// BackgroundColorHoverButton -------------------------------------------------

// A custom button that allows for setting a background color when hovered over.
class BackgroundColorHoverButton : public views::LabelButton {
 public:
  BackgroundColorHoverButton(views::ButtonListener* listener,
                             const base::string16& text,
                             const gfx::ImageSkia& icon)
      : views::LabelButton(listener, text) {
    SetImageLabelSpacing(views::kItemLabelSpacing);
    SetBorder(views::Border::CreateEmptyBorder(
        0, views::kButtonHEdgeMarginNew, 0, views::kButtonHEdgeMarginNew));
    SetMinSize(gfx::Size(0,
        kButtonHeight + views::kRelatedControlVerticalSpacing));
    SetImage(STATE_NORMAL, icon);
    SetFocusable(true);
  }

  ~BackgroundColorHoverButton() override {}

 private:
  // views::LabelButton:
  void OnPaint(gfx::Canvas* canvas) override {
    if ((state() == STATE_PRESSED) ||
        (state() == STATE_HOVERED)) {
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

  gfx::Size GetPreferredSize() const override { return preferred_size_; }

 private:
  gfx::Size preferred_size_;
};

// A view to host the GAIA webview overlapped with a back button.  This class
// is needed to reparent the back button inside a native view so that on
// windows, user input can be be properly routed to the button.
class HostView : public views::View {
 public:
  HostView() {}

  void Initialize(views::View* title_view, views::View* main_view);

 private:
  // views::View:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  // The title itself and the overlaped widget that contains it.
  views::View* title_view_ = nullptr;  // Not owned.
  scoped_ptr<views::Widget> title_widget_;

  DISALLOW_COPY_AND_ASSIGN(HostView);
};

void HostView::Initialize(views::View* title_view, views::View* main_view) {
  title_view_ = title_view;
  AddChildView(main_view);
  SetLayoutManager(new views::FillLayout());
}

void HostView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (title_widget_ != nullptr || GetWidget() == nullptr)
    return;

  // The title view must be placed within its own widget so that it can
  // properly receive user input when overlapped on another view.
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_CONTROL);
  params.parent = GetWidget()->GetNativeView();
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  title_widget_.reset(new views::Widget);
  title_widget_->Init(params);
  title_widget_->SetContentsView(title_view_);

  gfx::Rect bounds(title_view_->GetPreferredSize());
  title_view_->SetBoundsRect(bounds);
  bounds.Offset(kTitleViewNativeWidgetOffset, kTitleViewNativeWidgetOffset);
  title_widget_->SetBounds(bounds);
}

}  // namespace

// RightAlignedIconLabelButton -------------------------------------------------

// A custom LabelButton that has a centered text and right aligned icon.
class RightAlignedIconLabelButton : public views::LabelButton {
 public:
  RightAlignedIconLabelButton(views::ButtonListener* listener,
                              const base::string16& text)
      : views::LabelButton(listener, text) {
    SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    label()->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  }

 protected:
  void Layout() override {
    views::LabelButton::Layout();

    // Keep the text centered and the icon right-aligned by stretching the label
    // to take up more of the content area and centering its contents.
    gfx::Rect content_bounds = GetContentsBounds();
    gfx::Rect label_bounds = label()->bounds();
    label_bounds.Inset(content_bounds.x() - label_bounds.x(), 0, 0, 0);
    label()->SetBoundsRect(label_bounds);
  }

 private:
  void OnFocus() override {
    SetState(STATE_HOVERED);
  }

  void OnBlur() override {
    SetState(STATE_NORMAL);
  }

  DISALLOW_COPY_AND_ASSIGN(RightAlignedIconLabelButton);
};

// EditableProfilePhoto -------------------------------------------------

// A custom Image control that shows a "change" button when moused over.
class EditableProfilePhoto : public views::LabelButton {
 public:
  EditableProfilePhoto(views::ButtonListener* listener,
                       const gfx::Image& icon,
                       bool is_editing_allowed,
                       const gfx::Rect& bounds)
      : views::LabelButton(listener, base::string16()),
        photo_overlay_(NULL) {
    gfx::Image image = profiles::GetSizedAvatarIcon(
        icon, true, kLargeImageSide, kLargeImageSide);
    SetImage(views::LabelButton::STATE_NORMAL, *image.ToImageSkia());
    SetBorder(views::Border::NullBorder());
    SetBoundsRect(bounds);

    // Calculate the circular mask that will be used to display the photo.
    circular_mask_.addCircle(SkIntToScalar(bounds.width() / 2),
                             SkIntToScalar(bounds.height() / 2),
                             SkIntToScalar(bounds.width() / 2));

    if (!is_editing_allowed) {
      SetEnabled(false);
      return;
    }

    set_notify_enter_exit_on_child(true);

    // Photo overlay that appears when hovering over the button.
    photo_overlay_ = new views::ImageView();

    const SkColor kBackgroundColor = SkColorSetARGB(65, 255, 255, 255);
    photo_overlay_->set_background(
        views::Background::CreateSolidBackground(kBackgroundColor));
    photo_overlay_->SetImage(gfx::CreateVectorIcon(
        gfx::VectorIconId::PHOTO_CAMERA, 48u, SkColorSetRGB(0x33, 0x33, 0x33)));

    photo_overlay_->SetSize(bounds.size());
    photo_overlay_->SetVisible(false);
    AddChildView(photo_overlay_);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    // Display the profile picture as a circle.
    canvas->ClipPath(circular_mask_, true);
    views::LabelButton::OnPaint(canvas);
  }

  void PaintChildren(const ui::PaintContext& context) override {
    // Display any children (the "change photo" overlay) as a circle.
    ui::ClipRecorder clip_recorder(context);
    clip_recorder.ClipPathWithAntiAliasing(circular_mask_);
    View::PaintChildren(context);
  }

 private:
  // views::CustomButton:
  void StateChanged() override {
    bool show_overlay =
        (state() == STATE_PRESSED || state() == STATE_HOVERED || HasFocus());
    if (photo_overlay_)
      photo_overlay_->SetVisible(show_overlay);
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

  gfx::Path circular_mask_;

  // Image that is shown when hovering over the image button. Can be NULL if
  // the photo isn't allowed to be edited (e.g. for guest profiles).
  views::ImageView* photo_overlay_;

  DISALLOW_COPY_AND_ASSIGN(EditableProfilePhoto);
};

// EditableProfileName -------------------------------------------------

// A custom text control that turns into a textfield for editing when clicked.
class EditableProfileName : public views::View,
                            public views::ButtonListener {
 public:
  EditableProfileName(views::TextfieldController* controller,
                      const base::string16& text,
                      bool is_editing_allowed)
      : button_(new RightAlignedIconLabelButton(this, text)),
        profile_name_textfield_(new views::Textfield()) {
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    const gfx::FontList& medium_font_list =
        rb->GetFontList(ui::ResourceBundle::MediumFont);
    button_->SetFontList(medium_font_list);

    if (!is_editing_allowed) {
      button_->SetBorder(views::Border::CreateEmptyBorder(2, 0, 2, 0));
      return;
    }

    // Show an "edit" pencil icon when hovering over. In the default state,
    // we need to create an empty placeholder of the correct size, so that
    // the text doesn't jump around when the hovered icon appears.
    // TODO(estade): revisit colors and press effect.
    const int kIconSize = 16;
    button_->SetImage(views::LabelButton::STATE_NORMAL,
                      CreateSquarePlaceholderImage(kIconSize));
    button_->SetImage(views::LabelButton::STATE_HOVERED,
                      gfx::CreateVectorIcon(
                          gfx::VectorIconId::MODE_EDIT, kIconSize,
                          SkColorSetRGB(0x33, 0x33, 0x33)));
    button_->SetImage(views::LabelButton::STATE_PRESSED,
                      gfx::CreateVectorIcon(
                          gfx::VectorIconId::MODE_EDIT, kIconSize,
                          SkColorSetRGB(0x20, 0x20, 0x20)));
    // To center the text, we need to offest it by the width of the icon we
    // are adding and its padding. We need to also add a small top/bottom
    // padding to account for the textfield's border.
    const int kIconTextLabelButtonSpacing = 5;
    button_->SetBorder(views::Border::CreateEmptyBorder(
        2, kIconSize + kIconTextLabelButtonSpacing, 2, 0));

    // Textfield that overlaps the button.
    profile_name_textfield_->set_controller(controller);
    profile_name_textfield_->SetFontList(medium_font_list);
    profile_name_textfield_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    profile_name_textfield_->SetVisible(false);

    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
    AddChildView(button_);
    AddChildView(profile_name_textfield_);
  }

  views::Textfield* profile_name_textfield() {
    return profile_name_textfield_;
  }

  // Hide the editable textfield to show the profile name button instead.
  void ShowReadOnlyView() {
    button_->SetVisible(true);
    profile_name_textfield_->SetVisible(false);
  }

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    button_->SetVisible(false);
    profile_name_textfield_->SetVisible(true);
    profile_name_textfield_->SetText(button_->GetText());
    profile_name_textfield_->SelectAll(false);
    profile_name_textfield_->RequestFocus();
    // Re-layouts the view after swaping the controls.
    Layout();
  }

  // views::LabelButton:
  bool OnKeyReleased(const ui::KeyEvent& event) override {
    // Override CustomButton's implementation, which presses the button when
    // you press space and clicks it when you release space, as the space can be
    // part of the new profile name typed in the textfield.
    return false;
  }

  RightAlignedIconLabelButton* button_;

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
    back_button_ = CreateBackButton(listener);
    *back_button = back_button_;

    title_label_ = new views::Label(message);
    title_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    const gfx::FontList& medium_font_list =
        rb->GetFontList(ui::ResourceBundle::MediumFont);
    title_label_->SetFontList(medium_font_list);

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

    layout->StartRowWithPadding(1, 0, 0, kVerticalSpacing);
    layout->AddView(title_card);
    layout->StartRowWithPadding(1, 1, 0, kVerticalSpacing);
    layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

    layout->StartRow(1, 1);
    layout->AddView(view);

    return titled_view;
  }

 private:
  void Layout() override {
    int back_button_width = back_button_->GetPreferredSize().width();
    back_button_->SetBounds(0, 0, back_button_width, height());
    int label_padding = back_button_width + views::kButtonHEdgeMarginNew;
    int label_width = width() - 2 * label_padding;
    DCHECK_GT(label_width, 0);
    title_label_->SetBounds(label_padding, 0, label_width, height());
  }

  gfx::Size GetPreferredSize() const override {
    int height = std::max(title_label_->GetPreferredSize().height(),
        back_button_->GetPreferredSize().height());
    return gfx::Size(width(), height);
  }

  views::ImageButton* back_button_;
  views::Label* title_label_;

  DISALLOW_COPY_AND_ASSIGN(TitleCard);
};

// ProfileBadge --------------------------------------------------------

const size_t kProfileBadgeSize = 30;
const size_t kProfileBadgeWhitePadding = 2;

// Draws a white circle, then a light blue circle, then a dark blue icon.
class ProfileBadge : public gfx::CanvasImageSource {
 public:
  ProfileBadge(gfx::VectorIconId id, size_t icon_size)
      : CanvasImageSource(gfx::Size(kProfileBadgeSize, kProfileBadgeSize),
                          false),
        id_(id),
        icon_size_(icon_size) {}

  ~ProfileBadge() override {}

  // CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    const SkISize size = canvas->sk_canvas()->getBaseLayerSize();
    gfx::Rect bounds(0, 0, size.width(), size.height());

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorWHITE);
    canvas->DrawCircle(bounds.CenterPoint(), size.width() / 2, paint);

    paint.setColor(SkColorSetRGB(0xAF, 0xD9, 0xFC));
    canvas->DrawCircle(bounds.CenterPoint(),
                       size.width() / 2 - kProfileBadgeWhitePadding, paint);

    int offset = (kProfileBadgeSize - icon_size_) / 2;
    canvas->Translate(gfx::Vector2d(offset, offset));
    gfx::PaintVectorIcon(canvas, id_, icon_size_, SkColorSetRGB(0, 0x66, 0xff));
  }

 private:
  const gfx::VectorIconId id_;
  const size_t icon_size_;

  DISALLOW_COPY_AND_ASSIGN(ProfileBadge);
};

gfx::ImageSkia CreateBadgeForProfile(Profile* profile) {
  ProfileBadge* badge =
      profile->IsChild()
          ? new ProfileBadge(gfx::VectorIconId::ACCOUNT_CHILD_INVERT, 26)
          : new ProfileBadge(gfx::VectorIconId::SUPERVISOR_ACCOUNT, 20);

  return gfx::ImageSkia(badge, badge->size());
}

// ProfileChooserView ---------------------------------------------------------

// static
ProfileChooserView* ProfileChooserView::profile_bubble_ = nullptr;
bool ProfileChooserView::close_on_deactivate_for_testing_ = true;

// static
void ProfileChooserView::ShowBubble(
    profiles::BubbleViewMode view_mode,
    profiles::TutorialMode tutorial_mode,
    const signin::ManageAccountsParams& manage_accounts_params,
    signin_metrics::AccessPoint access_point,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    views::BubbleBorder::BubbleAlignment border_alignment,
    Browser* browser) {
  // Don't start creating the view if it would be an empty fast user switcher.
  // It has to happen here to prevent the view system from creating an empty
  // container.
  if (view_mode == profiles::BUBBLE_VIEW_MODE_FAST_PROFILE_CHOOSER &&
      !profiles::HasProfileSwitchTargets(browser->profile())) {
    return;
  }

  if (IsShowing()) {
    if (tutorial_mode != profiles::TUTORIAL_MODE_NONE) {
      profile_bubble_->tutorial_mode_ = tutorial_mode;
      profile_bubble_->ShowViewFromMode(view_mode);
    }
    return;
  }

  profile_bubble_ = new ProfileChooserView(
      anchor_view, arrow, browser, view_mode, tutorial_mode,
      manage_accounts_params.service_type, access_point);
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
                                       profiles::TutorialMode tutorial_mode,
                                       signin::GAIAServiceType service_type,
                                       signin_metrics::AccessPoint access_point)
    : BubbleDelegateView(anchor_view, arrow),
      browser_(browser),
      view_mode_(view_mode),
      tutorial_mode_(tutorial_mode),
      gaia_service_type_(service_type),
      access_point_(access_point) {
  // Reset the default margins inherited from the BubbleDelegateView.
  // Add a small bottom inset so that the bubble's rounded corners show up.
  set_margins(gfx::Insets(0, 0, 1, 0));
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
  tutorial_close_button_ = NULL;
  tutorial_sync_settings_link_ = NULL;
  tutorial_see_whats_new_button_ = NULL;
  tutorial_not_you_link_ = NULL;
  tutorial_learn_more_link_ = NULL;
}

void ProfileChooserView::Init() {
  // If view mode is PROFILE_CHOOSER but there is an auth error, force
  // ACCOUNT_MANAGEMENT mode.
  if (IsProfileChooser(view_mode_) &&
      HasAuthError(browser_->profile()) &&
      switches::IsEnableAccountConsistency() &&
      avatar_menu_->GetItemAt(avatar_menu_->GetActiveProfileIndex()).
          signed_in) {
    view_mode_ = profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT;
  }

  // The arrow keys can be used to tab between items.
  AddAccelerator(ui::Accelerator(ui::VKEY_DOWN, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_UP, ui::EF_NONE));

  ShowViewFromMode(view_mode_);
}

void ProfileChooserView::OnNativeThemeChanged(
    const ui::NativeTheme* native_theme) {
  views::BubbleDelegateView::OnNativeThemeChanged(native_theme);
  set_background(views::Background::CreateSolidBackground(
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_DialogBackground)));
  if (auth_error_email_button_) {
    auth_error_email_button_->SetTextColor(
        views::LabelButton::STATE_NORMAL,
        native_theme->GetSystemColor(ui::NativeTheme::kColorId_LinkEnabled));
  }
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
    // --enable-account-consistency flag.
    ShowViewFromMode(switches::IsEnableAccountConsistency() ?
        profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT :
        profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER);
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
    DCHECK(switches::IsEnableAccountConsistency());
    const AvatarMenu::Item& active_item = avatar_menu->GetItemAt(
        avatar_menu->GetActiveProfileIndex());
    DCHECK(active_item.signed_in);
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
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH: {
      const int width = switches::UsePasswordSeparatedSigninFlow()
          ? kFixedGaiaViewWidth : kPasswordCombinedFixedGaiaViewWidth;
      layout = CreateSingleColumnLayout(this, width);
      DCHECK(!switches::UsePasswordSeparatedSigninFlow());
      sub_view = CreateGaiaSigninView(&view_to_focus);
      break;
    }
    case profiles::BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL:
      layout = CreateSingleColumnLayout(this, kFixedAccountRemovalViewWidth);
      sub_view = CreateAccountRemovalView();
      break;
    case profiles::BUBBLE_VIEW_MODE_SWITCH_USER:
      layout = CreateSingleColumnLayout(this, kFixedSwitchUserViewWidth);
      sub_view = CreateSwitchUserView();
      ProfileMetrics::LogProfileNewAvatarMenuNotYou(
          ProfileMetrics::PROFILE_AVATAR_MENU_NOT_YOU_VIEW);
      break;
    case profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT:
    case profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER:
    case profiles::BUBBLE_VIEW_MODE_FAST_PROFILE_CHOOSER:
      layout = CreateSingleColumnLayout(this, kFixedMenuWidth);
      sub_view = CreateProfileChooserView(avatar_menu);
      break;
  }
  // Clears tutorial mode for all non-profile-chooser views.
  if (view_mode_ != profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER)
    tutorial_mode_ = profiles::TUTORIAL_MODE_NONE;

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
    BrowserWindow::AvatarBubbleMode converted_mode =
        BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT;
    if (mode == profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN) {
      converted_mode = BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN;
    } else if (mode == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT) {
      converted_mode = BrowserWindow::AVATAR_BUBBLE_MODE_ADD_ACCOUNT;
    } else if (mode == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH) {
      converted_mode = BrowserWindow::AVATAR_BUBBLE_MODE_REAUTH;
    }

    browser_->window()->ShowModalSigninWindow(converted_mode, access_point_);
  } else {
    ShowView(mode, avatar_menu_.get());
  }
}

void ProfileChooserView::WindowClosing() {
  DCHECK_EQ(profile_bubble_, this);
  profile_bubble_ = NULL;

  if (tutorial_mode_ == profiles::TUTORIAL_MODE_CONFIRM_SIGNIN) {
    LoginUIServiceFactory::GetForProfile(browser_->profile())->
        SyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  }
}

bool ProfileChooserView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() != ui::VKEY_DOWN &&
      accelerator.key_code() != ui::VKEY_UP)
    return BubbleDelegateView::AcceleratorPressed(accelerator);
  // Move the focus up or down.
  GetFocusManager()->AdvanceFocus(accelerator.key_code() != ui::VKEY_DOWN);
  return true;
}

views::View* ProfileChooserView::GetInitiallyFocusedView() {
  return signin_current_profile_link_;
}

bool ProfileChooserView::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Suppresses the context menu because some features, such as inspecting
  // elements, are not appropriate in a bubble.
  return true;
}

void ProfileChooserView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == users_button_) {
    // If this is a guest session, close all the guest browser windows.
    if (browser_->profile()->IsGuestSession()) {
      profiles::CloseGuestProfileWindows();
    } else {
      UserManager::Show(base::FilePath(),
                        profiles::USER_MANAGER_NO_TUTORIAL,
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
  } else if (sender == auth_error_email_button_) {
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH);
  } else if (sender == tutorial_sync_settings_ok_button_) {
    LoginUIServiceFactory::GetForProfile(browser_->profile())->
        SyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
    DismissTutorial();
    ProfileMetrics::LogProfileNewAvatarMenuSignin(
        ProfileMetrics::PROFILE_AVATAR_MENU_SIGNIN_OK);
  } else if (sender == tutorial_close_button_) {
    DCHECK(tutorial_mode_ != profiles::TUTORIAL_MODE_NONE &&
           tutorial_mode_ != profiles::TUTORIAL_MODE_CONFIRM_SIGNIN);
    DismissTutorial();
  } else if (sender == tutorial_see_whats_new_button_) {
    ProfileMetrics::LogProfileNewAvatarMenuUpgrade(
        ProfileMetrics::PROFILE_AVATAR_MENU_UPGRADE_WHATS_NEW);
    UserManager::Show(base::FilePath(),
                      profiles::USER_MANAGER_TUTORIAL_OVERVIEW,
                      profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
  } else if (sender == remove_account_button_) {
    RemoveAccount();
  } else if (sender == account_removal_cancel_button_) {
    account_id_to_remove_.clear();
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT);
  } else if (sender == gaia_signin_cancel_button_) {
    // The account management view is only available with the
    // --enable-account-consistency flag.
    bool account_management_available =
        SigninManagerFactory::GetForProfile(browser_->profile())->
            IsAuthenticated() &&
        switches::IsEnableAccountConsistency();
    ShowViewFromMode(account_management_available ?
        profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT :
        profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER);
  } else if (sender == current_profile_photo_) {
    avatar_menu_->EditProfile(avatar_menu_->GetActiveProfileIndex());
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_IMAGE);
  } else if (sender == signin_current_profile_link_) {
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN);
  } else if (sender == add_person_button_) {
    ProfileMetrics::LogProfileNewAvatarMenuNotYou(
        ProfileMetrics::PROFILE_AVATAR_MENU_NOT_YOU_ADD_PERSON);
    UserManager::Show(base::FilePath(),
                      profiles::USER_MANAGER_NO_TUTORIAL,
                      profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
  } else if (sender == disconnect_button_) {
    ProfileMetrics::LogProfileNewAvatarMenuNotYou(
        ProfileMetrics::PROFILE_AVATAR_MENU_NOT_YOU_DISCONNECT);
    chrome::ShowSettings(browser_);
  } else if (sender == switch_user_cancel_button_) {
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER);
    ProfileMetrics::LogProfileNewAvatarMenuNotYou(
        ProfileMetrics::PROFILE_AVATAR_MENU_NOT_YOU_BACK);
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
  } else if (sender == tutorial_sync_settings_link_) {
    LoginUIServiceFactory::GetForProfile(browser_->profile())->
        SyncConfirmationUIClosed(LoginUIService::CONFIGURE_SYNC_FIRST);
    tutorial_mode_ = profiles::TUTORIAL_MODE_NONE;
    ProfileMetrics::LogProfileNewAvatarMenuSignin(
        ProfileMetrics::PROFILE_AVATAR_MENU_SIGNIN_SETTINGS);
  } else if (sender == tutorial_not_you_link_) {
    ProfileMetrics::LogProfileNewAvatarMenuUpgrade(
        ProfileMetrics::PROFILE_AVATAR_MENU_UPGRADE_NOT_YOU);
    ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_SWITCH_USER);
  } else {
    DCHECK(sender == tutorial_learn_more_link_);
    signin_ui_util::ShowSigninErrorLearnMorePage(browser_->profile());
  }
}

void ProfileChooserView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                const gfx::Range& range,
                                                int event_flags) {
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
    base::TrimWhitespace(new_profile_name, base::TRIM_ALL, &new_profile_name);
    if (new_profile_name.empty())
      return true;

    const AvatarMenu::Item& active_item = avatar_menu_->GetItemAt(
        avatar_menu_->GetActiveProfileIndex());
    Profile* profile = g_browser_process->profile_manager()->GetProfile(
        active_item.profile_path);
    DCHECK(profile);

    if (profile->IsLegacySupervised())
      return true;

    profiles::UpdateProfileName(profile, new_profile_name);
    PostActionPerformed(ProfileMetrics::PROFILE_DESKTOP_MENU_EDIT_NAME);
    current_profile_name_->ShowReadOnlyView();
    return true;
  }
  return false;
}

void ProfileChooserView::PopulateCompleteProfileChooserView(
    views::GridLayout* layout,
    AvatarMenu* avatar_menu) {
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
          item.signed_in && profiles::IsLockAvailable(browser_->profile()));
      current_profile_view = CreateCurrentProfileView(item, false);
      if (IsProfileChooser(view_mode_)) {
        tutorial_view = CreateTutorialViewIfNeeded(item);
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
  } else {
    tutorial_mode_ = profiles::TUTORIAL_MODE_NONE;
  }

  if (!current_profile_view) {
    // Guest windows don't have an active profile.
    current_profile_view = CreateGuestProfileView();
    option_buttons_view = CreateOptionsView(false);
  }

  layout->StartRow(1, 0);
  layout->AddView(current_profile_view);

  if (!IsProfileChooser(view_mode_)) {
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

  layout->StartRow(0, 0);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

  if (option_buttons_view) {
    layout->StartRow(0, 0);
    layout->AddView(option_buttons_view);
  }
}

void ProfileChooserView::PopulateMinimalProfileChooserView(
    views::GridLayout* layout,
    AvatarMenu* avatar_menu) {
  Indexes other_profiles;
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(i);
    if (!item.active) {
      other_profiles.push_back(i);
    }
  }

  layout->StartRow(1, 0);
  layout->AddView(CreateOtherProfilesView(other_profiles));
}

views::View* ProfileChooserView::CreateProfileChooserView(
    AvatarMenu* avatar_menu) {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view, kFixedMenuWidth);

  if (view_mode_ == profiles::BUBBLE_VIEW_MODE_FAST_PROFILE_CHOOSER) {
    PopulateMinimalProfileChooserView(layout, avatar_menu);
    // The user is using right-click switching, no need to tell them about it.
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetBoolean(
        prefs::kProfileAvatarRightClickTutorialDismissed, true);
  } else {
    PopulateCompleteProfileChooserView(layout, avatar_menu);
  }

  return view;
}

void ProfileChooserView::DismissTutorial() {
  // Never shows the upgrade tutorial again if manually closed.
  if (tutorial_mode_ == profiles::TUTORIAL_MODE_WELCOME_UPGRADE) {
    browser_->profile()->GetPrefs()->SetInteger(
        prefs::kProfileAvatarTutorialShown,
        signin_ui_util::kUpgradeWelcomeTutorialShowMax + 1);
  }

  if (tutorial_mode_ == profiles::TUTORIAL_MODE_RIGHT_CLICK_SWITCHING) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetBoolean(
        prefs::kProfileAvatarRightClickTutorialDismissed, true);
  }

  tutorial_mode_ = profiles::TUTORIAL_MODE_NONE;
  ShowViewFromMode(profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER);
}

views::View* ProfileChooserView::CreateTutorialViewIfNeeded(
    const AvatarMenu::Item& item) {
  if (tutorial_mode_ == profiles::TUTORIAL_MODE_CONFIRM_SIGNIN)
    return CreateSigninConfirmationView();

  if (tutorial_mode_ == profiles::TUTORIAL_MODE_SHOW_ERROR)
    return CreateSigninErrorView();

  if (profiles::ShouldShowWelcomeUpgradeTutorial(
      browser_->profile(), tutorial_mode_)) {
    if (tutorial_mode_ != profiles::TUTORIAL_MODE_WELCOME_UPGRADE) {
      Profile* profile = browser_->profile();
      const int show_count = profile->GetPrefs()->GetInteger(
          prefs::kProfileAvatarTutorialShown);
      profile->GetPrefs()->SetInteger(
          prefs::kProfileAvatarTutorialShown, show_count + 1);
    }

    return CreateWelcomeUpgradeTutorialView(item);
  }

  if (profiles::ShouldShowRightClickTutorial(browser_->profile()))
    return CreateRightClickTutorialView();

  return nullptr;
}

views::View* ProfileChooserView::CreateTutorialView(
    profiles::TutorialMode tutorial_mode,
    const base::string16& title_text,
    const base::string16& content_text,
    const base::string16& link_text,
    const base::string16& button_text,
    bool stack_button,
    views::Link** link,
    views::LabelButton** button,
    views::ImageButton** close_button) {
  tutorial_mode_ = tutorial_mode;

  views::View* view = new views::View();
  view->set_background(views::Background::CreateSolidBackground(
      profiles::kAvatarTutorialBackgroundColor));
  views::GridLayout* layout = CreateSingleColumnLayout(view,
      kFixedMenuWidth - 2 * views::kButtonHEdgeMarginNew);
  // Creates a second column set for buttons and links.
  views::ColumnSet* button_columns = layout->AddColumnSet(1);
  button_columns->AddColumn(views::GridLayout::LEADING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  button_columns->AddPaddingColumn(
      1, views::kUnrelatedControlHorizontalSpacing);
  button_columns->AddColumn(views::GridLayout::TRAILING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  layout->SetInsets(views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew,
                    views::kButtonVEdgeMarginNew,
                    views::kButtonHEdgeMarginNew);

  // Adds title and close button if needed.
  views::Label* title_label = new views::Label(title_text);
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetAutoColorReadabilityEnabled(false);
  title_label->SetEnabledColor(SK_ColorWHITE);
  title_label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));

  if (close_button) {
    layout->StartRow(1, 1);
    layout->AddView(title_label);
    *close_button = new views::ImageButton(this);
    (*close_button)->SetImageAlignment(views::ImageButton::ALIGN_RIGHT,
                                       views::ImageButton::ALIGN_MIDDLE);
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    (*close_button)->SetImage(views::ImageButton::STATE_NORMAL,
                              rb->GetImageSkiaNamed(IDR_CLOSE_1));
    (*close_button)->SetImage(views::ImageButton::STATE_HOVERED,
                              rb->GetImageSkiaNamed(IDR_CLOSE_1_H));
    (*close_button)->SetImage(views::ImageButton::STATE_PRESSED,
                              rb->GetImageSkiaNamed(IDR_CLOSE_1_P));
    layout->AddView(*close_button);
  } else {
    layout->StartRow(1, 0);
    layout->AddView(title_label);
  }

  // Adds body content.
  views::Label* content_label = new views::Label(content_text);
  content_label->SetMultiLine(true);
  content_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  content_label->SetAutoColorReadabilityEnabled(false);
  content_label->SetEnabledColor(profiles::kAvatarTutorialContentTextColor);
  layout->StartRowWithPadding(1, 0, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(content_label);

  // Adds links and buttons.
  bool has_button = !button_text.empty();
  if (has_button) {
    *button = new views::LabelButton(this, button_text);
    (*button)->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    (*button)->SetStyle(views::Button::STYLE_BUTTON);
  }

  bool has_link = !link_text.empty();
  if (has_link) {
    *link = CreateLink(link_text, this);
    (*link)->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    (*link)->SetAutoColorReadabilityEnabled(false);
    (*link)->SetEnabledColor(SK_ColorWHITE);
  }

  if (stack_button) {
    DCHECK(has_button);
    layout->StartRowWithPadding(
        1, 0, 0, views::kUnrelatedControlVerticalSpacing);
    layout->AddView(*button);
    if (has_link) {
      layout->StartRowWithPadding(
          1, 0, 0, views::kRelatedControlVerticalSpacing);
      (*link)->SetHorizontalAlignment(gfx::ALIGN_CENTER);
      layout->AddView(*link);
    }
  } else {
    DCHECK(has_link || has_button);
    layout->StartRowWithPadding(
        1, 1, 0, views::kUnrelatedControlVerticalSpacing);
    if (has_link)
      layout->AddView(*link);
    else
      layout->SkipColumns(1);
    if (has_button)
      layout->AddView(*button);
    else
      layout->SkipColumns(1);
  }

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

  if (browser_->profile()->IsSupervised()) {
    views::ImageView* supervised_icon = new views::ImageView();
    supervised_icon->SetImage(CreateBadgeForProfile(browser_->profile()));
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
  bool editing_allowed = !is_guest &&
                         !browser_->profile()->IsLegacySupervised();
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
          IsProfileChooser(view_mode_) ?
              IDS_PROFILES_PROFILE_MANAGE_ACCOUNTS_BUTTON :
              IDS_PROFILES_PROFILE_HIDE_MANAGE_ACCOUNTS_BUTTON);
      manage_accounts_link_ = CreateLink(link_title, this);
      manage_accounts_link_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
      layout->AddView(manage_accounts_link_);
    } else {
      // Badge the email address if there's an authentication error.
      if (HasAuthError(browser_->profile())) {
        auth_error_email_button_ =
            new RightAlignedIconLabelButton(this, avatar_item.username);
        auth_error_email_button_->SetElideBehavior(gfx::ELIDE_EMAIL);
        auth_error_email_button_->SetImage(
            views::LabelButton::STATE_NORMAL,
            gfx::CreateVectorIcon(gfx::VectorIconId::WARNING, 18,
                                  gfx::kChromeIconGrey));

        auth_error_email_button_->SetFocusable(true);
        gfx::Insets insets =
            views::LabelButtonAssetBorder::GetDefaultInsetsForStyle(
                views::Button::STYLE_TEXTBUTTON);
        auth_error_email_button_->SetBorder(views::Border::CreateEmptyBorder(
            insets.top(), insets.left(), insets.bottom(), insets.right()));
        layout->AddView(auth_error_email_button_);
      } else {
        // Add a small padding between the email button and the profile name.
        layout->StartRowWithPadding(1, 0, 0, 2);
        views::Label* email_label = new views::Label(avatar_item.username);
        email_label->SetElideBehavior(gfx::ELIDE_EMAIL);
        email_label->SetEnabled(false);
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

  for (size_t index : avatars_to_show) {
    const AvatarMenu::Item& item = avatar_menu_->GetItemAt(index);
    const int kSmallImageSide = 32;

    // Use the low-res, small default avatars in the fast user switcher, like
    // we do in the menu bar.
    gfx::Image item_icon;
    bool is_rectangle;
    AvatarMenu::GetImageForMenuButton(
        item.profile_path, &item_icon, &is_rectangle);

    gfx::Image image = profiles::GetSizedAvatarIcon(
        item_icon, true, kSmallImageSide, kSmallImageSide);

    views::LabelButton* button = new BackgroundColorHoverButton(
        this,
        profiles::GetProfileSwitcherTextForItem(item),
        *image.ToImageSkia());
    open_other_profile_indexes_map_[button] = index;

    layout->StartRow(1, 0);
    layout->AddView(new views::Separator(views::Separator::HORIZONTAL));
    layout->StartRow(1, 0);
    layout->AddView(button);
  }

  return view;
}

views::View* ProfileChooserView::CreateOptionsView(bool display_lock) {
  views::View* view = new views::View();
  views::GridLayout* layout = CreateSingleColumnLayout(view, kFixedMenuWidth);

  base::string16 text = browser_->profile()->IsGuestSession() ?
      l10n_util::GetStringUTF16(IDS_PROFILES_EXIT_GUEST) :
      l10n_util::GetStringUTF16(IDS_PROFILES_SWITCH_USERS_BUTTON);
  const int kIconSize = 16;
  users_button_ = new BackgroundColorHoverButton(
      this, text, gfx::CreateVectorIcon(gfx::VectorIconId::ACCOUNT_BOX,
                                        kIconSize, gfx::kChromeIconGrey));
  layout->StartRow(1, 0);
  layout->AddView(users_button_);

  if (ShouldShowGoIncognito()) {
    layout->StartRow(1, 0);
    layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    go_incognito_button_ = new BackgroundColorHoverButton(
        this,
        l10n_util::GetStringUTF16(IDS_PROFILES_GO_INCOGNITO_BUTTON),
        *rb->GetImageSkiaNamed(IDR_ICON_PROFILES_MENU_INCOGNITO));
    layout->StartRow(1, 0);
    layout->AddView(go_incognito_button_);
  }

  if (display_lock) {
    layout->StartRow(1, 0);
    layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

    lock_button_ = new BackgroundColorHoverButton(
        this, l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON),
        gfx::CreateVectorIcon(gfx::VectorIconId::LOCK, kIconSize,
                              gfx::kChromeIconGrey));
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
  disclaimer->SetAllowCharacterBreak(true);
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
                                             const std::string& account_id,
                                             bool is_primary_account,
                                             bool reauth_required,
                                             int width) {
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
        gfx::VectorIconId::WARNING, kIconSize, gfx::kChromeIconGrey);
    warning_button_width = kIconSize + views::kRelatedButtonHSpacing;
  }
  int available_width = width - 2 * views::kButtonHEdgeMarginNew -
                        kDeleteButtonWidth - warning_button_width;
  views::LabelButton* email_button = new BackgroundColorHoverButton(
      reauth_required ? this : NULL,
      base::UTF8ToUTF16(email),
      warning_default_image);
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
        width - views::kButtonHEdgeMarginNew - kDeleteButtonWidth,
        0, kDeleteButtonWidth, kButtonHeight);

    email_button->set_notify_enter_exit_on_child(true);
    email_button->AddChildView(delete_button);

    // Save the original email address, as the button text could be elided.
    delete_account_button_map_[delete_button] = account_id;
  }
}

views::View* ProfileChooserView::CreateGaiaSigninView(
    views::View** signin_content_view) {
  views::WebView* web_view = SigninViewController::CreateGaiaWebView(
      this, view_mode_, browser_->profile(), access_point_);

  int message_id;
  switch (view_mode_) {
    case profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN:
      message_id = IDS_PROFILES_GAIA_SIGNIN_TITLE;
      break;
    case profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT:
      message_id = IDS_PROFILES_GAIA_ADD_ACCOUNT_TITLE;
      break;
    case profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH: {
      message_id = IDS_PROFILES_GAIA_REAUTH_TITLE;
      break;
    }
    default:
      NOTREACHED() << "Called with invalid mode=" << view_mode_;
      return NULL;
  }

  if (signin_content_view)
    *signin_content_view = web_view;

  TitleCard* title_card = new TitleCard(l10n_util::GetStringUTF16(message_id),
                                        this,
                                        &gaia_signin_cancel_button_);
  return TitleCard::AddPaddedTitleCard(
      web_view, title_card, kPasswordCombinedFixedGaiaViewWidth);
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
      browser_->profile())->GetAuthenticatedAccountId();
  bool is_primary_account = primary_account == account_id_to_remove_;

  // Adds main text.
  layout->StartRowWithPadding(1, 0, 0, views::kUnrelatedControlVerticalSpacing);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  const gfx::FontList& small_font_list =
      rb->GetFontList(ui::ResourceBundle::SmallFont);

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

views::View* ProfileChooserView::CreateWelcomeUpgradeTutorialView(
    const AvatarMenu::Item& avatar_item) {
  ProfileMetrics::LogProfileNewAvatarMenuUpgrade(
      ProfileMetrics::PROFILE_AVATAR_MENU_UPGRADE_VIEW);

  // For local profiles, the "Not you" link doesn't make sense.
  base::string16 link_message = avatar_item.signed_in ?
      l10n_util::GetStringFUTF16(IDS_PROFILES_NOT_YOU, avatar_item.name) :
      base::string16();

  return CreateTutorialView(
      profiles::TUTORIAL_MODE_WELCOME_UPGRADE,
      l10n_util::GetStringUTF16(
          IDS_PROFILES_WELCOME_UPGRADE_TUTORIAL_TITLE),
      l10n_util::GetStringUTF16(
          IDS_PROFILES_WELCOME_UPGRADE_TUTORIAL_CONTENT_TEXT),
      link_message,
      l10n_util::GetStringUTF16(IDS_PROFILES_TUTORIAL_WHATS_NEW_BUTTON),
      true /* stack_button */,
      &tutorial_not_you_link_,
      &tutorial_see_whats_new_button_,
      &tutorial_close_button_);
}

views::View* ProfileChooserView::CreateSigninConfirmationView() {
  ProfileMetrics::LogProfileNewAvatarMenuSignin(
      ProfileMetrics::PROFILE_AVATAR_MENU_SIGNIN_VIEW);

  return CreateTutorialView(
      profiles::TUTORIAL_MODE_CONFIRM_SIGNIN,
      l10n_util::GetStringUTF16(IDS_PROFILES_CONFIRM_SIGNIN_TUTORIAL_TITLE),
      l10n_util::GetStringUTF16(
          IDS_PROFILES_CONFIRM_SIGNIN_TUTORIAL_CONTENT_TEXT),
      l10n_util::GetStringUTF16(IDS_PROFILES_SYNC_SETTINGS_LINK),
      l10n_util::GetStringUTF16(IDS_PROFILES_TUTORIAL_OK_BUTTON),
      false /* stack_button */,
      &tutorial_sync_settings_link_,
      &tutorial_sync_settings_ok_button_,
      NULL /* close_button*/);
}

views::View* ProfileChooserView::CreateSigninErrorView() {
  LoginUIService* login_ui_service =
      LoginUIServiceFactory::GetForProfile(browser_->profile());
  base::string16 last_login_result(login_ui_service->GetLastLoginResult());
  return CreateTutorialView(
      profiles::TUTORIAL_MODE_SHOW_ERROR,
      l10n_util::GetStringUTF16(IDS_PROFILES_ERROR_TUTORIAL_TITLE),
      last_login_result,
      l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_TUTORIAL_LEARN_MORE),
      base::string16(),
      false /* stack_button */,
      &tutorial_learn_more_link_,
      NULL,
      &tutorial_close_button_);
}

views::View* ProfileChooserView::CreateRightClickTutorialView() {
    return CreateTutorialView(
      profiles::TUTORIAL_MODE_RIGHT_CLICK_SWITCHING,
      l10n_util::GetStringUTF16(IDS_PROFILES_RIGHT_CLICK_TUTORIAL_TITLE),
      l10n_util::GetStringUTF16(IDS_PROFILES_RIGHT_CLICK_TUTORIAL_CONTENT_TEXT),
      base::string16(),
      l10n_util::GetStringUTF16(IDS_PROFILES_TUTORIAL_OK_BUTTON),
      false,
      nullptr,
      &tutorial_sync_settings_ok_button_,
      nullptr);
}

views::View* ProfileChooserView::CreateSwitchUserView() {
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
  const AvatarMenu::Item& avatar_item =
      avatar_menu_->GetItemAt(avatar_menu_->GetActiveProfileIndex());
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

  const int kIconSize = 24;
  add_person_button_ = new BackgroundColorHoverButton(
      this, l10n_util::GetStringUTF16(IDS_PROFILES_ADD_PERSON_BUTTON),
      gfx::CreateVectorIcon(gfx::VectorIconId::ACCOUNT_BOX, kIconSize,
                            gfx::kChromeIconGrey));
  layout->StartRow(1, 0);
  layout->AddView(add_person_button_);

  // Adds "Disconnect your Google Account" button.
  layout->StartRow(1, 0);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

  disconnect_button_ = new BackgroundColorHoverButton(
      this, l10n_util::GetStringUTF16(IDS_PROFILES_DISCONNECT_BUTTON),
      gfx::CreateVectorIcon(gfx::VectorIconId::REMOVE_BOX, kIconSize,
                            gfx::kChromeIconGrey));
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
