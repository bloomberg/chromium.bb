// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_dialog_views.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_sign_in_delegate.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/autofill/loading_animation.h"
#include "chrome/browser/ui/views/autofill/decorated_textfield.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/gfx/point.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/combobox/combobox.h"
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
using web_modal::WebContentsModalDialogManagerDelegate;

namespace autofill {

namespace {

// The default height of the stack of messages in the overlay view.
const int kDefaultMessageStackHeight = 90;

// The width for the section container.
const int kSectionContainerWidth = 440;

// The minimum useful height of the contents area of the dialog.
const int kMinimumContentsHeight = 101;

// The default height of the loading shield, also its minimum size.
const int kInitialLoadingShieldHeight = 150;

// Horizontal padding between text and other elements (in pixels).
const int kAroundTextPadding = 4;

// The space between the edges of a notification bar and the text within (in
// pixels).
const int kNotificationPadding = 17;

// Vertical padding above and below each detail section (in pixels).
const int kDetailSectionVerticalPadding = 10;

const int kArrowHeight = 7;
const int kArrowWidth = 2 * kArrowHeight;

// The padding inside the edges of the dialog, in pixels.
const int kDialogEdgePadding = 20;

// The vertical padding between rows of manual inputs (in pixels).
const int kManualInputRowPadding = 10;

// The margin between the content of the error bubble and its border.
const int kErrorBubbleHorizontalMargin = 14;
const int kErrorBubbleVerticalMargin = 12;

// The visible width of bubble borders (differs from the actual width) in px.
const int kBubbleBorderVisibleWidth = 1;

// Slight shading for mouse hover and legal document background.
SkColor kShadingColor = SkColorSetARGB(7, 0, 0, 0);

// A border color for the legal document view.
SkColor kSubtleBorderColor = SkColorSetARGB(10, 0, 0, 0);

// The top and bottom padding, in pixels, for the suggestions menu dropdown
// arrows.
const int kMenuButtonTopInset = 3;
const int kMenuButtonBottomInset = 6;

// Spacing between lines of text in the overlay view.
const int kOverlayTextInterlineSpacing = 10;

// Spacing below image and above text messages in overlay view.
const int kOverlayImageBottomMargin = 100;

// A dimmer text color used in various parts of the dialog. TODO(estade): should
// this be part of NativeTheme? Currently the value is duplicated in several
// places.
const SkColor kGreyTextColor = SkColorSetRGB(102, 102, 102);

const char kNotificationAreaClassName[] = "autofill/NotificationArea";
const char kOverlayViewClassName[] = "autofill/OverlayView";
const char kSectionContainerClassName[] = "autofill/SectionContainer";
const char kSuggestedButtonClassName[] = "autofill/SuggestedButton";

// Draws an arrow at the top of |canvas| pointing to |tip_x|.
void DrawArrow(gfx::Canvas* canvas,
               int tip_x,
               const SkColor& fill_color,
               const SkColor& stroke_color) {
  const int arrow_half_width = kArrowWidth / 2.0f;

  SkPath arrow;
  arrow.moveTo(tip_x - arrow_half_width, kArrowHeight);
  arrow.lineTo(tip_x, 0);
  arrow.lineTo(tip_x + arrow_half_width, kArrowHeight);

  SkPaint fill_paint;
  fill_paint.setColor(fill_color);
  canvas->DrawPath(arrow, fill_paint);

  if (stroke_color != SK_ColorTRANSPARENT) {
    SkPaint stroke_paint;
    stroke_paint.setColor(stroke_color);
    stroke_paint.setStyle(SkPaint::kStroke_Style);
    canvas->DrawPath(arrow, stroke_paint);
  }
}

// This class handles layout for the first row of a SuggestionView.
// It exists to circumvent shortcomings of GridLayout and BoxLayout (namely that
// the former doesn't fully respect child visibility, and that the latter won't
// expand a single child).
class SectionRowView : public views::View {
 public:
  SectionRowView() {
    set_border(views::Border::CreateEmptyBorder(10, 0, 0, 0));
  }

  virtual ~SectionRowView() {}

  // views::View implementation:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    int height = 0;
    int width = 0;
    for (int i = 0; i < child_count(); ++i) {
      if (child_at(i)->visible()) {
        if (width > 0)
          width += kAroundTextPadding;

        gfx::Size size = child_at(i)->GetPreferredSize();
        height = std::max(height, size.height());
        width += size.width();
      }
    }

    gfx::Insets insets = GetInsets();
    return gfx::Size(width + insets.width(), height + insets.height());
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

// A tooltip icon (just an ImageView with a tooltip). Looks like (?).
class TooltipIcon : public views::ImageView {
 public:
  explicit TooltipIcon(const base::string16& tooltip) : tooltip_(tooltip) {
    SetImage(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AUTOFILL_TOOLTIP_ICON).ToImageSkia());
  }
  virtual ~TooltipIcon() {}

  // views::View implementation
  virtual bool GetTooltipText(const gfx::Point& p,
                              base::string16* tooltip) const OVERRIDE {
    *tooltip = tooltip_;
    return !tooltip_.empty();
  }

 private:
  base::string16 tooltip_;

  DISALLOW_COPY_AND_ASSIGN(TooltipIcon);
};

// A View for a single notification banner.
class NotificationView : public views::View,
                         public views::ButtonListener,
                         public views::StyledLabelListener {
 public:
  NotificationView(const DialogNotification& data,
                   AutofillDialogViewDelegate* delegate)
      : data_(data),
        delegate_(delegate),
        checkbox_(NULL) {
    scoped_ptr<views::View> label_view;
    if (data.HasCheckbox()) {
      scoped_ptr<views::Checkbox> checkbox(
          new views::Checkbox(base::string16()));
      checkbox->SetText(data.display_text());
      checkbox->SetTextMultiLine(true);
      checkbox->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      checkbox->SetTextColor(views::Button::STATE_NORMAL,
                             data.GetTextColor());
      checkbox->SetTextColor(views::Button::STATE_HOVERED,
                             data.GetTextColor());
      checkbox->SetChecked(data.checked());
      checkbox->set_listener(this);
      checkbox_ = checkbox.get();
      label_view.reset(checkbox.release());
    } else {
      scoped_ptr<views::StyledLabel> label(new views::StyledLabel(
          data.display_text(), this));
      label->set_auto_color_readability_enabled(false);

      views::StyledLabel::RangeStyleInfo text_style;
      text_style.color = data.GetTextColor();

      if (!data.link_range().is_empty()) {
        label->AddStyleRange(gfx::Range(0, data.link_range().start()),
                             text_style);
        label->AddStyleRange(
            gfx::Range(0, data.link_range().start()),
            views::StyledLabel::RangeStyleInfo::CreateForLink());
        label->AddStyleRange(gfx::Range(0, data.link_range().start()),
                             text_style);
        label->AddStyleRange(
            gfx::Range(data.link_range().end(), data.display_text().size()),
            views::StyledLabel::RangeStyleInfo::CreateForLink());
      } else {
        label->AddStyleRange(gfx::Range(0, data.display_text().size()),
                             text_style);
      }

      label_view.reset(label.release());
    }

    AddChildView(label_view.release());

    if (!data.tooltip_text().empty())
      AddChildView(new TooltipIcon(data.tooltip_text()));

    set_background(
       views::Background::CreateSolidBackground(data.GetBackgroundColor()));
    set_border(views::Border::CreateSolidSidedBorder(1, 0, 1, 0,
                                                     data.GetBorderColor()));
  }

  virtual ~NotificationView() {}

  views::Checkbox* checkbox() {
    return checkbox_;
  }

  // views::View implementation.
  virtual gfx::Insets GetInsets() const OVERRIDE {
    int vertical_padding = kNotificationPadding;
    if (checkbox_)
      vertical_padding -= 3;
    return gfx::Insets(vertical_padding, kDialogEdgePadding,
                       vertical_padding, kDialogEdgePadding);
  }

  virtual int GetHeightForWidth(int width) OVERRIDE {
    int label_width = width - GetInsets().width();
    if (child_count() > 1) {
      views::View* tooltip_icon = child_at(1);
      label_width -= tooltip_icon->GetPreferredSize().width() +
          kDialogEdgePadding;
    }

    return child_at(0)->GetHeightForWidth(label_width) + GetInsets().height();
  }

  virtual void Layout() OVERRIDE {
    // Surprisingly, GetContentsBounds() doesn't consult GetInsets().
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(GetInsets());
    int right_bound = bounds.right();

    if (child_count() > 1) {
      // The icon takes up the entire vertical space and an extra 20px on
      // each side. This increases the hover target for the tooltip.
      views::View* tooltip_icon = child_at(1);
      gfx::Size icon_size = tooltip_icon->GetPreferredSize();
      int icon_width = icon_size.width() + kDialogEdgePadding;
      right_bound -= icon_width;
      tooltip_icon->SetBounds(
          right_bound, 0,
          icon_width + kDialogEdgePadding, GetLocalBounds().height());
    }

    child_at(0)->SetBounds(bounds.x(), bounds.y(),
                           right_bound - bounds.x(), bounds.height());
  }

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    DCHECK_EQ(sender, checkbox_);
    delegate_->NotificationCheckboxStateChanged(data_.type(),
                                                checkbox_->checked());
  }

