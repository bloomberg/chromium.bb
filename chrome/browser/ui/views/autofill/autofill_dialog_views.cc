// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_dialog_views.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_dialog_sign_in_delegate.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/multi_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

using web_modal::WebContentsModalDialogManager;

namespace autofill {

namespace {

// The minimum useful height of the contents area of the dialog.
const int kMinimumContentsHeight = 100;

// Horizontal padding between text and other elements (in pixels).
const int kAroundTextPadding = 4;

// Padding around icons inside DecoratedTextfields.
const size_t kTextfieldIconPadding = 3;

// Size of the triangular mark that indicates an invalid textfield (in pixels).
const size_t kDogEarSize = 10;

// The space between the edges of a notification bar and the text within (in
// pixels).
const size_t kNotificationPadding = 14;

// Vertical padding above and below each detail section (in pixels).
const size_t kDetailSectionInset = 10;

const size_t kAutocheckoutStepsAreaPadding = 28;
const size_t kAutocheckoutStepInset = 20;

const size_t kAutocheckoutProgressBarWidth = 375;
const size_t kAutocheckoutProgressBarHeight = 15;

const size_t kArrowHeight = 7;
const size_t kArrowWidth = 2 * kArrowHeight;

// The padding around the edges of the legal documents text, in pixels.
const size_t kLegalDocPadding = 20;

// Slight shading for mouse hover and legal document background.
SkColor kShadingColor = SkColorSetARGB(7, 0, 0, 0);

// A border color for the legal document view.
SkColor kSubtleBorderColor = SkColorSetARGB(10, 0, 0, 0);

// The top padding, in pixels, for the suggestions menu dropdown arrows.
const size_t kMenuButtonTopOffset = 5;

// The side padding, in pixels, for the suggestions menu dropdown arrows.
const size_t kMenuButtonHorizontalPadding = 20;

const char kDecoratedTextfieldClassName[] = "autofill/DecoratedTextfield";
const char kNotificationAreaClassName[] = "autofill/NotificationArea";

views::Border* CreateLabelAlignmentBorder() {
  // TODO(estade): this should be made to match the native textfield top
  // inset. It's hard to get at, so for now it's hard-coded.
  return views::Border::CreateEmptyBorder(4, 0, 0, 0);
}

// Returns a label that describes a details section.
views::Label* CreateDetailsSectionLabel(const string16& text) {
  views::Label* label = new views::Label(text);
  label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  label->SetFont(label->font().DeriveFont(0, gfx::Font::BOLD));
  label->set_border(CreateLabelAlignmentBorder());
  return label;
}

// Draws an arrow at the top of |canvas| pointing to |tip_x|.
void DrawArrow(gfx::Canvas* canvas, int tip_x, const SkColor& color) {
  const int arrow_half_width = kArrowWidth / 2.0f;
  const int arrow_middle = tip_x - arrow_half_width;

  SkPath arrow;
  arrow.moveTo(arrow_middle - arrow_half_width, kArrowHeight);
  arrow.lineTo(arrow_middle + arrow_half_width, kArrowHeight);
  arrow.lineTo(arrow_middle, 0);
  arrow.close();
  canvas->ClipPath(arrow);
  canvas->DrawColor(color);
}

typedef ui::MultiAnimation::Part Part;
typedef ui::MultiAnimation::Parts Parts;

class OverlayView : public views::View,
                    public ui::AnimationDelegate {
 public:
  OverlayView() {
    SetLayoutManager(new views::FillLayout());

    set_background(views::Background::CreateSolidBackground(GetNativeTheme()->
        GetSystemColor(ui::NativeTheme::kColorId_DialogBackground)));

    Parts parts;
    // For this part of the animation, simply show the splash image.
    parts.push_back(Part(kSplashDisplayDurationMs, ui::Tween::ZERO));
    // For this part of the animation, fade out the splash image.
    parts.push_back(Part(kSplashFadeOutDurationMs, ui::Tween::EASE_IN));
    // For this part of the animation, fade out |this| (fade in the dialog).
    parts.push_back(Part(kSplashFadeInDialogDurationMs, ui::Tween::EASE_OUT));
    fade_out_.reset(
        new ui::MultiAnimation(parts,
                               ui::MultiAnimation::GetDefaultTimerInterval()));
    fade_out_->set_delegate(this);
    fade_out_->set_continuous(false);
    fade_out_->Start();
  }

  virtual ~OverlayView() {}

  // ui::AnimationDelegate implementation:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE {
    DCHECK_EQ(animation, fade_out_.get());
    if (fade_out_->current_part_index() != 0)
      SchedulePaint();
  }

  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE {
    DCHECK_EQ(animation, fade_out_.get());
    SetVisible(false);
  }

  // views::View implementation:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    // BubbleFrameView doesn't mask the window, it just draws the border via
    // image assets. Match that rounding here.
    static const SkScalar kCornerRadius = SkIntToScalar(2);
    gfx::Rect rect =
        GetWidget()->non_client_view()->frame_view()->GetLocalBounds();
    rect.Inset(12, 12, 12, 12);
    gfx::Path window_mask;
    window_mask.addRoundRect(gfx::RectToSkRect(rect),
                             kCornerRadius, kCornerRadius);
    canvas->ClipPath(window_mask);

    if (fade_out_->current_part_index() == 2) {
      canvas->SaveLayerAlpha((1 - fade_out_->GetCurrentValue()) * 255);
      views::View::OnPaint(canvas);
      canvas->Restore();
    } else {
      views::View::OnPaint(canvas);
    }
  }

  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE {
    if (fade_out_->current_part_index() == 0) {
      views::View::PaintChildren(canvas);
    } else if (fade_out_->current_part_index() == 1) {
      canvas->SaveLayerAlpha((1 - fade_out_->GetCurrentValue()) * 255);
      views::View::PaintChildren(canvas);
      canvas->Restore();
    }
  }

 private:
  // This MultiAnimation is used to first fade out the contents of the overlay,
  // then fade out the background of the overlay (revealing the dialog behind
  // the overlay). This avoids cross-fade.
  scoped_ptr<ui::MultiAnimation> fade_out_;

  DISALLOW_COPY_AND_ASSIGN(OverlayView);
};

// This class handles layout for the first row of a SuggestionView.
// It exists to circumvent shortcomings of GridLayout and BoxLayout (namely that
// the former doesn't fully respect child visibility, and that the latter won't
// expand a single child).
class SectionRowView : public views::View {
 public:
  SectionRowView() {}
  virtual ~SectionRowView() {}

  // views::View implementation:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    // Only the height matters anyway.
    return child_at(2)->GetPreferredSize();
  }

  virtual void Layout() OVERRIDE {
    const gfx::Rect bounds = GetContentsBounds();

    // Icon is left aligned.
    int start_x = bounds.x();
    views::View* icon = child_at(0);
    if (icon->visible()) {
      icon->SizeToPreferredSize();
      icon->SetX(start_x);
      icon->SetY(bounds.y() +
          (bounds.height() - icon->bounds().height()) / 2);
      start_x += icon->bounds().width() + kAroundTextPadding;
    }

    // Textfield is right aligned.
    int end_x = bounds.width();
    views::View* decorated = child_at(2);
    if (decorated->visible()) {
      decorated->SizeToPreferredSize();
      decorated->SetX(bounds.width() - decorated->bounds().width());
      decorated->SetY(bounds.y());
      end_x = decorated->bounds().x() - kAroundTextPadding;
    }

    // Label takes up all the space in between.
    views::View* label = child_at(1);
    if (label->visible())
      label->SetBounds(start_x, bounds.y(), end_x - start_x, bounds.height());

    views::View::Layout();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SectionRowView);
};

// This view is used for the contents of the error bubble widget.
class ErrorBubbleContents : public views::View {
 public:
  explicit ErrorBubbleContents(const string16& message)
      : color_(kWarningColor) {
    set_border(views::Border::CreateEmptyBorder(kArrowHeight, 0, 0, 0));

    views::Label* label = new views::Label();
    label->SetText(message);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetEnabledColor(SK_ColorWHITE);
    label->set_border(
        views::Border::CreateSolidSidedBorder(5, 10, 5, 10, color_));
    label->set_background(
        views::Background::CreateSolidBackground(color_));
    SetLayoutManager(new views::FillLayout());
    AddChildView(label);
  }
  virtual ~ErrorBubbleContents() {}

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    views::View::OnPaint(canvas);
    DrawArrow(canvas, width() / 2.0f, color_);
  }

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(ErrorBubbleContents);
};