  // views::StyledLabelListener implementation.
  virtual void StyledLabelLinkClicked(const gfx::Range& range, int event_flags)
      OVERRIDE {
    delegate_->LinkClicked(data_.link_url());
  }

 private:
  // The model data for this notification.
  DialogNotification data_;

  // The delegate that handles interaction with |this|.
  AutofillDialogViewDelegate* delegate_;

  // The checkbox associated with this notification, or NULL if there is none.
  views::Checkbox* checkbox_;

  DISALLOW_COPY_AND_ASSIGN(NotificationView);
};

// A view that displays a loading message with some dancing dots.
class LoadingAnimationView : public views::View,
                             public gfx::AnimationDelegate {
 public:
  explicit LoadingAnimationView(const base::string16& text) :
      container_(new views::View()),
      animation_(this) {

    set_background(views::Background::CreateSolidBackground(
        GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_DialogBackground)));

    AddChildView(container_);
    container_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

    gfx::Font font = ui::ResourceBundle::GetSharedInstance().GetFont(
        ui::ResourceBundle::BaseFont).DeriveFont(8);

    views::Label* label = new views::Label();
    label->SetText(text);
    label->SetFont(font);
    container_->AddChildView(label);

    for (size_t i = 0; i < 3; ++i) {
      views::Label* dot = new views::Label();
      dot->SetText(ASCIIToUTF16("."));
      dot->SetFont(font);
      container_->AddChildView(dot);
    }
  }

  virtual ~LoadingAnimationView() {}

  // views::View implementation.
  virtual void SetVisible(bool visible) OVERRIDE {
    if (visible)
      animation_.Start();
    else
      animation_.Reset();

    views::View::SetVisible(visible);
  }

  virtual void Layout() OVERRIDE {
    gfx::Size container_size = container_->GetPreferredSize();
    gfx::Rect container_bounds((width() - container_size.width()) / 2,
                               (height() - container_size.height()) / 2,
                               container_size.width(),
                               container_size.height());
    container_->SetBoundsRect(container_bounds);
    container_->Layout();

    for (size_t i = 0; i < 3; ++i) {
      views::View* dot = container_->child_at(i + 1);
      dot->SetY(dot->y() + animation_.GetCurrentValueForDot(i) * 10.0);
    }
  }

  // gfx::AnimationDelegate implementation.
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE {
    DCHECK_EQ(animation, &animation_);
    Layout();
  }

 private:
  // Contains the "Loading" label and the dots.
  views::View* container_;

  LoadingAnimation animation_;

  DISALLOW_COPY_AND_ASSIGN(LoadingAnimationView);
};

}  // namespace

// AutofillDialogViews::ErrorBubble --------------------------------------------

AutofillDialogViews::ErrorBubble::ErrorBubble(views::View* anchor,
                                              views::View* anchor_container,
                                              const base::string16& message)
    : anchor_(anchor),
      anchor_container_(anchor_container) {
  DCHECK(anchor_container_->Contains(anchor));

  SetAnchorView(anchor_);
  set_arrow(ShouldArrowGoOnTheRight() ? views::BubbleBorder::TOP_RIGHT :
                                        views::BubbleBorder::TOP_LEFT);
  set_margins(gfx::Insets(kErrorBubbleVerticalMargin,
                          kErrorBubbleHorizontalMargin,
                          kErrorBubbleVerticalMargin,
                          kErrorBubbleHorizontalMargin));
  set_use_focusless(true);

  SetLayoutManager(new views::FillLayout);
  views::Label* label = new views::Label(message);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  AddChildView(label);

  widget_ = views::BubbleDelegateView::CreateBubble(this);
  UpdatePosition();
}

AutofillDialogViews::ErrorBubble::~ErrorBubble() {
  DCHECK(!widget_);
}

void AutofillDialogViews::ErrorBubble::Hide() {
  views::Widget* widget = GetWidget();
  if (widget && !widget->IsClosed())
    widget->Close();
}

void AutofillDialogViews::ErrorBubble::UpdatePosition() {
  if (!widget_)
    return;

  if (!anchor_->GetVisibleBounds().IsEmpty()) {
    SizeToContents();
    widget_->SetVisibilityChangedAnimationsEnabled(true);
    widget_->Show();
  } else {
    widget_->SetVisibilityChangedAnimationsEnabled(false);
    widget_->Hide();
  }
}

gfx::Size AutofillDialogViews::ErrorBubble::GetPreferredSize() {
  int pref_width = GetPreferredBubbleWidth();
  pref_width -= GetBubbleFrameView()->GetInsets().width();
  pref_width -= 2 * kBubbleBorderVisibleWidth;
  return gfx::Size(pref_width, GetHeightForWidth(pref_width));
}

gfx::Rect AutofillDialogViews::ErrorBubble::GetBubbleBounds() {
  gfx::Rect bounds = views::BubbleDelegateView::GetBubbleBounds();
  gfx::Rect anchor_bounds = anchor_->GetBoundsInScreen();
  anchor_bounds.Inset(-GetBubbleFrameView()->bubble_border()->GetInsets());
  bounds.set_x(ShouldArrowGoOnTheRight() ?
      anchor_bounds.right() - bounds.width() - kBubbleBorderVisibleWidth :
      anchor_bounds.x() + kBubbleBorderVisibleWidth);
  return bounds;
}

void AutofillDialogViews::ErrorBubble::OnWidgetClosing(views::Widget* widget) {
  if (widget == widget_)
    widget_ = NULL;
}

bool AutofillDialogViews::ErrorBubble::ShouldFlipArrowForRtl() const {
  return false;
}

int AutofillDialogViews::ErrorBubble::GetContainerWidth() {
  return anchor_container_->width() - anchor_container_->GetInsets().width();
}

int AutofillDialogViews::ErrorBubble::GetPreferredBubbleWidth() {
  return (GetContainerWidth() - views::kRelatedControlHorizontalSpacing) / 2;
}

bool AutofillDialogViews::ErrorBubble::ShouldArrowGoOnTheRight() {
  gfx::Point anchor_offset;
  views::View::ConvertPointToTarget(anchor_, anchor_container_, &anchor_offset);
  anchor_offset.Offset(-anchor_container_->GetInsets().left(), 0);

  if (base::i18n::IsRTL()) {
    int anchor_right_x = anchor_offset.x() + anchor_->width();
    return anchor_right_x >= GetPreferredBubbleWidth();
  }

  return anchor_offset.x() + GetPreferredBubbleWidth() > GetContainerWidth();
}

// AutofillDialogViews::AccountChooser -----------------------------------------

AutofillDialogViews::AccountChooser::AccountChooser(
    AutofillDialogViewDelegate* delegate)
    : image_(new views::ImageView()),
      menu_button_(new views::MenuButton(NULL, base::string16(), this, true)),
      link_(new views::Link()),
      delegate_(delegate) {
  set_border(views::Border::CreateEmptyBorder(0, 0, 0, 10));
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                           kAroundTextPadding));
  AddChildView(image_);

  menu_button_->set_background(NULL);
  menu_button_->set_border(NULL);
  gfx::Insets insets = GetInsets();
  menu_button_->set_focus_border(
      views::FocusBorder::CreateDashedFocusBorder(insets.left(),
                                                  insets.top(),
                                                  insets.right(),
                                                  insets.bottom()));
  menu_button_->set_focusable(true);
  AddChildView(menu_button_);

  link_->set_listener(this);
  AddChildView(link_);
}

AutofillDialogViews::AccountChooser::~AccountChooser() {}

void AutofillDialogViews::AccountChooser::Update() {
  SetVisible(!delegate_->ShouldShowSpinner());

  gfx::Image icon = delegate_->AccountChooserImage();
  image_->SetImage(icon.AsImageSkia());
  menu_button_->SetText(delegate_->AccountChooserText());
  // This allows the button to shrink if the new text is smaller.
  menu_button_->ClearMaxTextSize();

  bool show_link = !delegate_->MenuModelForAccountChooser();
  menu_button_->SetVisible(!show_link);
  link_->SetText(delegate_->SignInLinkText());
  link_->SetVisible(show_link);
  link_->SetEnabled(!delegate_->ShouldDisableSignInLink());

  menu_runner_.reset();

  PreferredSizeChanged();
}

void AutofillDialogViews::AccountChooser::OnMenuButtonClicked(
    views::View* source,
    const gfx::Point& point) {
  DCHECK_EQ(menu_button_, source);

  ui::MenuModel* model = delegate_->MenuModelForAccountChooser();
  if (!model)
    return;

  menu_runner_.reset(new views::MenuRunner(model));
  if (menu_runner_->RunMenuAt(source->GetWidget(),
                              NULL,
                              source->GetBoundsInScreen(),
                              views::MenuItemView::TOPRIGHT,
                              ui::MENU_SOURCE_NONE,
                              0) == views::MenuRunner::MENU_DELETED) {
    return;
  }
}

void AutofillDialogViews::AccountChooser::LinkClicked(views::Link* source,
                                                      int event_flags) {
  delegate_->SignInLinkClicked();
}

// AutofillDialogViews::OverlayView --------------------------------------------

AutofillDialogViews::OverlayView::OverlayView(
    AutofillDialogViewDelegate* delegate)
    : delegate_(delegate),
      image_view_(new views::ImageView()),
      message_stack_(new views::View()) {
  set_background(views::Background::CreateSolidBackground(GetNativeTheme()->
      GetSystemColor(ui::NativeTheme::kColorId_DialogBackground)));

  AddChildView(image_view_);

  AddChildView(message_stack_);
  message_stack_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                           kOverlayTextInterlineSpacing));
}

AutofillDialogViews::OverlayView::~OverlayView() {}

int AutofillDialogViews::OverlayView::GetHeightForContentsForWidth(int width) {
  // In this case, 0 means "no preference".
  if (!message_stack_->visible())
    return 0;

  return kOverlayImageBottomMargin +
      views::kButtonVEdgeMarginNew +
      message_stack_->GetHeightForWidth(width) +
      image_view_->GetHeightForWidth(width);
}

void AutofillDialogViews::OverlayView::UpdateState() {
  const DialogOverlayState& state = delegate_->GetDialogOverlay();

  if (state.image.IsEmpty()) {
    SetVisible(false);
    return;
  }

  image_view_->SetImage(state.image.ToImageSkia());

  int label_height = 0;
  message_stack_->RemoveAllChildViews(true);

  if (!state.string.text.empty()) {
    views::Label* label = new views::Label();
    label->SetAutoColorReadabilityEnabled(false);
    label->SetMultiLine(true);
    label->SetText(state.string.text);
    label->SetFont(state.string.font);
    label->SetEnabledColor(state.string.text_color);
    message_stack_->AddChildView(label);
    label_height = label->GetPreferredSize().height();
  }

  message_stack_->SetVisible(message_stack_->child_count() > 0);

  const int kVerticalPadding = std::max(
      (kDefaultMessageStackHeight - label_height) / 2, kDialogEdgePadding);
  message_stack_->set_border(
      views::Border::CreateEmptyBorder(kVerticalPadding,
                                       kDialogEdgePadding,
                                       kVerticalPadding,
                                       kDialogEdgePadding));

  SetVisible(true);
  InvalidateLayout();
  if (parent())
    parent()->Layout();
}

gfx::Insets AutofillDialogViews::OverlayView::GetInsets() const {
  return gfx::Insets(12, 12, 12, 12);
}

void AutofillDialogViews::OverlayView::Layout() {
  gfx::Rect bounds = ContentBoundsSansBubbleBorder();
  if (!message_stack_->visible()) {
    image_view_->SetBoundsRect(bounds);
    return;
  }

  int message_height = message_stack_->GetHeightForWidth(bounds.width());
  int y = bounds.bottom() - message_height;
  message_stack_->SetBounds(bounds.x(), y, bounds.width(), message_height);

  gfx::Size image_size = image_view_->GetPreferredSize();
  y -= image_size.height() + kOverlayImageBottomMargin;
  image_view_->SetBounds(bounds.x(), y, bounds.width(), image_size.height());
}

const char* AutofillDialogViews::OverlayView::GetClassName() const {
  return kOverlayViewClassName;
}

void AutofillDialogViews::OverlayView::OnPaint(gfx::Canvas* canvas) {
  // BubbleFrameView doesn't mask the window, it just draws the border via
  // image assets. Match that rounding here.
  gfx::Rect rect = ContentBoundsSansBubbleBorder();
  const SkScalar kCornerRadius = SkIntToScalar(
      GetBubbleBorder() ? GetBubbleBorder()->GetBorderCornerRadius() : 2);
  gfx::Path window_mask;
  window_mask.addRoundRect(gfx::RectToSkRect(rect),
                           kCornerRadius, kCornerRadius);
  canvas->ClipPath(window_mask);

  OnPaintBackground(canvas);

  // Draw the arrow, border, and fill for the bottom area.
  if (message_stack_->visible()) {
    const int arrow_half_width = kArrowWidth / 2.0f;
    SkPath arrow;
    int y = message_stack_->y() - 1;
    // Note that we purposely draw slightly outside of |rect| so that the
    // stroke is hidden on the sides.
    arrow.moveTo(rect.x() - 1, y);
    arrow.rLineTo(rect.width() / 2 - arrow_half_width, 0);
    arrow.rLineTo(arrow_half_width, -kArrowHeight);
    arrow.rLineTo(arrow_half_width, kArrowHeight);
    arrow.lineTo(rect.right() + 1, y);
    arrow.lineTo(rect.right() + 1, rect.bottom() + 1);
    arrow.lineTo(rect.x() - 1, rect.bottom() + 1);
    arrow.close();

    SkPaint paint;
    paint.setColor(kShadingColor);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->DrawPath(arrow, paint);
    paint.setColor(kSubtleBorderColor);
    paint.setStyle(SkPaint::kStroke_Style);
    canvas->DrawPath(arrow, paint);
  }

  PaintChildren(canvas);
}

views::BubbleBorder* AutofillDialogViews::OverlayView::GetBubbleBorder() {
  views::View* frame = GetWidget()->non_client_view()->frame_view();
  std::string bubble_frame_view_name(views::BubbleFrameView::kViewClassName);
  if (frame->GetClassName() == bubble_frame_view_name)
    return static_cast<views::BubbleFrameView*>(frame)->bubble_border();
  NOTREACHED();
  return NULL;
}

gfx::Rect AutofillDialogViews::OverlayView::ContentBoundsSansBubbleBorder() {
  gfx::Rect bounds = GetContentsBounds();
  int bubble_width = 5;
  if (GetBubbleBorder())
    bubble_width = GetBubbleBorder()->GetBorderThickness();
  bounds.Inset(bubble_width, bubble_width, bubble_width, bubble_width);
  return bounds;
}

// AutofillDialogViews::NotificationArea ---------------------------------------

AutofillDialogViews::NotificationArea::NotificationArea(
    AutofillDialogViewDelegate* delegate)
    : delegate_(delegate) {
  // Reserve vertical space for the arrow (regardless of whether one exists).
  // The -1 accounts for the border.
  set_border(views::Border::CreateEmptyBorder(kArrowHeight - 1, 0, 0, 0));

  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
  SetLayoutManager(box_layout);
}

AutofillDialogViews::NotificationArea::~NotificationArea() {}