// A view that runs a callback whenever its bounds change.
class DetailsContainerView : public views::View {
 public:
  explicit DetailsContainerView(const base::Closure& callback)
      : bounds_changed_callback_(callback) {}
  virtual ~DetailsContainerView() {}

  // views::View implementation.
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE {
    bounds_changed_callback_.Run();
  }

 private:
  base::Closure bounds_changed_callback_;

  DISALLOW_COPY_AND_ASSIGN(DetailsContainerView);
};

// A view that propagates visibility and preferred size changes.
class LayoutPropagationView : public views::View {
 public:
  LayoutPropagationView() {}
  virtual ~LayoutPropagationView() {}

 protected:
  virtual void ChildVisibilityChanged(views::View* child) OVERRIDE {
    PreferredSizeChanged();
  }
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE {
    PreferredSizeChanged();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LayoutPropagationView);
};

// A class which displays the status of an individual step in an
// Autocheckout flow.
class AutocheckoutStepProgressView : public views::View {
 public:
  AutocheckoutStepProgressView(const string16& description,
                               const gfx::Font& font,
                               const SkColor color,
                               const bool is_icon_visible) {
    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);
    const int kColumnSetId = 0;
    views::ColumnSet* columns = layout->AddColumnSet(kColumnSetId);
    columns->AddColumn(views::GridLayout::LEADING,
                       views::GridLayout::LEADING,
                       0,
                       views::GridLayout::USE_PREF,
                       0,
                       0);
    columns->AddPaddingColumn(0, 8);
    columns->AddColumn(views::GridLayout::LEADING,
                       views::GridLayout::LEADING,
                       0,
                       views::GridLayout::USE_PREF,
                       0,
                       0);
    layout->StartRow(0, kColumnSetId);
    views::Label* label = new views::Label();
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->set_border(views::Border::CreateEmptyBorder(0, 0, 0, 0));
    label->SetText(description);
    label->SetFont(font);
    label->SetEnabledColor(color);

    views::ImageView* icon = new views::ImageView();
    icon->SetVisible(is_icon_visible);
    icon->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_WALLET_STEP_CHECK).ToImageSkia());

    layout->AddView(icon);
    layout->AddView(label);
  }

  virtual ~AutocheckoutStepProgressView() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AutocheckoutStepProgressView);
};

}  // namespace

// AutofillDialogViews::SizeLimitedScrollView ----------------------------------

AutofillDialogViews::SizeLimitedScrollView::SizeLimitedScrollView(
    views::View* scroll_contents)
    : max_height_(-1) {
  set_hide_horizontal_scrollbar(true);
  SetContents(scroll_contents);
}

AutofillDialogViews::SizeLimitedScrollView::~SizeLimitedScrollView() {}

void AutofillDialogViews::SizeLimitedScrollView::Layout() {
  contents()->SizeToPreferredSize();
  ScrollView::Layout();
}

gfx::Size AutofillDialogViews::SizeLimitedScrollView::GetPreferredSize() {
  gfx::Size size = contents()->GetPreferredSize();
  if (contents()->visible()) {
    if (max_height_ >= 0 && max_height_ < size.height())
      size.set_height(max_height_);
  } else {
    // When we hide detailed steps we want to remove the excess vertical
    // space left in their absence, but keep the width so that the dialog
    // doesn't appear too 'elastic' in its dimensions.
    size.set_height(0);
  }

  return size;
}

void AutofillDialogViews::SizeLimitedScrollView::SetMaximumHeight(
    int max_height) {
  int old_max = max_height_;
  max_height_ = max_height;

  if (max_height_ < height() || old_max <= height())
    PreferredSizeChanged();
}

// AutofillDialogViews::ErrorBubble --------------------------------------------

AutofillDialogViews::ErrorBubble::ErrorBubble(views::View* anchor,
                                              const string16& message)
    : anchor_(anchor),
      contents_(new ErrorBubbleContents(message)),
      observer_(this) {
  widget_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.transparent = true;
  views::Widget* anchor_widget = anchor->GetWidget();
  DCHECK(anchor_widget);
  params.parent = anchor_widget->GetNativeView();

  widget_->Init(params);
  widget_->SetContentsView(contents_);
  UpdatePosition();
  observer_.Add(widget_);
}

AutofillDialogViews::ErrorBubble::~ErrorBubble() {
  if (widget_)
    widget_->Close();
}

bool AutofillDialogViews::ErrorBubble::IsShowing() {
  return widget_ && widget_->IsVisible();
}

void AutofillDialogViews::ErrorBubble::UpdatePosition() {
  if (!widget_)
    return;

  if (!anchor_->GetVisibleBounds().IsEmpty()) {
    widget_->SetBounds(GetBoundsForWidget());
    widget_->SetVisibilityChangedAnimationsEnabled(true);
    widget_->Show();
  } else {
    widget_->SetVisibilityChangedAnimationsEnabled(false);
    widget_->Hide();
  }
}

void AutofillDialogViews::ErrorBubble::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(widget_, widget);
  observer_.Remove(widget_);
  widget_ = NULL;
}

gfx::Rect AutofillDialogViews::ErrorBubble::GetBoundsForWidget() {
  gfx::Rect anchor_bounds = anchor_->GetBoundsInScreen();
  gfx::Rect bubble_bounds;
  bubble_bounds.set_size(contents_->GetPreferredSize());
  bubble_bounds.set_x(anchor_bounds.right() -
      (anchor_bounds.width() + bubble_bounds.width()) / 2);
  const int kErrorBubbleOverlap = 3;
  bubble_bounds.set_y(anchor_bounds.bottom() - kErrorBubbleOverlap);

  return bubble_bounds;
}

// AutofillDialogViews::DecoratedTextfield -------------------------------------

AutofillDialogViews::DecoratedTextfield::DecoratedTextfield(
    const string16& default_value,
    const string16& placeholder,
    views::TextfieldController* controller)
    : border_(new views::FocusableBorder()),
      invalid_(false) {
  set_border(border_);
  // Removes the border from |native_wrapper_|.
  RemoveBorder();
  set_placeholder_text(placeholder);
  SetText(default_value);
  SetController(controller);
  SetHorizontalMargins(0, 0);
}

AutofillDialogViews::DecoratedTextfield::~DecoratedTextfield() {}

void AutofillDialogViews::DecoratedTextfield::SetInvalid(bool invalid) {
  invalid_ = invalid;
  if (invalid)
    border_->SetColor(kWarningColor);
  else
    border_->UseDefaultColor();
  SchedulePaint();
}

void AutofillDialogViews::DecoratedTextfield::SetIcon(const gfx::Image& icon) {
  int icon_space = icon.IsEmpty() ? 0 :
                                    icon.Width() + 2 * kTextfieldIconPadding;
  int left = base::i18n::IsRTL() ? icon_space : 0;
  int right = base::i18n::IsRTL() ? 0 : icon_space;
  SetHorizontalMargins(left, right);
  icon_ = icon;

  PreferredSizeChanged();
  SchedulePaint();
}

const char* AutofillDialogViews::DecoratedTextfield::GetClassName() const {
  return kDecoratedTextfieldClassName;
}

void AutofillDialogViews::DecoratedTextfield::PaintChildren(
    gfx::Canvas* canvas) {}

void AutofillDialogViews::DecoratedTextfield::OnPaint(gfx::Canvas* canvas) {
  // Draw the textfield first.
  views::View::PaintChildren(canvas);

  border_->set_has_focus(HasFocus());
  OnPaintBorder(canvas);

  if (!icon_.IsEmpty()) {
    gfx::Rect bounds = GetContentsBounds();
    int x = base::i18n::IsRTL() ?
        kTextfieldIconPadding :
        bounds.right() - icon_.Width() - kTextfieldIconPadding;
    canvas->DrawImageInt(icon_.AsImageSkia(), x,
                         bounds.y() + (bounds.height() - icon_.Height()) / 2);
  }

  // Then draw extra stuff on top.
  if (invalid_) {
    if (base::i18n::IsRTL()) {
      canvas->Translate(gfx::Vector2d(width(), 0));
      canvas->Scale(-1, 1);
    }

    SkPath dog_ear;
    dog_ear.moveTo(width() - kDogEarSize, 0);
    dog_ear.lineTo(width(), 0);
    dog_ear.lineTo(width(), kDogEarSize);
    dog_ear.close();
    canvas->ClipPath(dog_ear);
    canvas->DrawColor(kWarningColor);
  }
}

// AutofillDialogViews::AccountChooser -----------------------------------------

AutofillDialogViews::AccountChooser::AccountChooser(
    AutofillDialogController* controller)
    : image_(new views::ImageView()),
      label_(new views::Label()),
      arrow_(new views::ImageView()),
      link_(new views::Link()),
      controller_(controller) {
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                           kAroundTextPadding));
  AddChildView(image_);
  AddChildView(label_);

  arrow_->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_MENU_DROPARROW).ToImageSkia());
  AddChildView(arrow_);

  link_->set_listener(this);
  AddChildView(link_);
}

AutofillDialogViews::AccountChooser::~AccountChooser() {}

void AutofillDialogViews::AccountChooser::Update() {
  gfx::Image icon = controller_->AccountChooserImage();
  image_->SetImage(icon.AsImageSkia());
  label_->SetText(controller_->AccountChooserText());

  bool show_link = !controller_->MenuModelForAccountChooser();
  label_->SetVisible(!show_link);
  arrow_->SetVisible(!show_link);
  link_->SetText(controller_->SignInLinkText());
  link_->SetVisible(show_link);

  menu_runner_.reset();

  PreferredSizeChanged();
}

bool AutofillDialogViews::AccountChooser::OnMousePressed(
    const ui::MouseEvent& event) {
  // Return true so we get the release event.
  if (controller_->MenuModelForAccountChooser())
    return event.IsOnlyLeftMouseButton();

  return false;
}

void AutofillDialogViews::AccountChooser::OnMouseReleased(
    const ui::MouseEvent& event) {
  if (!HitTestPoint(event.location()))
    return;

  ui::MenuModel* model = controller_->MenuModelForAccountChooser();
  if (!model)
    return;

  menu_runner_.reset(new views::MenuRunner(model));
  ignore_result(
      menu_runner_->RunMenuAt(GetWidget(),
                              NULL,
                              GetBoundsInScreen(),
                              views::MenuItemView::TOPRIGHT,
                              ui::MENU_SOURCE_MOUSE,
                              0));
}

void AutofillDialogViews::AccountChooser::LinkClicked(views::Link* source,
                                                      int event_flags) {
  controller_->SignInLinkClicked();
}

// AutofillDialogViews::ShieldableContentsView ---------------------------------

AutofillDialogViews::ShieldableContentsView::ShieldableContentsView()
    : contents_(new views::View),
      contents_shield_(new views::Label) {
  contents_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                           views::kRelatedControlVerticalSpacing));
  contents_shield_->SetVisible(false);
  contents_shield_->set_background(views::Background::CreateSolidBackground(
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_DialogBackground)));
  contents_shield_->SetFont(ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::BaseFont).DeriveFont(15));

  AddChildView(contents_);
  AddChildView(contents_shield_);
}

AutofillDialogViews::ShieldableContentsView::~ShieldableContentsView() {}

void AutofillDialogViews::ShieldableContentsView::AddToContents(
    views::View* view) {
  contents_->AddChildView(view);
}

void AutofillDialogViews::ShieldableContentsView::ObscureContents(
    const base::string16& message) {
  if (message == contents_shield_->text())
    return;

  contents_shield_->SetVisible(!message.empty());
  contents_shield_->SetText(message);
  SchedulePaint();
}

void AutofillDialogViews::ShieldableContentsView::Layout() {
  contents_->SetBoundsRect(bounds());
  contents_shield_->SetBoundsRect(bounds());
}

gfx::Size AutofillDialogViews::ShieldableContentsView::GetPreferredSize() {
  return contents_->GetPreferredSize();
}

// AutofillDialogViews::NotificationArea ---------------------------------------

AutofillDialogViews::NotificationArea::NotificationArea(
    AutofillDialogController* controller)
    : controller_(controller),
      checkbox_(NULL) {
  // Reserve vertical space for the arrow (regardless of whether one exists).
  set_border(views::Border::CreateEmptyBorder(kArrowHeight, 0, 0, 0));

  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
  SetLayoutManager(box_layout);
}

AutofillDialogViews::NotificationArea::~NotificationArea() {}

void AutofillDialogViews::NotificationArea::SetNotifications(
    const std::vector<DialogNotification>& notifications) {
  notifications_ = notifications;

  RemoveAllChildViews(true);
  checkbox_ = NULL;

  if (notifications_.empty())
    return;

  for (size_t i = 0; i < notifications_.size(); ++i) {
    const DialogNotification& notification = notifications_[i];

    scoped_ptr<views::View> view;
    if (notification.HasCheckbox()) {
      scoped_ptr<views::Checkbox> checkbox(new views::Checkbox(string16()));
      checkbox_ = checkbox.get();
      // We have to do this instead of using set_border() because a border
      // is being used to draw the check square.
      static_cast<views::LabelButtonBorder*>(checkbox->border())->
          set_insets(gfx::Insets(kNotificationPadding,
                                 kNotificationPadding,
                                 kNotificationPadding,
                                 kNotificationPadding));
      if (!notification.interactive())
        checkbox->SetState(views::Button::STATE_DISABLED);
      checkbox->SetText(notification.display_text());
      checkbox->SetTextMultiLine(true);
      checkbox->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      checkbox->SetTextColor(views::Button::STATE_NORMAL,
                             notification.GetTextColor());
      checkbox->SetTextColor(views::Button::STATE_HOVERED,
                             notification.GetTextColor());
      checkbox->SetChecked(notification.checked());
      checkbox->set_listener(this);
      view.reset(checkbox.release());
    } else {
      scoped_ptr<views::Label> label(new views::Label());
      label->SetText(notification.display_text());
      label->SetMultiLine(true);
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      label->SetAutoColorReadabilityEnabled(false);
      label->SetEnabledColor(notification.GetTextColor());
      label->set_border(views::Border::CreateSolidBorder(
          kNotificationPadding, notification.GetBackgroundColor()));
      view.reset(label.release());
    }

    view->set_background(views::Background::CreateSolidBackground(
        notification.GetBackgroundColor()));
    AddChildView(view.release());
  }

  PreferredSizeChanged();
}

gfx::Size AutofillDialogViews::NotificationArea::GetPreferredSize() {
  gfx::Size size = views::View::GetPreferredSize();
  // Ensure that long notifications wrap and don't enlarge the dialog.
  size.set_width(1);
  return size;
}

const char* AutofillDialogViews::NotificationArea::GetClassName() const {
  return kNotificationAreaClassName;
}

void AutofillDialogViews::NotificationArea::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  if (HasArrow()) {
    DrawArrow(
        canvas,
        GetMirroredXInView(width() - arrow_centering_anchor_->width() / 2.0f),
        notifications_[0].GetBackgroundColor());
  }
}

void AutofillDialogViews::OnWidgetClosing(views::Widget* widget) {
  observer_.Remove(widget);
}

void AutofillDialogViews::OnWidgetBoundsChanged(views::Widget* widget,
                                                const gfx::Rect& new_bounds) {
  int non_scrollable_height = window_->GetContentsView()->bounds().height() -
      scrollable_area_->bounds().height();
  int browser_window_height = widget->GetContentsView()->bounds().height();

  scrollable_area_->SetMaximumHeight(
      std::max(kMinimumContentsHeight,
               (browser_window_height - non_scrollable_height) * 8 / 10));
  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::NotificationArea::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  DCHECK_EQ(sender, checkbox_);
  controller_->NotificationCheckboxStateChanged(notifications_.front().type(),
                                                checkbox_->checked());
}

bool AutofillDialogViews::NotificationArea::HasArrow() {
  return !notifications_.empty() && notifications_[0].HasArrow() &&
      arrow_centering_anchor_.get();
}

// AutofillDialogViews::SectionContainer ---------------------------------------