void AutofillDialogViews::NotificationArea::SetNotifications(
    const std::vector<DialogNotification>& notifications) {
  notifications_ = notifications;

  RemoveAllChildViews(true);

  if (notifications_.empty())
    return;

  for (size_t i = 0; i < notifications_.size(); ++i) {
    const DialogNotification& notification = notifications_[i];
    scoped_ptr<NotificationView> view(new NotificationView(notification,
                                                           delegate_));

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

void AutofillDialogViews::NotificationArea::PaintChildren(
    gfx::Canvas* canvas) {}

void AutofillDialogViews::NotificationArea::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  views::View::PaintChildren(canvas);

  if (HasArrow()) {
    DrawArrow(
        canvas,
        GetMirroredXInView(width() - arrow_centering_anchor_->width() / 2.0f),
        notifications_[0].GetBackgroundColor(),
        notifications_[0].GetBorderColor());
  }
}

void AutofillDialogViews::OnWidgetClosing(views::Widget* widget) {
  observer_.Remove(widget);
}

void AutofillDialogViews::OnWidgetBoundsChanged(views::Widget* widget,
                                                const gfx::Rect& new_bounds) {
  // Notify the web contents of its new auto-resize limits.
  if (sign_in_delegate_ && sign_in_webview_->visible())
    sign_in_delegate_->UpdateLimitsAndEnableAutoResize(
        GetMinimumSignInViewSize(), GetMaximumSignInViewSize());
  HideErrorBubble();
}

bool AutofillDialogViews::NotificationArea::HasArrow() {
  return !notifications_.empty() && notifications_[0].HasArrow() &&
      arrow_centering_anchor_.get();
}

// AutofillDialogViews::SectionContainer ---------------------------------------

AutofillDialogViews::SectionContainer::SectionContainer(
    const base::string16& label,
    views::View* controls,
    views::Button* proxy_button)
    : proxy_button_(proxy_button),
      forward_mouse_events_(false) {
  set_notify_enter_exit_on_child(true);

  set_border(views::Border::CreateEmptyBorder(kDetailSectionVerticalPadding,
                                              kDialogEdgePadding,
                                              kDetailSectionVerticalPadding,
                                              kDialogEdgePadding));

  // TODO(estade): this label should be semi-bold.
  views::Label* label_view = new views::Label(label);
  label_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::View* label_bar = new views::View();
  views::GridLayout* label_bar_layout = new views::GridLayout(label_bar);
  label_bar->SetLayoutManager(label_bar_layout);
  const int kColumnSetId = 0;
  views::ColumnSet* columns = label_bar_layout->AddColumnSet(kColumnSetId);
  columns->AddColumn(
      views::GridLayout::LEADING,
      views::GridLayout::LEADING,
      0,
      views::GridLayout::FIXED,
      kSectionContainerWidth - proxy_button->GetPreferredSize().width(),
      0);
  columns->AddColumn(views::GridLayout::LEADING,
                     views::GridLayout::LEADING,
                     0,
                     views::GridLayout::USE_PREF,
                     0,
                     0);
  label_bar_layout->StartRow(0, kColumnSetId);
  label_bar_layout->AddView(label_view);
  label_bar_layout->AddView(proxy_button);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  AddChildView(label_bar);
  AddChildView(controls);
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

const char* AutofillDialogViews::SectionContainer::GetClassName() const {
  return kSectionContainerClassName;
}

void AutofillDialogViews::SectionContainer::OnMouseMoved(
    const ui::MouseEvent& event) {
  SetActive(ShouldForwardEvent(event));
}

void AutofillDialogViews::SectionContainer::OnMouseEntered(
    const ui::MouseEvent& event) {
  if (!ShouldForwardEvent(event))
    return;

  SetActive(true);
  proxy_button_->OnMouseEntered(ProxyEvent(event));
  SchedulePaint();
}

void AutofillDialogViews::SectionContainer::OnMouseExited(
    const ui::MouseEvent& event) {
  SetActive(false);
  if (!ShouldForwardEvent(event))
    return;

  proxy_button_->OnMouseExited(ProxyEvent(event));
  SchedulePaint();
}

bool AutofillDialogViews::SectionContainer::OnMousePressed(
    const ui::MouseEvent& event) {
  if (!ShouldForwardEvent(event))
    return false;

  return proxy_button_->OnMousePressed(ProxyEvent(event));
}

void AutofillDialogViews::SectionContainer::OnMouseReleased(
    const ui::MouseEvent& event) {
  if (!ShouldForwardEvent(event))
    return;

  proxy_button_->OnMouseReleased(ProxyEvent(event));
}

views::View* AutofillDialogViews::SectionContainer::GetEventHandlerForPoint(
    const gfx::Point& point) {
  views::View* handler = views::View::GetEventHandlerForPoint(point);
  // If the event is not in the label bar and there's no background to be
  // cleared, let normal event handling take place.
  if (!background() &&
      point.y() > child_at(0)->bounds().bottom()) {
    return handler;
  }

  // Special case for (CVC) inputs in the suggestion view.
  if (forward_mouse_events_ &&
      handler->GetAncestorWithClassName(DecoratedTextfield::kViewClassName)) {
    return handler;
  }

  // Special case for the proxy button itself.
  if (handler == proxy_button_)
    return handler;

  return this;
}

// static
ui::MouseEvent AutofillDialogViews::SectionContainer::ProxyEvent(
    const ui::MouseEvent& event) {
  ui::MouseEvent event_copy = event;
  event_copy.set_location(gfx::Point());
  return event_copy;
}

bool AutofillDialogViews::SectionContainer::ShouldForwardEvent(
    const ui::MouseEvent& event) {
  // Always forward events on the label bar.
  return forward_mouse_events_ || event.y() <= child_at(0)->bounds().bottom();
}

// AutofillDialogViews::SuggestedButton ----------------------------------------

AutofillDialogViews::SuggestedButton::SuggestedButton(
    views::MenuButtonListener* listener)
    : views::MenuButton(NULL, base::string16(), listener, false) {
  const int kFocusBorderWidth = 1;
  set_border(views::Border::CreateEmptyBorder(kMenuButtonTopInset,
                                              kDialogEdgePadding,
                                              kMenuButtonBottomInset,
                                              kFocusBorderWidth));
  gfx::Insets insets = GetInsets();
  insets += gfx::Insets(-kFocusBorderWidth, -kFocusBorderWidth,
                        -kFocusBorderWidth, -kFocusBorderWidth);
  set_focus_border(
      views::FocusBorder::CreateDashedFocusBorder(insets.left(),
                                                  insets.top(),
                                                  insets.right(),
                                                  insets.bottom()));
  set_focusable(true);
}

AutofillDialogViews::SuggestedButton::~SuggestedButton() {}

gfx::Size AutofillDialogViews::SuggestedButton::GetPreferredSize() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::Size size = rb.GetImageNamed(ResourceIDForState()).Size();
  const gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

const char* AutofillDialogViews::SuggestedButton::GetClassName() const {
  return kSuggestedButtonClassName;
}

void AutofillDialogViews::SuggestedButton::PaintChildren(gfx::Canvas* canvas) {}

void AutofillDialogViews::SuggestedButton::OnPaint(gfx::Canvas* canvas) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::Insets insets = GetInsets();
  canvas->DrawImageInt(*rb.GetImageSkiaNamed(ResourceIDForState()),
                       insets.left(), insets.top());
  views::View::OnPaintFocusBorder(canvas);
}

int AutofillDialogViews::SuggestedButton::ResourceIDForState() const {
  views::Button::ButtonState button_state = state();
  if (button_state == views::Button::STATE_PRESSED)
    return IDR_AUTOFILL_DIALOG_MENU_BUTTON_P;
  else if (button_state == views::Button::STATE_HOVERED)
    return IDR_AUTOFILL_DIALOG_MENU_BUTTON_H;
  else if (button_state == views::Button::STATE_DISABLED)
    return IDR_AUTOFILL_DIALOG_MENU_BUTTON_D;
  DCHECK_EQ(views::Button::STATE_NORMAL, button_state);
  return IDR_AUTOFILL_DIALOG_MENU_BUTTON;
}

// AutofillDialogViews::DetailsContainerView -----------------------------------

AutofillDialogViews::DetailsContainerView::DetailsContainerView(
    const base::Closure& callback)
    : bounds_changed_callback_(callback),
      ignore_layouts_(false) {}

AutofillDialogViews::DetailsContainerView::~DetailsContainerView() {}

void AutofillDialogViews::DetailsContainerView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  bounds_changed_callback_.Run();
}

void AutofillDialogViews::DetailsContainerView::Layout() {
  if (!ignore_layouts_)
    views::View::Layout();
}

// AutofillDialogViews::SuggestionView -----------------------------------------

AutofillDialogViews::SuggestionView::SuggestionView(
    AutofillDialogViews* autofill_dialog)
    : label_(new views::Label()),
      label_line_2_(new views::Label()),
      icon_(new views::ImageView()),
      decorated_(
          new DecoratedTextfield(base::string16(),
                                 base::string16(),
                                 autofill_dialog)) {
  // TODO(estade): Make this the correct color.
  set_border(
      views::Border::CreateSolidSidedBorder(1, 0, 0, 0, SK_ColorLTGRAY));

  SectionRowView* label_container = new SectionRowView();
  AddChildView(label_container);

  // Label and icon.
  label_container->AddChildView(icon_);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_container->AddChildView(label_);

  // TODO(estade): get the sizing and spacing right on this textfield.
  decorated_->SetVisible(false);
  decorated_->set_default_width_in_chars(15);
  label_container->AddChildView(decorated_);

  label_line_2_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_line_2_->SetVisible(false);
  label_line_2_->SetLineHeight(22);
  label_line_2_->SetMultiLine(true);
  AddChildView(label_line_2_);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 7));
}