AutofillDialogViews::SectionContainer::SectionContainer(
    const string16& label,
    views::View* controls,
    views::Button* proxy_button)
    : proxy_button_(proxy_button),
      forward_mouse_events_(false) {
  set_notify_enter_exit_on_child(true);

  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(kDetailSectionInset, 0, kDetailSectionInset, 0);
  SetLayoutManager(layout);

  const int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  // TODO(estade): pull out these constants, and figure out better values
  // for them.
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        180,
                        0);
  column_set->AddPaddingColumn(0, 30);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        300,
                        0);

  layout->StartRow(0, kColumnSetId);
  layout->AddView(CreateDetailsSectionLabel(label));
  layout->AddView(controls);
}

AutofillDialogViews::SectionContainer::~SectionContainer() {}

void AutofillDialogViews::SectionContainer::SetActive(bool active) {
  bool is_active = active && proxy_button_->visible();
  if (is_active == !!background())
    return;

  set_background(is_active ?
      views::Background::CreateSolidBackground(kShadingColor) :
      NULL);
  SchedulePaint();
}

void AutofillDialogViews::SectionContainer::SetForwardMouseEvents(
    bool forward) {
  forward_mouse_events_ = forward;
  if (!forward)
    set_background(NULL);
}

void AutofillDialogViews::SectionContainer::OnMouseMoved(
    const ui::MouseEvent& event) {
  if (!forward_mouse_events_)
    return;

  SetActive(true);
}

void AutofillDialogViews::SectionContainer::OnMouseEntered(
    const ui::MouseEvent& event) {
  if (!forward_mouse_events_)
    return;

  SetActive(true);
  proxy_button_->OnMouseEntered(ProxyEvent(event));
  SchedulePaint();
}

void AutofillDialogViews::SectionContainer::OnMouseExited(
    const ui::MouseEvent& event) {
  if (!forward_mouse_events_)
    return;

  SetActive(false);
  proxy_button_->OnMouseExited(ProxyEvent(event));
  SchedulePaint();
}

bool AutofillDialogViews::SectionContainer::OnMousePressed(
    const ui::MouseEvent& event) {
  if (!forward_mouse_events_)
    return false;

  return proxy_button_->OnMousePressed(ProxyEvent(event));
}

void AutofillDialogViews::SectionContainer::OnMouseReleased(
    const ui::MouseEvent& event) {
  if (!forward_mouse_events_)
    return;

  proxy_button_->OnMouseReleased(ProxyEvent(event));
}

// static
ui::MouseEvent AutofillDialogViews::SectionContainer::ProxyEvent(
    const ui::MouseEvent& event) {
  ui::MouseEvent event_copy = event;
  event_copy.set_location(gfx::Point());
  return event_copy;
}

// AutofilDialogViews::SuggestionView ------------------------------------------

AutofillDialogViews::SuggestionView::SuggestionView(
    const string16& edit_label,
    AutofillDialogViews* autofill_dialog)
    : label_(new views::Label()),
      label_line_2_(new views::Label()),
      icon_(new views::ImageView()),
      label_container_(new SectionRowView()),
      decorated_(
          new DecoratedTextfield(string16(), string16(), autofill_dialog)) {
  // Label and icon.
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->set_border(CreateLabelAlignmentBorder());
  label_container_->AddChildView(icon_);
  label_container_->AddChildView(label_);
  label_container_->AddChildView(decorated_);
  decorated_->SetVisible(false);
  // TODO(estade): get the sizing and spacing right on this textfield.
  decorated_->set_default_width_in_chars(10);
  AddChildView(label_container_);

  label_line_2_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_line_2_->SetVisible(false);
  label_line_2_->SetMultiLine(true);
  AddChildView(label_line_2_);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
}

AutofillDialogViews::SuggestionView::~SuggestionView() {}

void AutofillDialogViews::SuggestionView::SetSuggestionText(
    const string16& text,
    gfx::Font::FontStyle text_style) {
  label_->SetFont(ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::BaseFont).DeriveFont(0, text_style));

  // TODO(estade): does this localize well?
  string16 line_return(ASCIIToUTF16("\n"));
  size_t position = text.find(line_return);
  if (position == string16::npos) {
    label_->SetText(text);
    label_line_2_->SetVisible(false);
  } else {
    label_->SetText(text.substr(0, position));
    label_line_2_->SetText(text.substr(position + line_return.length()));
    label_line_2_->SetVisible(true);
  }
}

void AutofillDialogViews::SuggestionView::SetSuggestionIcon(
    const gfx::Image& image) {
  icon_->SetVisible(!image.IsEmpty());
  icon_->SetImage(image.AsImageSkia());
}

void AutofillDialogViews::SuggestionView::ShowTextfield(
    const string16& placeholder_text,
    const gfx::Image& icon) {
  decorated_->set_placeholder_text(placeholder_text);
  decorated_->SetIcon(icon);
  decorated_->SetVisible(true);
  // The textfield will increase the height of the first row and cause the
  // label to be aligned properly, so the border is not necessary.
  label_->set_border(NULL);
}

// AutofillDialogViews::AutocheckoutStepsArea ---------------------------------

AutofillDialogViews::AutocheckoutStepsArea::AutocheckoutStepsArea() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        kAutocheckoutStepsAreaPadding,
                                        0,
                                        kAutocheckoutStepInset));
}

void AutofillDialogViews::AutocheckoutStepsArea::SetSteps(
    const std::vector<DialogAutocheckoutStep>& steps) {
  RemoveAllChildViews(true);
  for (size_t i = 0; i < steps.size(); ++i) {
    const DialogAutocheckoutStep& step = steps[i];
    AutocheckoutStepProgressView* progressView =
        new AutocheckoutStepProgressView(step.GetDisplayText(),
                                         step.GetTextFont(),
                                         step.GetTextColor(),
                                         step.IsIconVisible());

    AddChildView(progressView);
  }

  PreferredSizeChanged();
}

// AutofillDialogViews::AutocheckoutProgressBar

AutofillDialogViews::AutocheckoutProgressBar::AutocheckoutProgressBar() {}
AutofillDialogViews::AutocheckoutProgressBar::~AutocheckoutProgressBar() {}

gfx::Size AutofillDialogViews::AutocheckoutProgressBar::GetPreferredSize() {
  return gfx::Size(kAutocheckoutProgressBarWidth,
                   kAutocheckoutProgressBarHeight);
}

// AutofillDialogView ----------------------------------------------------------

// static
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return new AutofillDialogViews(controller);
}

// AutofillDialogViews ---------------------------------------------------------

AutofillDialogViews::AutofillDialogViews(AutofillDialogController* controller)
    : controller_(controller),
      window_(NULL),
      contents_(NULL),
      notification_area_(NULL),
      account_chooser_(NULL),
      sign_in_webview_(NULL),
      main_container_(NULL),
      scrollable_area_(NULL),
      details_container_(NULL),
      overlay_view_(NULL),
      button_strip_extra_view_(NULL),
      save_in_chrome_checkbox_(NULL),
      autocheckout_steps_area_(NULL),
      autocheckout_progress_bar_view_(NULL),
      autocheckout_progress_bar_(NULL),
      footnote_view_(NULL),
      legal_document_view_(NULL),
      focus_manager_(NULL),
      observer_(this) {
  DCHECK(controller);
  detail_groups_.insert(std::make_pair(SECTION_EMAIL,
                                       DetailsGroup(SECTION_EMAIL)));
  detail_groups_.insert(std::make_pair(SECTION_CC,
                                       DetailsGroup(SECTION_CC)));
  detail_groups_.insert(std::make_pair(SECTION_BILLING,
                                       DetailsGroup(SECTION_BILLING)));
  detail_groups_.insert(std::make_pair(SECTION_CC_BILLING,
                                       DetailsGroup(SECTION_CC_BILLING)));
  detail_groups_.insert(std::make_pair(SECTION_SHIPPING,
                                       DetailsGroup(SECTION_SHIPPING)));
}

AutofillDialogViews::~AutofillDialogViews() {
  DCHECK(!window_);
}

void AutofillDialogViews::Show() {
  InitChildViews();
  UpdateAccountChooser();
  UpdateNotificationArea();
  UpdateSaveInChromeCheckbox();

  // Ownership of |contents_| is handed off by this call. The widget will take
  // care of deleting itself after calling DeleteDelegate().
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(
          controller_->web_contents());
  window_ = CreateWebContentsModalDialogViews(
      this,
      controller_->web_contents()->GetView()->GetNativeView(),
      web_contents_modal_dialog_manager->delegate()->
          GetWebContentsModalDialogHost());
  web_contents_modal_dialog_manager->ShowDialog(window_->GetNativeView());
  focus_manager_ = window_->GetFocusManager();
  focus_manager_->AddFocusChangeListener(this);

#if defined(OS_WIN) && !defined(USE_AURA)
  // On non-Aura Windows a standard accelerator gets registered that will
  // navigate on a backspace. Override that here.
  // TODO(abodenha): Remove this when no longer needed. See
  // http://crbug.com/242584.
  ui::Accelerator backspace(ui::VKEY_BACK, ui::EF_NONE);
  focus_manager_->RegisterAccelerator(
      backspace, ui::AcceleratorManager::kNormalPriority, this);
#endif

  // Listen for size changes on the browser.
  views::Widget* browser_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          controller_->web_contents()->GetView()->GetNativeView());
  observer_.Add(browser_widget);
  OnWidgetBoundsChanged(browser_widget, gfx::Rect());
}

void AutofillDialogViews::Hide() {
  if (window_)
    window_->Close();
}

void AutofillDialogViews::UpdateAccountChooser() {
  account_chooser_->Update();
  // TODO(estade): replace this with a better loading image/animation.
  // See http://crbug.com/230932
  main_container_->ObscureContents(controller_->ShouldShowSpinner() ?
      ASCIIToUTF16("Loading...") : base::string16());

  // Update legal documents for the account.
  if (footnote_view_) {
    const string16 text = controller_->LegalDocumentsText();
    legal_document_view_->SetText(text);

    if (!text.empty()) {
      const std::vector<ui::Range>& link_ranges =
          controller_->LegalDocumentLinks();
      for (size_t i = 0; i < link_ranges.size(); ++i) {
        legal_document_view_->AddStyleRange(
            link_ranges[i],
            views::StyledLabel::RangeStyleInfo::CreateForLink());
      }
    }

    footnote_view_->SetVisible(!text.empty());
    ContentsPreferredSizeChanged();
  }
}