AutofillDialogViews::SuggestionView::~SuggestionView() {}

gfx::Size AutofillDialogViews::SuggestionView::GetPreferredSize() {
  // There's no preferred width. The parent's layout should get the preferred
  // height from GetHeightForWidth().
  return gfx::Size();
}

int AutofillDialogViews::SuggestionView::GetHeightForWidth(int width) {
  int height = 0;
  CanUseVerticallyCompactText(width, &height);
  return height;
}

bool AutofillDialogViews::SuggestionView::CanUseVerticallyCompactText(
    int available_width,
    int* resulting_height) {
  // This calculation may be costly, avoid doing it more than once per width.
  if (!calculated_heights_.count(available_width)) {
    // Changing the state of |this| now will lead to extra layouts and
    // paints we don't want, so create another SuggestionView to calculate
    // which label we have room to show.
    SuggestionView sizing_view(NULL);
    sizing_view.SetLabelText(state_.vertically_compact_text);
    sizing_view.SetIcon(state_.icon);
    sizing_view.SetTextfield(state_.extra_text, state_.extra_icon);
    sizing_view.label_->SetSize(gfx::Size(available_width, 0));
    sizing_view.label_line_2_->SetSize(gfx::Size(available_width, 0));

    // Shortcut |sizing_view|'s GetHeightForWidth() to avoid an infinite loop.
    // Its BoxLayout must do these calculations for us.
    views::LayoutManager* layout = sizing_view.GetLayoutManager();
    if (layout->GetPreferredSize(&sizing_view).width() <= available_width) {
      calculated_heights_[available_width] = std::make_pair(
          true,
          layout->GetPreferredHeightForWidth(&sizing_view, available_width));
    } else {
      sizing_view.SetLabelText(state_.horizontally_compact_text);
      calculated_heights_[available_width] = std::make_pair(
          false,
          layout->GetPreferredHeightForWidth(&sizing_view, available_width));
    }
  }

  const std::pair<bool, int>& values = calculated_heights_[available_width];
  *resulting_height = values.second;
  return values.first;
}

void AutofillDialogViews::SuggestionView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  int unused;
  SetLabelText(CanUseVerticallyCompactText(width(), &unused) ?
      state_.vertically_compact_text :
      state_.horizontally_compact_text);
}

void AutofillDialogViews::SuggestionView::SetState(
    const SuggestionState& state) {
  calculated_heights_.clear();
  state_ = state;
  SetVisible(state_.visible);
  // Set to the more compact text for now. |this| will optionally switch to
  // the more vertically expanded view when the bounds are set.
  SetLabelText(state_.vertically_compact_text);
  SetIcon(state_.icon);
  SetTextfield(state_.extra_text, state_.extra_icon);
  PreferredSizeChanged();
}

void AutofillDialogViews::SuggestionView::SetLabelText(
    const base::string16& text) {
  // TODO(estade): does this localize well?
  base::string16 line_return(ASCIIToUTF16("\n"));
  size_t position = text.find(line_return);
  if (position == base::string16::npos) {
    label_->SetText(text);
    label_line_2_->SetVisible(false);
  } else {
    label_->SetText(text.substr(0, position));
    label_line_2_->SetText(text.substr(position + line_return.length()));
    label_line_2_->SetVisible(true);
  }
}

void AutofillDialogViews::SuggestionView::SetIcon(
    const gfx::Image& image) {
  icon_->SetVisible(!image.IsEmpty());
  icon_->SetImage(image.AsImageSkia());
}

void AutofillDialogViews::SuggestionView::SetTextfield(
    const base::string16& placeholder_text,
    const gfx::Image& icon) {
  decorated_->set_placeholder_text(placeholder_text);
  decorated_->SetIcon(icon);
  decorated_->SetVisible(!placeholder_text.empty());
}

// AutofillDialogView ----------------------------------------------------------

// static
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogViewDelegate* delegate) {
  return new AutofillDialogViews(delegate);
}

// AutofillDialogViews ---------------------------------------------------------

AutofillDialogViews::AutofillDialogViews(AutofillDialogViewDelegate* delegate)
    : delegate_(delegate),
      updates_scope_(0),
      needs_update_(false),
      window_(NULL),
      notification_area_(NULL),
      account_chooser_(NULL),
      sign_in_webview_(NULL),
      scrollable_area_(NULL),
      details_container_(NULL),
      loading_shield_(NULL),
      loading_shield_height_(0),
      overlay_view_(NULL),
      button_strip_extra_view_(NULL),
      save_in_chrome_checkbox_(NULL),
      save_in_chrome_checkbox_container_(NULL),
      button_strip_image_(NULL),
      footnote_view_(NULL),
      legal_document_view_(NULL),
      focus_manager_(NULL),
      error_bubble_(NULL),
      observer_(this) {
  DCHECK(delegate);
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
  HideErrorBubble();
  DCHECK(!window_);
}

void AutofillDialogViews::Show() {
  InitChildViews();
  UpdateAccountChooser();
  UpdateNotificationArea();
  UpdateButtonStripExtraView();

  // Ownership of |contents_| is handed off by this call. The widget will take
  // care of deleting itself after calling DeleteDelegate().
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(
          delegate_->GetWebContents());
  WebContentsModalDialogManagerDelegate* modal_delegate =
      web_contents_modal_dialog_manager->delegate();
  DCHECK(modal_delegate);

  window_ = views::Widget::CreateWindowAsFramelessChild(
      this,
      delegate_->GetWebContents()->GetView()->GetNativeView(),
      modal_delegate->GetWebContentsModalDialogHost()->GetHostView());
  web_contents_modal_dialog_manager->ShowDialog(window_->GetNativeView());
  web_contents_modal_dialog_manager->SetCloseOnInterstitialWebUI(
      window_->GetNativeView(), true);
  focus_manager_ = window_->GetFocusManager();
  focus_manager_->AddFocusChangeListener(this);

  // This line fixes http://crbug.com/271779, which happens only on Views/Win32.
  // TODO(rouslan): Remove this line after Windows is entirely on Aura.
  focus_manager_->SetFocusedView(GetInitiallyFocusedView());

  // Listen for size changes on the browser.
  views::Widget* browser_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          delegate_->GetWebContents()->GetView()->GetNativeView());
  observer_.Add(browser_widget);
}

void AutofillDialogViews::Hide() {
  if (window_)
    window_->Close();
}

void AutofillDialogViews::UpdatesStarted() {
  updates_scope_++;
}

void AutofillDialogViews::UpdatesFinished() {
  updates_scope_--;
  DCHECK_GE(updates_scope_, 0);
  if (updates_scope_ == 0 && needs_update_) {
    needs_update_ = false;
    ContentsPreferredSizeChanged();
  }
}

void AutofillDialogViews::UpdateAccountChooser() {
  account_chooser_->Update();

  bool show_loading = delegate_->ShouldShowSpinner();
  if (show_loading != loading_shield_->visible()) {
    if (show_loading) {
      loading_shield_height_ = std::max(kInitialLoadingShieldHeight,
                                        GetContentsBounds().height());
    }

    loading_shield_->SetVisible(show_loading);
    InvalidateLayout();
    ContentsPreferredSizeChanged();
  }

  // Update legal documents for the account.
  if (footnote_view_) {
    const base::string16 text = delegate_->LegalDocumentsText();
    legal_document_view_->SetText(text);

    if (!text.empty()) {
      const std::vector<gfx::Range>& link_ranges =
          delegate_->LegalDocumentLinks();
      for (size_t i = 0; i < link_ranges.size(); ++i) {
        legal_document_view_->AddStyleRange(
            link_ranges[i],
            views::StyledLabel::RangeStyleInfo::CreateForLink());
      }
    }

    footnote_view_->SetVisible(!text.empty());
    ContentsPreferredSizeChanged();
  }

  if (GetWidget())
    GetWidget()->UpdateWindowTitle();
}

void AutofillDialogViews::UpdateButtonStrip() {
  button_strip_extra_view_->SetVisible(
      GetDialogButtons() != ui::DIALOG_BUTTON_NONE);
  UpdateButtonStripExtraView();
  GetDialogClientView()->UpdateDialogButtons();

  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::UpdateOverlay() {
  DialogOverlayState overlay_state = delegate_->GetDialogOverlay();
  overlay_view_->UpdateState();
}

void AutofillDialogViews::UpdateDetailArea() {
  scrollable_area_->SetVisible(true);
  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::UpdateForErrors() {
  ValidateForm();
}

void AutofillDialogViews::UpdateNotificationArea() {
  DCHECK(notification_area_);
  notification_area_->SetNotifications(delegate_->CurrentNotifications());
  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::UpdateSection(DialogSection section) {
  UpdateSectionImpl(section, true);
}

void AutofillDialogViews::UpdateErrorBubble() {
  if (!delegate_->ShouldShowErrorBubble())
    HideErrorBubble();
}

void AutofillDialogViews::FillSection(DialogSection section,
                                      const DetailInput& originating_input) {
  DetailsGroup* group = GroupForSection(section);
  // Make sure to overwrite the originating input.
  TextfieldMap::iterator text_mapping =
      group->textfields.find(&originating_input);
  if (text_mapping != group->textfields.end())
    text_mapping->second->SetText(base::string16());

  // If the Autofill data comes from a credit card, make sure to overwrite the
  // CC comboboxes (even if they already have something in them). If the
  // Autofill data comes from an AutofillProfile, leave the comboboxes alone.
  if ((section == SECTION_CC || section == SECTION_CC_BILLING) &&
      AutofillType(originating_input.type).group() == CREDIT_CARD) {
    for (ComboboxMap::const_iterator it = group->comboboxes.begin();
         it != group->comboboxes.end(); ++it) {
      if (AutofillType(it->first->type).group() == CREDIT_CARD)
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

base::string16 AutofillDialogViews::GetCvc() {
  DialogSection billing_section = delegate_->SectionIsActive(SECTION_CC) ?
      SECTION_CC : SECTION_CC_BILLING;
  return GroupForSection(billing_section)->suggested_info->
      decorated_textfield()->text();
}

bool AutofillDialogViews::HitTestInput(const DetailInput& input,
                                       const gfx::Point& screen_point) {
  views::View* view = TextfieldForInput(input);
  if (!view)
    view = ComboboxForInput(input);

  if (view) {
    gfx::Point target_point(screen_point);
    views::View::ConvertPointFromScreen(view, &target_point);
    return view->HitTestPoint(target_point);
  }

  NOTREACHED();
  return false;
}

bool AutofillDialogViews::SaveDetailsLocally() {
  DCHECK(save_in_chrome_checkbox_->visible());
  return save_in_chrome_checkbox_->checked();
}

const content::NavigationController* AutofillDialogViews::ShowSignIn() {
  // TODO(abodenha) We should be able to use the WebContents of the WebView
  // to navigate instead of LoadInitialURL.  Figure out why it doesn't work.

  sign_in_delegate_.reset(
      new AutofillDialogSignInDelegate(
          this, sign_in_webview_->GetWebContents(),
          delegate_->GetWebContents()->GetDelegate(),
          GetMinimumSignInViewSize(), GetMaximumSignInViewSize()));
  sign_in_webview_->LoadInitialURL(wallet::GetSignInUrl());

  sign_in_webview_->SetVisible(true);
  sign_in_webview_->RequestFocus();
  UpdateButtonStrip();
  ContentsPreferredSizeChanged();
  return &sign_in_webview_->web_contents()->GetController();
}

void AutofillDialogViews::HideSignIn() {
  sign_in_webview_->SetVisible(false);
  UpdateButtonStrip();
  ContentsPreferredSizeChanged();
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
  GetDialogClientView()->CancelWindow();
}

base::string16 AutofillDialogViews::GetTextContentsOfInput(
    const DetailInput& input) {
  views::Textfield* textfield = TextfieldForInput(input);
  if (textfield)
    return textfield->text();

  views::Combobox* combobox = ComboboxForInput(input);
  if (combobox)
    return combobox->model()->GetItemAt(combobox->selected_index());

  NOTREACHED();
  return base::string16();
}

void AutofillDialogViews::SetTextContentsOfInput(
    const DetailInput& input,
    const base::string16& contents) {
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

gfx::Size AutofillDialogViews::GetPreferredSize() {
  if (preferred_size_.IsEmpty())
    preferred_size_ = CalculatePreferredSize(false);

  return preferred_size_;
}

gfx::Size AutofillDialogViews::GetMinimumSize() {
  return CalculatePreferredSize(true);
}

void AutofillDialogViews::Layout() {
  const gfx::Rect content_bounds = GetContentsBounds();
  if (sign_in_webview_->visible()) {
    sign_in_webview_->SetBoundsRect(content_bounds);
    return;
  }

  if (loading_shield_->visible()) {
    loading_shield_->SetBoundsRect(bounds());
    return;
  }

  const int x = content_bounds.x();
  const int y = content_bounds.y();
  const int width = content_bounds.width();
  // Layout notification area at top of dialog.
  int notification_height = notification_area_->GetHeightForWidth(width);
  notification_area_->SetBounds(x, y, width, notification_height);

  // The rest (the |scrollable_area_|) takes up whatever's left.
  if (scrollable_area_->visible()) {
    int scroll_y = y;
    if (notification_height > notification_area_->GetInsets().height())
      scroll_y += notification_height + views::kRelatedControlVerticalSpacing;

    int scroll_bottom = content_bounds.bottom();
    DCHECK_EQ(scrollable_area_->contents(), details_container_);
    details_container_->SizeToPreferredSize();
    // TODO(estade): remove this hack. See crbug.com/285996
    details_container_->set_ignore_layouts(true);
    scrollable_area_->SetBounds(x, scroll_y, width, scroll_bottom - scroll_y);
    details_container_->set_ignore_layouts(false);
  }

  if (error_bubble_)
    error_bubble_->UpdatePosition();
}

base::string16 AutofillDialogViews::GetWindowTitle() const {
  base::string16 title = delegate_->DialogTitle();
  // Hack alert: we don't want the dialog to jiggle when a title is added or
  // removed. Setting a non-empty string here keeps the dialog's title bar the
  // same size.
  return title.empty() ? ASCIIToUTF16(" ") : title;
}

void AutofillDialogViews::WindowClosing() {
  focus_manager_->RemoveFocusChangeListener(this);
}

void AutofillDialogViews::DeleteDelegate() {
  window_ = NULL;
  // |this| belongs to the controller (|delegate_|).
  delegate_->ViewClosed();
}

int AutofillDialogViews::GetDialogButtons() const {
  return delegate_->GetDialogButtons();
}

int AutofillDialogViews::GetDefaultDialogButton() const {
  if (GetDialogButtons() & ui::DIALOG_BUTTON_OK)
    return ui::DIALOG_BUTTON_OK;

  return ui::DIALOG_BUTTON_NONE;
}

base::string16 AutofillDialogViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ?
      delegate_->ConfirmButtonText() : delegate_->CancelButtonText();
}

bool AutofillDialogViews::ShouldDefaultButtonBeBlue() const {
  return true;
}

bool AutofillDialogViews::IsDialogButtonEnabled(ui::DialogButton button) const {
  return delegate_->IsDialogButtonEnabled(button);
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
                           kDialogEdgePadding,
                           kDialogEdgePadding,
                           0));
  footnote_view_->set_border(
      views::Border::CreateSolidSidedBorder(1, 0, 0, 0, kSubtleBorderColor));
  footnote_view_->set_background(
      views::Background::CreateSolidBackground(kShadingColor));

  legal_document_view_ = new views::StyledLabel(base::string16(), this);
  views::StyledLabel::RangeStyleInfo default_style;
  default_style.color = kGreyTextColor;
  legal_document_view_->SetDefaultStyle(default_style);

  footnote_view_->AddChildView(legal_document_view_);
  footnote_view_->SetVisible(false);

  return footnote_view_;
}

views::View* AutofillDialogViews::CreateOverlayView() {
  return overlay_view_;
}

bool AutofillDialogViews::Cancel() {
  return delegate_->OnCancel();
}

bool AutofillDialogViews::Accept() {
  if (ValidateForm())
    return delegate_->OnAccept();

  if (!validity_map_.empty())
    validity_map_.begin()->first->RequestFocus();
  return false;
}

// TODO(wittman): Remove this override once we move to the new style frame view
// on all dialogs.
views::NonClientFrameView* AutofillDialogViews::CreateNonClientFrameView(
    views::Widget* widget) {
  return CreateConstrainedStyleNonClientFrameView(
      widget,
      delegate_->GetWebContents()->GetBrowserContext());
}

void AutofillDialogViews::ContentsChanged(views::Textfield* sender,
                                          const base::string16& new_contents) {
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
  return delegate_->HandleKeyPressEventInInput(event);
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
  delegate_->FocusMoved();
  HideErrorBubble();
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
  if (focused_now) {
    focused_now->ScrollRectToVisible(focused_now->GetLocalBounds());
    ShowErrorBubbleForViewIfNecessary(focused_now);
  }
}

void AutofillDialogViews::OnSelectedIndexChanged(views::Combobox* combobox) {
  DetailsGroup* group = GroupForView(combobox);
  ValidateGroup(*group, VALIDATE_EDIT);
}

void AutofillDialogViews::StyledLabelLinkClicked(const gfx::Range& range,
                                                 int event_flags) {
  delegate_->LegalDocumentLinkClicked(range);
}

void AutofillDialogViews::OnMenuButtonClicked(views::View* source,
                                              const gfx::Point& point) {
  DCHECK_EQ(kSuggestedButtonClassName, source->GetClassName());

  DetailsGroup* group = NULL;
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    if (source == iter->second.suggested_button) {
      group = &iter->second;
      break;
    }
  }
  DCHECK(group);

  if (!group->suggested_button->visible())
    return;

  menu_runner_.reset(new views::MenuRunner(
                         delegate_->MenuModelForSection(group->section)));

  group->container->SetActive(true);
  views::Button::ButtonState state = group->suggested_button->state();
  group->suggested_button->SetState(views::Button::STATE_PRESSED);

  gfx::Rect screen_bounds = source->GetBoundsInScreen();
  screen_bounds.Inset(source->GetInsets());
  if (menu_runner_->RunMenuAt(source->GetWidget(),
                              NULL,
                              screen_bounds,
                              views::MenuItemView::TOPRIGHT,
                              ui::MENU_SOURCE_NONE,
                              0) == views::MenuRunner::MENU_DELETED) {
    return;
  }

  group->container->SetActive(false);
  group->suggested_button->SetState(state);
}

gfx::Size AutofillDialogViews::CalculatePreferredSize(bool get_minimum_size) {
  gfx::Insets insets = GetInsets();
  gfx::Size scroll_size = scrollable_area_->contents()->GetPreferredSize();
  // The width is always set by the scroll area.
  const int width = scroll_size.width();

  if (sign_in_webview_->visible()) {
    const gfx::Size size = static_cast<views::View*>(sign_in_webview_)->
        GetPreferredSize();
    return gfx::Size(width + insets.width(), size.height() + insets.height());
  }

  if (overlay_view_->visible()) {
    const int height = overlay_view_->GetHeightForContentsForWidth(width);
    if (height != 0)
      return gfx::Size(width + insets.width(), height + insets.height());
  }

  if (loading_shield_->visible()) {
    return gfx::Size(width + insets.width(),
                     loading_shield_height_ + insets.height());
  }

  int height = 0;
  const int notification_height = notification_area_->GetHeightForWidth(width);
  if (notification_height > notification_area_->GetInsets().height())
    height += notification_height + views::kRelatedControlVerticalSpacing;

  if (scrollable_area_->visible())
    height += get_minimum_size ? kMinimumContentsHeight : scroll_size.height();

  return gfx::Size(width + insets.width(), height + insets.height());
}

gfx::Size AutofillDialogViews::GetMinimumSignInViewSize() const {
  return gfx::Size(GetDialogClientView()->size().width() - GetInsets().width(),
                   kMinimumContentsHeight);
}

gfx::Size AutofillDialogViews::GetMaximumSignInViewSize() const {
  web_modal::WebContentsModalDialogHost* dialog_host =
      WebContentsModalDialogManager::FromWebContents(
          delegate_->GetWebContents())->delegate()->
              GetWebContentsModalDialogHost();

  // Inset the maximum dialog height to get the maximum content height.
  int height = dialog_host->GetMaximumDialogSize().height();
  const int non_client_height = GetWidget()->non_client_view()->height();
  const int client_height = GetWidget()->client_view()->height();
  // TODO(msw): Resolve the 12 pixel discrepancy; is that the bubble border?
  height -= non_client_height - client_height - 12;
  height = std::max(height, kMinimumContentsHeight);

  // The dialog's width never changes.
  const int width = GetDialogClientView()->size().width() - GetInsets().width();
  return gfx::Size(width, height);
}

void AutofillDialogViews::InitChildViews() {
  button_strip_extra_view_ = new LayoutPropagationView();
  button_strip_extra_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

  save_in_chrome_checkbox_container_ = new views::View();
  save_in_chrome_checkbox_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 7));
  button_strip_extra_view_->AddChildView(save_in_chrome_checkbox_container_);

  save_in_chrome_checkbox_ =
      new views::Checkbox(delegate_->SaveLocallyText());
  save_in_chrome_checkbox_->SetChecked(delegate_->ShouldSaveInChrome());
  save_in_chrome_checkbox_container_->AddChildView(save_in_chrome_checkbox_);

  save_in_chrome_checkbox_container_->AddChildView(
      new TooltipIcon(delegate_->SaveLocallyTooltip()));

  button_strip_image_ = new views::ImageView();
  button_strip_extra_view_->AddChildView(button_strip_image_);

  account_chooser_ = new AccountChooser(delegate_);
  notification_area_ = new NotificationArea(delegate_);
  notification_area_->set_arrow_centering_anchor(account_chooser_->AsWeakPtr());
  AddChildView(notification_area_);

  scrollable_area_ = new views::ScrollView();
  scrollable_area_->set_hide_horizontal_scrollbar(true);
  scrollable_area_->SetContents(CreateDetailsContainer());
  AddChildView(scrollable_area_);

  loading_shield_ = new LoadingAnimationView(delegate_->SpinnerText());
  loading_shield_->SetVisible(false);
  AddChildView(loading_shield_);

  sign_in_webview_ = new views::WebView(delegate_->profile());
  sign_in_webview_->SetVisible(false);
  AddChildView(sign_in_webview_);

  overlay_view_ = new OverlayView(delegate_);
  overlay_view_->SetVisible(false);
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
      delegate_->LabelForSection(section),
      inputs_container,
      group->suggested_button);
  DCHECK(group->suggested_button->parent());
  UpdateDetailsGroupState(*group);
}