void AutofillDialogViews::UpdateAutocheckoutStepsArea() {
  autocheckout_steps_area_->SetSteps(controller_->CurrentAutocheckoutSteps());
  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::UpdateButtonStrip() {
  button_strip_extra_view_->SetVisible(
      GetDialogButtons() != ui::DIALOG_BUTTON_NONE);
  UpdateSaveInChromeCheckbox();
  autocheckout_progress_bar_view_->SetVisible(
      controller_->ShouldShowProgressBar());
  GetDialogClientView()->UpdateDialogButtons();
  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::UpdateDetailArea() {
  details_container_->SetVisible(controller_->ShouldShowDetailArea());
  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::UpdateForErrors() {
  ValidateForm();
}

void AutofillDialogViews::UpdateNotificationArea() {
  DCHECK(notification_area_);
  notification_area_->SetNotifications(controller_->CurrentNotifications());
  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::UpdateSection(DialogSection section) {
  UpdateSectionImpl(section, true);
}

void AutofillDialogViews::FillSection(DialogSection section,
                                      const DetailInput& originating_input) {
  DetailsGroup* group = GroupForSection(section);
  // Make sure to overwrite the originating input.
  TextfieldMap::iterator text_mapping =
      group->textfields.find(&originating_input);
  if (text_mapping != group->textfields.end())
    text_mapping->second->SetText(string16());

  // If the Autofill data comes from a credit card, make sure to overwrite the
  // CC comboboxes (even if they already have something in them). If the
  // Autofill data comes from an AutofillProfile, leave the comboboxes alone.
  if ((section == SECTION_CC || section == SECTION_CC_BILLING) &&
      AutofillType(originating_input.type).group() ==
              AutofillType::CREDIT_CARD) {
    for (ComboboxMap::const_iterator it = group->comboboxes.begin();
         it != group->comboboxes.end(); ++it) {
      if (AutofillType(it->first->type).group() == AutofillType::CREDIT_CARD)
        it->second->SetSelectedIndex(it->second->model()->GetDefaultIndex());
    }
  }

  UpdateSectionImpl(section, false);
}

void AutofillDialogViews::GetUserInput(DialogSection section,
                                       DetailOutputMap* output) {
  DetailsGroup* group = GroupForSection(section);
  for (TextfieldMap::const_iterator it = group->textfields.begin();
       it != group->textfields.end(); ++it) {
    output->insert(std::make_pair(it->first, it->second->text()));
  }
  for (ComboboxMap::const_iterator it = group->comboboxes.begin();
       it != group->comboboxes.end(); ++it) {
    output->insert(std::make_pair(it->first,
        it->second->model()->GetItemAt(it->second->selected_index())));
  }
}

string16 AutofillDialogViews::GetCvc() {
  DialogSection billing_section = controller_->SectionIsActive(SECTION_CC) ?
      SECTION_CC : SECTION_CC_BILLING;
  return GroupForSection(billing_section)->suggested_info->
      decorated_textfield()->text();
}

bool AutofillDialogViews::SaveDetailsLocally() {
  DCHECK(save_in_chrome_checkbox_->visible());
  return save_in_chrome_checkbox_->checked();
}

const content::NavigationController* AutofillDialogViews::ShowSignIn() {
  // TODO(abodenha) We should be able to use the WebContents of the WebView
  // to navigate instead of LoadInitialURL.  Figure out why it doesn't work.

  sign_in_webview_->LoadInitialURL(wallet::GetSignInUrl());

  main_container_->SetVisible(false);
  sign_in_webview_->SetVisible(true);
  UpdateButtonStrip();
  ContentsPreferredSizeChanged();
  return &sign_in_webview_->web_contents()->GetController();
}

void AutofillDialogViews::HideSignIn() {
  sign_in_webview_->SetVisible(false);
  main_container_->SetVisible(true);
  UpdateButtonStrip();
  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::UpdateProgressBar(double value) {
  autocheckout_progress_bar_->SetValue(value);
}

void AutofillDialogViews::ModelChanged() {
  menu_runner_.reset();

  for (DetailGroupMap::const_iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    UpdateDetailsGroupState(iter->second);
  }
}

TestableAutofillDialogView* AutofillDialogViews::GetTestableView() {
  return this;
}

void AutofillDialogViews::OnSignInResize(const gfx::Size& pref_size) {
  sign_in_webview_->SetPreferredSize(pref_size);
  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::SubmitForTesting() {
  Accept();
}

void AutofillDialogViews::CancelForTesting() {
  if (Cancel())
    Hide();
}

string16 AutofillDialogViews::GetTextContentsOfInput(const DetailInput& input) {
  views::Textfield* textfield = TextfieldForInput(input);
  if (textfield)
    return textfield->text();

  views::Combobox* combobox = ComboboxForInput(input);
  if (combobox)
    return combobox->model()->GetItemAt(combobox->selected_index());

  NOTREACHED();
  return string16();
}

void AutofillDialogViews::SetTextContentsOfInput(const DetailInput& input,
                                                 const string16& contents) {
  views::Textfield* textfield = TextfieldForInput(input);
  if (textfield) {
    TextfieldForInput(input)->SetText(contents);
    return;
  }

  views::Combobox* combobox = ComboboxForInput(input);
  if (combobox) {
    for (int i = 0; i < combobox->model()->GetItemCount(); ++i) {
      if (contents == combobox->model()->GetItemAt(i)) {
        combobox->SetSelectedIndex(i);
        return;
      }
    }
    // If we don't find a match, return the combobox to its default state.
    combobox->SetSelectedIndex(combobox->model()->GetDefaultIndex());
    return;
  }

  NOTREACHED();
}

void AutofillDialogViews::SetTextContentsOfSuggestionInput(
    DialogSection section,
    const base::string16& text) {
  GroupForSection(section)->suggested_info->decorated_textfield()->
      SetText(text);
}

void AutofillDialogViews::ActivateInput(const DetailInput& input) {
  TextfieldEditedOrActivated(TextfieldForInput(input), false);
}

gfx::Size AutofillDialogViews::GetSize() const {
  return GetWidget() ? GetWidget()->GetRootView()->size() : gfx::Size();
}

bool AutofillDialogViews::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  ui::KeyboardCode key = accelerator.key_code();
  if (key == ui::VKEY_BACK)
    return true;
  return false;
}

bool AutofillDialogViews::CanHandleAccelerators() const {
  return true;
}

string16 AutofillDialogViews::GetWindowTitle() const {
  return controller_->DialogTitle();
}

void AutofillDialogViews::WindowClosing() {
  focus_manager_->RemoveFocusChangeListener(this);
}

void AutofillDialogViews::DeleteDelegate() {
  window_ = NULL;
  // |this| belongs to |controller_|.
  controller_->ViewClosed();
}

views::Widget* AutofillDialogViews::GetWidget() {
  return contents_->GetWidget();
}

const views::Widget* AutofillDialogViews::GetWidget() const {
  return contents_->GetWidget();
}

views::View* AutofillDialogViews::GetContentsView() {
  return contents_;
}

int AutofillDialogViews::GetDialogButtons() const {
  if (sign_in_webview_->visible())
    return ui::DIALOG_BUTTON_NONE;

  return controller_->GetDialogButtons();
}

string16 AutofillDialogViews::GetDialogButtonLabel(ui::DialogButton button)
    const {
  return button == ui::DIALOG_BUTTON_OK ?
      controller_->ConfirmButtonText() : controller_->CancelButtonText();
}

bool AutofillDialogViews::IsDialogButtonEnabled(ui::DialogButton button) const {
  return controller_->IsDialogButtonEnabled(button);
}

views::View* AutofillDialogViews::CreateExtraView() {
  return button_strip_extra_view_;
}

views::View* AutofillDialogViews::CreateTitlebarExtraView() {
  return account_chooser_;
}

views::View* AutofillDialogViews::CreateFootnoteView() {
  footnote_view_ = new LayoutPropagationView();
  footnote_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical,
                           kLegalDocPadding,
                           kLegalDocPadding,
                           0));
  footnote_view_->set_border(
      views::Border::CreateSolidSidedBorder(1, 0, 0, 0, kSubtleBorderColor));
  footnote_view_->set_background(
      views::Background::CreateSolidBackground(kShadingColor));

  legal_document_view_ = new views::StyledLabel(string16(), this);
  footnote_view_->AddChildView(legal_document_view_);
  footnote_view_->SetVisible(false);

  return footnote_view_;
}

views::View* AutofillDialogViews::CreateOverlayView() {
  gfx::Image splash_image = controller_->SplashPageImage();
  if (splash_image.IsEmpty())
    return NULL;

  overlay_view_ = new OverlayView();
  views::ImageView* image = new views::ImageView();
  image->SetImage(splash_image.ToImageSkia());
  overlay_view_->AddChildView(image);

  return overlay_view_;
}

bool AutofillDialogViews::Cancel() {
  controller_->OnCancel();
  return true;
}

bool AutofillDialogViews::Accept() {
  if (ValidateForm())
    controller_->OnAccept();
  else if (!validity_map_.empty())
    validity_map_.begin()->first->RequestFocus();

  // |controller_| decides when to hide the dialog.
  return false;
}

// TODO(wittman): Remove this override once we move to the new style frame view
// on all dialogs.
views::NonClientFrameView* AutofillDialogViews::CreateNonClientFrameView(
    views::Widget* widget) {
  return CreateConstrainedStyleNonClientFrameView(
      widget,
      controller_->web_contents()->GetBrowserContext());
}

void AutofillDialogViews::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  // TODO(estade): Should the menu be shown on mouse down?
  DetailsGroup* group = NULL;
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    if (sender == iter->second.suggested_button) {
      group = &iter->second;
      break;
    }
  }
  DCHECK(group);

  if (!group->suggested_button->visible())
    return;

  menu_runner_.reset(new views::MenuRunner(
                         controller_->MenuModelForSection(group->section)));

  group->container->SetActive(true);
  views::Button::ButtonState state = group->suggested_button->state();
  group->suggested_button->SetState(views::Button::STATE_PRESSED);
  // Ignore the result since we don't need to handle a deleted menu specially.
  gfx::Rect bounds = group->suggested_button->GetBoundsInScreen();
  bounds.Inset(group->suggested_button->GetInsets());
  ignore_result(
      menu_runner_->RunMenuAt(sender->GetWidget(),
                              NULL,
                              bounds,
                              views::MenuItemView::TOPRIGHT,
                              ui::GetMenuSourceTypeForEvent(event),
                              0));
  group->container->SetActive(false);
  group->suggested_button->SetState(state);
}

void AutofillDialogViews::ContentsChanged(views::Textfield* sender,
                                          const string16& new_contents) {
  TextfieldEditedOrActivated(sender, true);
}

bool AutofillDialogViews::HandleKeyEvent(views::Textfield* sender,
                                         const ui::KeyEvent& key_event) {
  scoped_ptr<ui::KeyEvent> copy(key_event.Copy());
#if defined(OS_WIN) && !defined(USE_AURA)
  content::NativeWebKeyboardEvent event(copy->native_event());
#else
  content::NativeWebKeyboardEvent event(copy.get());
#endif
  return controller_->HandleKeyPressEventInInput(event);
}

bool AutofillDialogViews::HandleMouseEvent(views::Textfield* sender,
                                           const ui::MouseEvent& mouse_event) {
  if (mouse_event.IsLeftMouseButton() && sender->HasFocus()) {
    TextfieldEditedOrActivated(sender, false);
    // Show an error bubble if a user clicks on an input that's already focused
    // (and invalid).
    ShowErrorBubbleForViewIfNecessary(sender);
  }

  return false;
}

void AutofillDialogViews::OnWillChangeFocus(
    views::View* focused_before,
    views::View* focused_now) {
  controller_->FocusMoved();
  error_bubble_.reset();
}

void AutofillDialogViews::OnDidChangeFocus(
    views::View* focused_before,
    views::View* focused_now) {
  // If user leaves an edit-field, revalidate the group it belongs to.
  if (focused_before) {
    DetailsGroup* group = GroupForView(focused_before);
    if (group && group->container->visible())
      ValidateGroup(*group, VALIDATE_EDIT);
  }

  // Show an error bubble when the user focuses the input.
  if (focused_now)
    ShowErrorBubbleForViewIfNecessary(focused_now);
}

void AutofillDialogViews::OnSelectedIndexChanged(views::Combobox* combobox) {
  DetailsGroup* group = GroupForView(combobox);
  ValidateGroup(*group, VALIDATE_EDIT);
}

void AutofillDialogViews::StyledLabelLinkClicked(const ui::Range& range,
                                                 int event_flags) {
  controller_->LegalDocumentLinkClicked(range);
}

void AutofillDialogViews::InitChildViews() {
  button_strip_extra_view_ = new LayoutPropagationView();
  button_strip_extra_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

  save_in_chrome_checkbox_ =
      new views::Checkbox(controller_->SaveLocallyText());
  save_in_chrome_checkbox_->SetChecked(true);
  button_strip_extra_view_->AddChildView(save_in_chrome_checkbox_);

  autocheckout_progress_bar_view_ = new views::View();
  autocheckout_progress_bar_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 15, 0));

  autocheckout_progress_bar_ = new AutocheckoutProgressBar();
  autocheckout_progress_bar_view_->AddChildView(autocheckout_progress_bar_);

  button_strip_extra_view_->AddChildView(autocheckout_progress_bar_view_);
  autocheckout_progress_bar_view_->SetVisible(false);

  contents_ = new views::View();
  contents_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  contents_->AddChildView(CreateMainContainer());
  sign_in_webview_ = new views::WebView(controller_->profile());
  sign_in_webview_->SetVisible(false);
  contents_->AddChildView(sign_in_webview_);
  sign_in_delegate_.reset(
      new AutofillDialogSignInDelegate(this,
                                       sign_in_webview_->GetWebContents()));
}

views::View* AutofillDialogViews::CreateMainContainer() {
  main_container_ = new ShieldableContentsView();

  account_chooser_ = new AccountChooser(controller_);
  notification_area_ = new NotificationArea(controller_);
  notification_area_->set_arrow_centering_anchor(account_chooser_->AsWeakPtr());
  main_container_->AddToContents(notification_area_);

  scrollable_area_ = new SizeLimitedScrollView(CreateDetailsContainer());
  main_container_->AddToContents(scrollable_area_);

  autocheckout_steps_area_ = new AutocheckoutStepsArea();
  main_container_->AddToContents(autocheckout_steps_area_);

  return main_container_;
}