views::View* AutofillDialogViews::CreateInputsContainer(DialogSection section) {
  // The |info_view| holds |manual_inputs| and |suggested_info|, allowing the
  // dialog to toggle which is shown.
  views::View* info_view = new views::View();
  info_view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  views::View* manual_inputs = InitInputsView(section);
  info_view->AddChildView(manual_inputs);
  SuggestionView* suggested_info = new SuggestionView(this);
  info_view->AddChildView(suggested_info);

  DetailsGroup* group = GroupForSection(section);
  // TODO(estade): It might be slightly more OO if this button were created
  // and listened to by the section container.
  group->suggested_button = new SuggestedButton(this);
  group->manual_input = manual_inputs;
  group->suggested_info = suggested_info;

  return info_view;
}

// TODO(estade): we should be using Chrome-style constrained window padding
// values.
views::View* AutofillDialogViews::InitInputsView(DialogSection section) {
  const DetailInputs& inputs = delegate_->RequestedFieldsForSection(section);
  TextfieldMap* textfields = &GroupForSection(section)->textfields;
  ComboboxMap* comboboxes = &GroupForSection(section)->comboboxes;

  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  for (DetailInputs::const_iterator it = inputs.begin();
       it != inputs.end(); ++it) {
    const DetailInput& input = *it;
    ui::ComboboxModel* input_model =
        delegate_->ComboboxModelForAutofillType(input.type);
    scoped_ptr<views::View> view_to_add;
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

      view_to_add.reset(combobox);
    } else {
      DecoratedTextfield* field = new DecoratedTextfield(
          input.initial_value,
          l10n_util::GetStringUTF16(input.placeholder_text_rid),
          this);

      textfields->insert(std::make_pair(&input, field));
      view_to_add.reset(field);
    }

    int kColumnSetId = input.row_id;
    if (kColumnSetId < 0) {
      other_owned_views_.push_back(view_to_add.release());
      continue;
    }

    views::ColumnSet* column_set = layout->GetColumnSet(kColumnSetId);
    if (!column_set) {
      // Create a new column set and row.
      column_set = layout->AddColumnSet(kColumnSetId);
      if (it != inputs.begin())
        layout->AddPaddingRow(0, kManualInputRowPadding);
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
                          views::GridLayout::FILL,
                          expand ? expand : 1.0,
                          views::GridLayout::USE_PREF,
                          0,
                          0);

    // This is the same as AddView(view_to_add), except that 1 is used for the
    // view's preferred width. Thus the width of the column completely depends
    // on |expand|.
    layout->AddView(view_to_add.release(), 1, 1,
                    views::GridLayout::FILL, views::GridLayout::FILL,
                    1, 0);
  }

  SetIconsForSection(section);

  return view;
}

void AutofillDialogViews::UpdateSectionImpl(
    DialogSection section,
    bool clobber_inputs) {
  // Reset all validity marks for this section.
  if (clobber_inputs)
    MarkInputsInvalid(section, ValidityMessages(), true);

  const DetailInputs& updated_inputs =
      delegate_->RequestedFieldsForSection(section);
  DetailsGroup* group = GroupForSection(section);

  for (DetailInputs::const_iterator iter = updated_inputs.begin();
       iter != updated_inputs.end(); ++iter) {
    const DetailInput& input = *iter;
    TextfieldMap::iterator text_mapping = group->textfields.find(&input);

    if (text_mapping != group->textfields.end()) {
      DecoratedTextfield* decorated = text_mapping->second;
      decorated->SetEnabled(input.editable);
      if (decorated->text().empty() || clobber_inputs)
        decorated->SetText(iter->initial_value);
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

  SetIconsForSection(section);
  UpdateDetailsGroupState(*group);
}

void AutofillDialogViews::UpdateDetailsGroupState(const DetailsGroup& group) {
  const SuggestionState& suggestion_state =
      delegate_->SuggestionStateForSection(group.section);
  group.suggested_info->SetState(suggestion_state);
  group.manual_input->SetVisible(!suggestion_state.visible);

  UpdateButtonStripExtraView();

  const bool has_menu = !!delegate_->MenuModelForSection(group.section);

  if (group.suggested_button)
    group.suggested_button->SetVisible(has_menu);

  if (group.container) {
    group.container->SetForwardMouseEvents(
        has_menu && suggestion_state.visible);
    group.container->SetVisible(delegate_->SectionIsActive(group.section));
    if (group.container->visible())
      ValidateGroup(group, VALIDATE_EDIT);
  }

  ContentsPreferredSizeChanged();
}

template<class T>
void AutofillDialogViews::SetValidityForInput(
    T* input,
    const base::string16& message) {
  bool invalid = !message.empty();
  input->SetInvalid(invalid);

  if (invalid) {
    validity_map_[input] = message;
  } else {
    validity_map_.erase(input);

    if (error_bubble_ && error_bubble_->anchor() == input) {
      validity_map_.erase(input);
      HideErrorBubble();
    }
  }
}

void AutofillDialogViews::ShowErrorBubbleForViewIfNecessary(views::View* view) {
  if (!view->GetWidget())
    return;

  if (!delegate_->ShouldShowErrorBubble()) {
    DCHECK(!error_bubble_);
    return;
  }

  std::map<views::View*, base::string16>::iterator error_message =
      validity_map_.find(view);
  if (error_message != validity_map_.end()) {
    view->ScrollRectToVisible(view->GetLocalBounds());

    if (!error_bubble_ || error_bubble_->anchor() != view) {
      HideErrorBubble();
      views::View* section =
          view->GetAncestorWithClassName(kSectionContainerClassName);
      error_bubble_ = new ErrorBubble(view, section, error_message->second);
    }
  }
}

void AutofillDialogViews::HideErrorBubble() {
  if (error_bubble_) {
    error_bubble_->Hide();
    error_bubble_ = NULL;
  }
}

void AutofillDialogViews::MarkInputsInvalid(
    DialogSection section,
    const ValidityMessages& messages,
    bool overwrite_unsure) {
  DetailsGroup* group = GroupForSection(section);
  DCHECK(group->container->visible());

  if (group->manual_input->visible()) {
    for (TextfieldMap::const_iterator iter = group->textfields.begin();
         iter != group->textfields.end(); ++iter) {
      const ValidityMessage& message =
          messages.GetMessageOrDefault(iter->first->type);
      if (overwrite_unsure || message.sure)
        SetValidityForInput(iter->second, message.text);
    }
    for (ComboboxMap::const_iterator iter = group->comboboxes.begin();
         iter != group->comboboxes.end(); ++iter) {
      const ValidityMessage& message =
          messages.GetMessageOrDefault(iter->first->type);
      if (overwrite_unsure || message.sure)
        SetValidityForInput(iter->second, message.text);
    }
  } else {
    // Purge invisible views from |validity_map_|.
    std::map<views::View*, base::string16>::iterator it;
    for (it = validity_map_.begin(); it != validity_map_.end();) {
      DCHECK(GroupForView(it->first));
      if (GroupForView(it->first) == group)
        validity_map_.erase(it++);
      else
        ++it;
    }

    if (section == SECTION_CC) {
      // Special case CVC as it's not part of |group->manual_input|.
      const ValidityMessage& message =
          messages.GetMessageOrDefault(CREDIT_CARD_VERIFICATION_CODE);
      if (overwrite_unsure || message.sure) {
        SetValidityForInput(group->suggested_info->decorated_textfield(),
                            message.text);
      }
    }
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
      base::string16 item =
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

  ValidityMessages validity = delegate_->InputsAreValid(group.section,
                                                        detail_outputs);
  MarkInputsInvalid(group.section, validity, validation_type == VALIDATE_FINAL);

  // If there are any validation errors, sure or unsure, the group is invalid.
  return !validity.HasErrors();
}

bool AutofillDialogViews::ValidateForm() {
  bool all_valid = true;
  validity_map_.clear();

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

  // Figure out the ServerFieldType this textfield represents.
  ServerFieldType type = UNKNOWN_TYPE;
  DecoratedTextfield* decorated = NULL;

  // Look for the input in the manual inputs.
  for (TextfieldMap::const_iterator iter = group->textfields.begin();
       iter != group->textfields.end();
       ++iter) {
    decorated = iter->second;
    if (decorated == textfield) {
      delegate_->UserEditedOrActivatedInput(group->section,
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
    SetValidityForInput(
        decorated,
        delegate_->InputValidityMessage(group->section, type,
                                        textfield->text()));

    // If the field transitioned from invalid to valid, re-validate the group,
    // since inter-field checks become meaningful with valid fields.
    if (!decorated->invalid())
      ValidateGroup(*group, VALIDATE_EDIT);
  }

  if (delegate_->FieldControlsIcons(type))
    SetIconsForSection(group->section);
}

void AutofillDialogViews::UpdateButtonStripExtraView() {
  save_in_chrome_checkbox_container_->SetVisible(
      delegate_->ShouldOfferToSaveInChrome());

  gfx::Image image = delegate_->ButtonStripImage();
  button_strip_image_->SetVisible(!image.IsEmpty());
  button_strip_image_->SetImage(image.AsImageSkia());
}

void AutofillDialogViews::ContentsPreferredSizeChanged() {
  if (updates_scope_ != 0) {
    needs_update_ = true;
    return;
  }

  preferred_size_ = gfx::Size();

  if (GetWidget() && delegate_ && delegate_->GetWebContents()) {
    UpdateWebContentsModalDialogPosition(
        GetWidget(),
        WebContentsModalDialogManager::FromWebContents(
            delegate_->GetWebContents())->delegate()->
                GetWebContentsModalDialogHost());
    SetBoundsRect(bounds());
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
        view->GetAncestorWithClassName(DecoratedTextfield::kViewClassName);

    // Textfields need to check a second case, since they can be suggested
    // inputs instead of directly editable inputs. Those are accessed via
    // |suggested_info|.
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

void AutofillDialogViews::SetIconsForSection(DialogSection section) {
  DetailOutputMap user_input;
  GetUserInput(section, &user_input);
  FieldValueMap field_values;
  for (DetailOutputMap::const_iterator user_input_it = user_input.begin();
       user_input_it != user_input.end();
       ++user_input_it) {
    const DetailInput* field_detail = user_input_it->first;
    const string16& field_value = user_input_it->second;
    field_values[field_detail->type] = field_value;
  }
  FieldIconMap field_icons = delegate_->IconsForFields(field_values);
  TextfieldMap* textfields = &GroupForSection(section)->textfields;
  for (TextfieldMap::const_iterator textfield_it = textfields->begin();
       textfield_it != textfields->end();
       ++textfield_it) {
    ServerFieldType field_type = textfield_it->first->type;
    FieldIconMap::const_iterator field_icon_it = field_icons.find(field_type);
    DecoratedTextfield* textfield = textfield_it->second;
    if (field_icon_it != field_icons.end())
      textfield->SetIcon(field_icon_it->second);
    else
      textfield->SetTooltipIcon(delegate_->TooltipForField(field_type));
  }
}

AutofillDialogViews::DetailsGroup::DetailsGroup(DialogSection section)
    : section(section),
      container(NULL),
      manual_input(NULL),
      suggested_info(NULL),
      suggested_button(NULL) {}

AutofillDialogViews::DetailsGroup::~DetailsGroup() {}

}  // namespace autofill