views::View* AutofillDialogViews::CreateDetailsContainer() {
  details_container_ = new DetailsContainerView(
      base::Bind(&AutofillDialogViews::DetailsContainerBoundsChanged,
                 base::Unretained(this)));
  // A box layout is used because it respects widget visibility.
  details_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    CreateDetailsSection(iter->second.section);
    details_container_->AddChildView(iter->second.container);
  }

  return details_container_;
}

void AutofillDialogViews::CreateDetailsSection(DialogSection section) {
  // Inputs container (manual inputs + combobox).
  views::View* inputs_container = CreateInputsContainer(section);

  DetailsGroup* group = GroupForSection(section);
  // Container (holds label + inputs).
  group->container = new SectionContainer(
      controller_->LabelForSection(section),
      inputs_container,
      group->suggested_button);
  UpdateDetailsGroupState(*group);
}

views::View* AutofillDialogViews::CreateInputsContainer(DialogSection section) {
  views::View* inputs_container = new views::View();
  views::GridLayout* layout = new views::GridLayout(inputs_container);
  inputs_container->SetLayoutManager(layout);

  int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  // A column for the menu button.
  column_set->AddColumn(views::GridLayout::CENTER,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  layout->StartRow(0, kColumnSetId);

  // The |info_view| holds |manual_inputs| and |suggested_info|, allowing the
  // dialog to toggle which is shown.
  views::View* info_view = new views::View();
  info_view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  views::View* manual_inputs = InitInputsView(section);
  info_view->AddChildView(manual_inputs);
  SuggestionView* suggested_info =
      new SuggestionView(controller_->EditSuggestionText(), this);
  info_view->AddChildView(suggested_info);
  layout->AddView(info_view);

  views::ImageButton* menu_button = new views::ImageButton(this);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  menu_button->SetImage(views::Button::STATE_NORMAL,
      rb.GetImageSkiaNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON));
  menu_button->SetImage(views::Button::STATE_PRESSED,
      rb.GetImageSkiaNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON_P));
  menu_button->SetImage(views::Button::STATE_HOVERED,
      rb.GetImageSkiaNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON_H));
  menu_button->SetImage(views::Button::STATE_DISABLED,
      rb.GetImageSkiaNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON_D));
  menu_button->set_border(views::Border::CreateEmptyBorder(
      kMenuButtonTopOffset,
      kMenuButtonHorizontalPadding,
      0,
      kMenuButtonHorizontalPadding));
  layout->AddView(menu_button);

  DetailsGroup* group = GroupForSection(section);
  group->suggested_button = menu_button;
  group->manual_input = manual_inputs;
  group->suggested_info = suggested_info;
  return inputs_container;
}

// TODO(estade): we should be using Chrome-style constrained window padding
// values.
views::View* AutofillDialogViews::InitInputsView(DialogSection section) {
  const DetailInputs& inputs = controller_->RequestedFieldsForSection(section);
  TextfieldMap* textfields = &GroupForSection(section)->textfields;
  ComboboxMap* comboboxes = &GroupForSection(section)->comboboxes;

  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  for (DetailInputs::const_iterator it = inputs.begin();
       it != inputs.end(); ++it) {
    const DetailInput& input = *it;
    int kColumnSetId = input.row_id;
    views::ColumnSet* column_set = layout->GetColumnSet(kColumnSetId);
    if (!column_set) {
      // Create a new column set and row.
      column_set = layout->AddColumnSet(kColumnSetId);
      if (it != inputs.begin())
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, kColumnSetId);
    } else {
      // Add a new column to existing row.
      column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
      // Must explicitly skip the padding column since we've already started
      // adding views.
      layout->SkipColumns(1);
    }

    float expand = input.expand_weight;
    column_set->AddColumn(views::GridLayout::FILL,
                          views::GridLayout::BASELINE,
                          expand ? expand : 1.0,
                          views::GridLayout::USE_PREF,
                          0,
                          0);

    ui::ComboboxModel* input_model =
        controller_->ComboboxModelForAutofillType(input.type);
    views::View* view_to_add = NULL;
    if (input_model) {
      views::Combobox* combobox = new views::Combobox(input_model);
      combobox->set_listener(this);
      comboboxes->insert(std::make_pair(&input, combobox));

      for (int i = 0; i < input_model->GetItemCount(); ++i) {
        if (input.initial_value == input_model->GetItemAt(i)) {
          combobox->SetSelectedIndex(i);
          break;
        }
      }
      view_to_add = combobox;
    } else {
      DecoratedTextfield* field = new DecoratedTextfield(
          input.initial_value,
          l10n_util::GetStringUTF16(input.placeholder_text_rid),
          this);

      gfx::Image icon =
          controller_->IconForField(input.type, input.initial_value);
      field->SetIcon(icon);

      textfields->insert(std::make_pair(&input, field));
      view_to_add = field;
    }

    // This is the same as AddView(view_to_add), except that 1 is used for the
    // view's preferred width. Thus the width of the column completely depends
    // on |expand|.
    layout->AddView(view_to_add, 1, 1,
                    views::GridLayout::FILL, views::GridLayout::BASELINE,
                    1, 0);
  }

  return view;
}

void AutofillDialogViews::UpdateSectionImpl(
    DialogSection section,
    bool clobber_inputs) {
  const DetailInputs& updated_inputs =
      controller_->RequestedFieldsForSection(section);
  DetailsGroup* group = GroupForSection(section);

  for (DetailInputs::const_iterator iter = updated_inputs.begin();
       iter != updated_inputs.end(); ++iter) {
    const DetailInput& input = *iter;
    TextfieldMap::iterator text_mapping = group->textfields.find(&input);

    if (text_mapping != group->textfields.end()) {
      DecoratedTextfield* decorated = text_mapping->second;
      decorated->SetEnabled(input.editable);
      if (decorated->text().empty() || clobber_inputs) {
        decorated->SetText(iter->initial_value);
        decorated->SetIcon(
            controller_->IconForField(input.type, decorated->text()));
      }
    }

    ComboboxMap::iterator combo_mapping = group->comboboxes.find(&input);
    if (combo_mapping != group->comboboxes.end()) {
      views::Combobox* combobox = combo_mapping->second;
      combobox->SetEnabled(input.editable);
      if (combobox->selected_index() == combobox->model()->GetDefaultIndex() ||
          clobber_inputs) {
        for (int i = 0; i < combobox->model()->GetItemCount(); ++i) {
          if (input.initial_value == combobox->model()->GetItemAt(i)) {
            combobox->SetSelectedIndex(i);
            break;
          }
        }
      }
    }
  }

  UpdateDetailsGroupState(*group);
}

void AutofillDialogViews::UpdateDetailsGroupState(const DetailsGroup& group) {
  const SuggestionState& suggestion_state =
      controller_->SuggestionStateForSection(group.section);
  bool show_suggestions = !suggestion_state.text.empty();
  group.suggested_info->SetVisible(show_suggestions);
  group.suggested_info->SetSuggestionText(suggestion_state.text,
                                          suggestion_state.text_style);
  group.suggested_info->SetSuggestionIcon(suggestion_state.icon);

  if (!suggestion_state.extra_text.empty()) {
    group.suggested_info->ShowTextfield(
        suggestion_state.extra_text,
        suggestion_state.extra_icon);
  }

  group.manual_input->SetVisible(!show_suggestions);

  // Show or hide the "Save in chrome" checkbox. If nothing is in editing mode,
  // hide. If the controller tells us not to show it, likewise hide.
  UpdateSaveInChromeCheckbox();

  const bool has_menu = !!controller_->MenuModelForSection(group.section);

  if (group.suggested_button)
    group.suggested_button->SetVisible(has_menu);

  if (group.container) {
    group.container->SetForwardMouseEvents(has_menu && show_suggestions);
    group.container->SetVisible(controller_->SectionIsActive(group.section));
    if (group.container->visible())
      ValidateGroup(group, VALIDATE_EDIT);
  }

  ContentsPreferredSizeChanged();
}

template<class T>
void AutofillDialogViews::SetValidityForInput(
    T* input,
    const string16& message) {
  bool invalid = !message.empty();
  input->SetInvalid(invalid);

  if (invalid) {
    validity_map_[input] = message;
  } else {
    validity_map_.erase(input);

    if (error_bubble_ && error_bubble_->anchor() == input) {
      validity_map_.erase(input);
      error_bubble_.reset();
    }
  }
}

void AutofillDialogViews::ShowErrorBubbleForViewIfNecessary(views::View* view) {
  if (!view->GetWidget())
    return;

  if (error_bubble_ && error_bubble_->anchor() == view)
    return;

  std::map<views::View*, string16>::iterator error_message =
      validity_map_.find(view);
  if (error_message != validity_map_.end())
    error_bubble_.reset(new ErrorBubble(view, error_message->second));
}

void AutofillDialogViews::MarkInputsInvalid(DialogSection section,
                                            const ValidityData& validity_data) {
  DetailsGroup group = *GroupForSection(section);
  DCHECK(group.container->visible());

  typedef std::map<AutofillFieldType,
      base::Callback<void(const base::string16&)> > FieldMap;
  FieldMap field_map;

  if (group.manual_input->visible()) {
    for (TextfieldMap::const_iterator iter = group.textfields.begin();
         iter != group.textfields.end(); ++iter) {
      field_map[iter->first->type] = base::Bind(
          &AutofillDialogViews::SetValidityForInput<DecoratedTextfield>,
          base::Unretained(this),
          iter->second);
    }
    for (ComboboxMap::const_iterator iter = group.comboboxes.begin();
         iter != group.comboboxes.end(); ++iter) {
      field_map[iter->first->type] = base::Bind(
          &AutofillDialogViews::SetValidityForInput<views::Combobox>,
          base::Unretained(this),
          iter->second);
    }
  } else if (section == SECTION_CC) {
    field_map[CREDIT_CARD_VERIFICATION_CODE] = base::Bind(
        &AutofillDialogViews::SetValidityForInput<DecoratedTextfield>,
        base::Unretained(this),
        group.suggested_info->decorated_textfield());
  }

  // Flag invalid fields, removing them from |field_map|.
  for (ValidityData::const_iterator iter = validity_data.begin();
       iter != validity_data.end(); ++iter) {
    const string16& message = iter->second;
    field_map[iter->first].Run(message);
    field_map.erase(iter->first);
  }

  // The remaining fields in |field_map| are valid. Mark them as such.
  for (FieldMap::iterator iter = field_map.begin(); iter != field_map.end();
       ++iter) {
    iter->second.Run(base::string16());
  }
}

bool AutofillDialogViews::ValidateGroup(const DetailsGroup& group,
                                        ValidationType validation_type) {
  DCHECK(group.container->visible());

  scoped_ptr<DetailInput> cvc_input;
  DetailOutputMap detail_outputs;

  if (group.manual_input->visible()) {
    for (TextfieldMap::const_iterator iter = group.textfields.begin();
         iter != group.textfields.end(); ++iter) {
      if (!iter->first->editable)
        continue;

      detail_outputs[iter->first] = iter->second->text();
    }
    for (ComboboxMap::const_iterator iter = group.comboboxes.begin();
         iter != group.comboboxes.end(); ++iter) {
      if (!iter->first->editable)
        continue;

      views::Combobox* combobox = iter->second;
      string16 item =
          combobox->model()->GetItemAt(combobox->selected_index());
      detail_outputs[iter->first] = item;
    }
  } else if (group.section == SECTION_CC) {
    DecoratedTextfield* decorated_cvc =
        group.suggested_info->decorated_textfield();
    cvc_input.reset(new DetailInput);
    cvc_input->type = CREDIT_CARD_VERIFICATION_CODE;
    detail_outputs[cvc_input.get()] = decorated_cvc->text();
  }

  ValidityData invalid_inputs = controller_->InputsAreValid(
      group.section, detail_outputs, validation_type);
  MarkInputsInvalid(group.section, invalid_inputs);

  return invalid_inputs.empty();
}

bool AutofillDialogViews::ValidateForm() {
  bool all_valid = true;
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    const DetailsGroup& group = iter->second;
    if (!group.container->visible())
      continue;

    if (!ValidateGroup(group, VALIDATE_FINAL))
      all_valid = false;
  }

  return all_valid;
}

void AutofillDialogViews::TextfieldEditedOrActivated(
    views::Textfield* textfield,
    bool was_edit) {
  DetailsGroup* group = GroupForView(textfield);
  DCHECK(group);

  // Figure out the AutofillFieldType this textfield represents.
  AutofillFieldType type = UNKNOWN_TYPE;
  DecoratedTextfield* decorated = NULL;

  // Look for the input in the manual inputs.
  for (TextfieldMap::const_iterator iter = group->textfields.begin();
       iter != group->textfields.end();
       ++iter) {
    decorated = iter->second;
    if (decorated == textfield) {
      controller_->UserEditedOrActivatedInput(group->section,
                                              iter->first,
                                              GetWidget()->GetNativeView(),
                                              textfield->GetBoundsInScreen(),
                                              textfield->text(),
                                              was_edit);
      type = iter->first->type;
      break;
    }
  }

  if (textfield == group->suggested_info->decorated_textfield()) {
    decorated = group->suggested_info->decorated_textfield();
    type = CREDIT_CARD_VERIFICATION_CODE;
  }
  DCHECK_NE(UNKNOWN_TYPE, type);

  // If the field is marked as invalid, check if the text is now valid.
  // Many fields (i.e. CC#) are invalid for most of the duration of editing,
  // so flagging them as invalid prematurely is not helpful. However,
  // correcting a minor mistake (i.e. a wrong CC digit) should immediately
  // result in validation - positive user feedback.
  if (decorated->invalid() && was_edit) {
    SetValidityForInput<DecoratedTextfield>(
        decorated,
        controller_->InputValidityMessage(group->section, type,
                                          textfield->text()));

    // If the field transitioned from invalid to valid, re-validate the group,
    // since inter-field checks become meaningful with valid fields.
    if (!decorated->invalid())
      ValidateGroup(*group, VALIDATE_EDIT);
  }

  gfx::Image icon = controller_->IconForField(type, textfield->text());
  decorated->SetIcon(icon);
}

void AutofillDialogViews::UpdateSaveInChromeCheckbox() {
  save_in_chrome_checkbox_->SetVisible(
      controller_->ShouldOfferToSaveInChrome());
}

void AutofillDialogViews::ContentsPreferredSizeChanged() {
  if (GetWidget()) {
    GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
    // If the above line does not cause the dialog's size to change, |contents_|
    // may not be laid out. This will trigger a layout only if it's needed.
    contents_->SetBoundsRect(contents_->bounds());

    if (error_bubble_)
      error_bubble_->UpdatePosition();
  }
}

AutofillDialogViews::DetailsGroup* AutofillDialogViews::GroupForSection(
    DialogSection section) {
  return &detail_groups_.find(section)->second;
}

AutofillDialogViews::DetailsGroup* AutofillDialogViews::GroupForView(
    views::View* view) {
  DCHECK(view);

  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    DetailsGroup* group = &iter->second;
    if (view->parent() == group->manual_input)
      return group;

    views::View* decorated =
        view->GetAncestorWithClassName(kDecoratedTextfieldClassName);

    // Textfields need to check a second case, since they can be
    // suggested inputs instead of directly editable inputs. Those are
    // accessed via |suggested_info|.
    if (decorated &&
        decorated == group->suggested_info->decorated_textfield()) {
      return group;
    }
  }
  return NULL;
}

views::Textfield* AutofillDialogViews::TextfieldForInput(
    const DetailInput& input) {
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    const DetailsGroup& group = iter->second;
    TextfieldMap::const_iterator text_mapping = group.textfields.find(&input);
    if (text_mapping != group.textfields.end())
      return text_mapping->second;
  }

  return NULL;
}

views::Combobox* AutofillDialogViews::ComboboxForInput(
    const DetailInput& input) {
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    const DetailsGroup& group = iter->second;
    ComboboxMap::const_iterator combo_mapping = group.comboboxes.find(&input);
    if (combo_mapping != group.comboboxes.end())
      return combo_mapping->second;
  }

  return NULL;
}

void AutofillDialogViews::DetailsContainerBoundsChanged() {
  if (error_bubble_)
    error_bubble_->UpdatePosition();
}

AutofillDialogViews::DetailsGroup::DetailsGroup(DialogSection section)
    : section(section),
      container(NULL),
      manual_input(NULL),
      suggested_info(NULL),
      suggested_button(NULL) {}

AutofillDialogViews::DetailsGroup::~DetailsGroup() {}

}  // namespace autofill
