// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_views.h"

#include "ash/ash_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/i18n/rtl.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/events/event.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector2d.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"

namespace ash {
namespace internal {

namespace {
const int kIconPaddingLeft = 5;
const int kPopupDetailLabelExtraLeftMargin = 8;
const int kCheckLabelPadding = 4;
const int kSpecialPopupRowHeight = 55;
const int kTrayPopupLabelButtonPaddingHorizontal = 16;
const int kTrayPopupLabelButtonPaddingVertical = 8;

const int kBarImagesActive[] = {
    IDR_SLIDER_ACTIVE_LEFT,
    IDR_SLIDER_ACTIVE_CENTER,
    IDR_SLIDER_ACTIVE_RIGHT,
};

const int kBarImagesDisabled[] = {
    IDR_SLIDER_DISABLED_LEFT,
    IDR_SLIDER_DISABLED_CENTER,
    IDR_SLIDER_DISABLED_RIGHT,
};

const int kTrayPopupLabelButtonBorderImagesNormal[] = {
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
};
const int kTrayPopupLabelButtonBorderImagesHovered[] = {
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_HOVER_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
};

views::View* CreatePopupHeaderButtonsContainer() {
  views::View* view = new views::View;
  view->SetLayoutManager(new
      views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, -1));
  view->set_border(views::Border::CreateEmptyBorder(0, 0, 0, 5));
  return view;
}

const int kBorderHeight = 3;
const SkColor kBorderGradientDark = SkColorSetRGB(0xae, 0xae, 0xae);
const SkColor kBorderGradientLight = SkColorSetRGB(0xe8, 0xe8, 0xe8);

class SpecialPopupRowBorder : public views::Border {
 public:
  SpecialPopupRowBorder()
      : painter_(views::Painter::CreateVerticalGradient(kBorderGradientDark,
                                                        kBorderGradientLight)) {
  }

  virtual ~SpecialPopupRowBorder() {}

 private:
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE {
    views::Painter::PaintPainterAt(canvas, painter_.get(),
        gfx::Rect(gfx::Size(view.width(), kBorderHeight)));
  }

  virtual gfx::Insets GetInsets() const OVERRIDE {
    return gfx::Insets(kBorderHeight, 0, 0, 0);
  }

  scoped_ptr<views::Painter> painter_;

  DISALLOW_COPY_AND_ASSIGN(SpecialPopupRowBorder);
};

}

////////////////////////////////////////////////////////////////////////////////
// FixedSizedImageView

FixedSizedImageView::FixedSizedImageView(int width, int height)
    : width_(width),
      height_(height) {
  SetHorizontalAlignment(views::ImageView::CENTER);
  SetVerticalAlignment(views::ImageView::CENTER);
}

FixedSizedImageView::~FixedSizedImageView() {
}

gfx::Size FixedSizedImageView::GetPreferredSize() {
  gfx::Size size = views::ImageView::GetPreferredSize();
  return gfx::Size(width_ ? width_ : size.width(),
                   height_ ? height_ : size.height());
}

////////////////////////////////////////////////////////////////////////////////
// ActionableView

ActionableView::ActionableView()
    : has_capture_(false) {
  set_focusable(true);
}

ActionableView::~ActionableView() {
}

void ActionableView::DrawBorder(gfx::Canvas* canvas, const gfx::Rect& bounds) {
  gfx::Rect rect = bounds;
  rect.Inset(1, 1, 3, 3);
  canvas->DrawRect(rect, kFocusBorderColor);
}

bool ActionableView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    return PerformAction(event);
  }
  return false;
}

bool ActionableView::OnMousePressed(const ui::MouseEvent& event) {
  // Return true so that this view starts capturing the events.
  has_capture_ = true;
  return true;
}

void ActionableView::OnMouseReleased(const ui::MouseEvent& event) {
  if (has_capture_ && GetLocalBounds().Contains(event.location()))
    PerformAction(event);
}

void ActionableView::OnMouseCaptureLost() {
  has_capture_ = false;
}

void ActionableView::SetAccessibleName(const string16& name) {
  accessible_name_ = name;
}

void ActionableView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusable()))
    DrawBorder(canvas, GetLocalBounds());
}

void ActionableView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP && PerformAction(*event))
    event->SetHandled();
}

void ActionableView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = accessible_name_;
}

////////////////////////////////////////////////////////////////////////////////
// HoverHighlightView

HoverHighlightView::HoverHighlightView(ViewClickListener* listener)
    : listener_(listener),
      text_label_(NULL),
      highlight_color_(kHoverBackgroundColor),
      default_color_(0),
      text_highlight_color_(0),
      text_default_color_(0),
      fixed_height_(0),
      hover_(false) {
  set_notify_enter_exit_on_child(true);
}

HoverHighlightView::~HoverHighlightView() {
}

void HoverHighlightView::AddIconAndLabel(const gfx::ImageSkia& image,
                                         const string16& text,
                                         gfx::Font::FontStyle style) {
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 3, kTrayPopupPaddingBetweenItems));
  views::ImageView* image_view =
      new FixedSizedImageView(kTrayPopupDetailsIconWidth, 0);
  image_view->SetImage(image);
  AddChildView(image_view);

  text_label_ = new views::Label(text);
  text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label_->SetFont(text_label_->font().DeriveFont(0, style));
  if (text_default_color_)
    text_label_->SetEnabledColor(text_default_color_);
  AddChildView(text_label_);

  SetAccessibleName(text);
}

views::Label* HoverHighlightView::AddLabel(const string16& text,
                                           gfx::Font::FontStyle style) {
  SetLayoutManager(new views::FillLayout());
  text_label_ = new views::Label(text);
  int margin = kTrayPopupPaddingHorizontal + kPopupDetailLabelExtraLeftMargin;
  int left_margin = 0;
  int right_margin = 0;
  if (base::i18n::IsRTL())
    right_margin = margin;
  else
    left_margin = margin;
  text_label_->set_border(
      views::Border::CreateEmptyBorder(5, left_margin, 5, right_margin));
  text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label_->SetFont(text_label_->font().DeriveFont(0, style));
  // Do not set alpha value in disable color. It will have issue with elide
  // blending filter in disabled state for rendering label text color.
  text_label_->SetDisabledColor(SkColorSetARGB(255, 127, 127, 127));
  if (text_default_color_)
    text_label_->SetEnabledColor(text_default_color_);
  AddChildView(text_label_);

  SetAccessibleName(text);
  return text_label_;
}

views::Label* HoverHighlightView::AddCheckableLabel(const string16& text,
                                                    gfx::Font::FontStyle style,
                                                    bool checked) {
  if (checked) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::ImageSkia* check =
        rb.GetImageNamed(IDR_MENU_CHECK).ToImageSkia();
    int margin = kTrayPopupPaddingHorizontal + kPopupDetailLabelExtraLeftMargin
        - kCheckLabelPadding;
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 3, kCheckLabelPadding));
    views::ImageView* image_view = new FixedSizedImageView(margin, 0);
    image_view->SetImage(check);
    image_view->SetHorizontalAlignment(views::ImageView::TRAILING);
    AddChildView(image_view);

    text_label_ = new views::Label(text);
    text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    text_label_->SetFont(text_label_->font().DeriveFont(0, style));
    text_label_->SetDisabledColor(SkColorSetARGB(127, 0, 0, 0));
    if (text_default_color_)
      text_label_->SetEnabledColor(text_default_color_);
    AddChildView(text_label_);

    SetAccessibleName(text);
    return text_label_;
  } else {
    return AddLabel(text, style);
  }
}

bool HoverHighlightView::PerformAction(const ui::Event& event) {
  if (!listener_)
    return false;
  listener_->ClickedOn(this);
  return true;
}

gfx::Size HoverHighlightView::GetPreferredSize() {
  gfx::Size size = ActionableView::GetPreferredSize();
  if (fixed_height_)
    size.set_height(fixed_height_);
  return size;
}

void HoverHighlightView::OnMouseEntered(const ui::MouseEvent& event) {
  hover_ = true;
  if (text_highlight_color_ && text_label_)
    text_label_->SetEnabledColor(text_highlight_color_);
  SchedulePaint();
}

void HoverHighlightView::OnMouseExited(const ui::MouseEvent& event) {
  hover_ = false;
  if (text_default_color_ && text_label_)
    text_label_->SetEnabledColor(text_default_color_);
  SchedulePaint();
}

void HoverHighlightView::OnEnabledChanged() {
  for (int i = 0; i < child_count(); ++i)
    child_at(i)->SetEnabled(enabled());
}

void HoverHighlightView::OnPaintBackground(gfx::Canvas* canvas) {
  canvas->DrawColor(hover_ ? highlight_color_ : default_color_);
}

void HoverHighlightView::OnFocus() {
  ScrollRectToVisible(gfx::Rect(gfx::Point(), size()));
  ActionableView::OnFocus();
}

////////////////////////////////////////////////////////////////////////////////
// FixedSizedScrollView

FixedSizedScrollView::FixedSizedScrollView() {
  set_focusable(true);
  set_notify_enter_exit_on_child(true);
}

FixedSizedScrollView::~FixedSizedScrollView() {}

void FixedSizedScrollView::SetContentsView(View* view) {
  SetContents(view);
  view->SetBoundsRect(gfx::Rect(view->GetPreferredSize()));
}

void FixedSizedScrollView::SetFixedSize(const gfx::Size& size) {
  if (fixed_size_ == size)
    return;
  fixed_size_ = size;
  PreferredSizeChanged();
}

gfx::Size FixedSizedScrollView::GetPreferredSize() {
  gfx::Size size = fixed_size_.IsEmpty() ?
      contents()->GetPreferredSize() : fixed_size_;
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void FixedSizedScrollView::Layout() {
  gfx::Rect bounds = gfx::Rect(contents()->GetPreferredSize());
  bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
  contents()->SetBoundsRect(bounds);

  views::ScrollView::Layout();
  if (!vertical_scroll_bar()->visible()) {
    gfx::Rect bounds = contents()->bounds();
    bounds.set_width(bounds.width() + GetScrollBarWidth());
    contents()->SetBoundsRect(bounds);
  }
}

void FixedSizedScrollView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  gfx::Rect bounds = gfx::Rect(contents()->GetPreferredSize());
  bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
  contents()->SetBoundsRect(bounds);
}

void FixedSizedScrollView::OnMouseEntered(const ui::MouseEvent& event) {
  // TODO(sad): This is done to make sure that the scroll view scrolls on
  // mouse-wheel events. This is ugly, and Ben thinks this is weird. There
  // should be a better fix for this.
  RequestFocus();
}

void FixedSizedScrollView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  // Do not paint the focus border.
}

////////////////////////////////////////////////////////////////////////////////
// TrayPopupLabelButtonBorder

TrayPopupLabelButtonBorder::TrayPopupLabelButtonBorder() {
  SetPainter(views::CustomButton::STATE_NORMAL,
             views::Painter::CreateImageGridPainter(
                 kTrayPopupLabelButtonBorderImagesNormal));
  SetPainter(views::CustomButton::STATE_DISABLED,
             views::Painter::CreateImageGridPainter(
                 kTrayPopupLabelButtonBorderImagesNormal));
  SetPainter(views::CustomButton::STATE_HOVERED,
             views::Painter::CreateImageGridPainter(
                 kTrayPopupLabelButtonBorderImagesHovered));
  SetPainter(views::CustomButton::STATE_PRESSED,
             views::Painter::CreateImageGridPainter(
                 kTrayPopupLabelButtonBorderImagesHovered));
}

TrayPopupLabelButtonBorder::~TrayPopupLabelButtonBorder() {}

void TrayPopupLabelButtonBorder::Paint(const views::View& view,
                                       gfx::Canvas* canvas) {
  const views::NativeThemeDelegate* native_theme_delegate =
      static_cast<const views::LabelButton*>(&view);
  ui::NativeTheme::ExtraParams extra;
  const ui::NativeTheme::State state =
      native_theme_delegate->GetThemeState(&extra);
  if (state == ui::NativeTheme::kNormal ||
      state == ui::NativeTheme::kDisabled) {
    // In normal and disabled state, the border is a vertical bar separating the
    // button from the preceding sibling. If this button is its parent's first
    // visible child, the separator bar should be omitted.
    const views::View* first_visible_child = NULL;
    for (int i = 0; i < view.parent()->child_count(); ++i) {
      const views::View* child = view.parent()->child_at(i);
      if (child->visible()) {
        first_visible_child = child;
        break;
      }
    }
    if (first_visible_child == &view)
      return;
  }
  if (base::i18n::IsRTL()) {
    canvas->Save();
    canvas->Translate(gfx::Vector2d(view.width(), 0));
    canvas->Scale(-1, 1);
    LabelButtonBorder::Paint(view, canvas);
    canvas->Restore();
  } else {
    LabelButtonBorder::Paint(view, canvas);
  }
}

gfx::Insets TrayPopupLabelButtonBorder::GetInsets() const {
  return gfx::Insets(kTrayPopupLabelButtonPaddingVertical,
                     kTrayPopupLabelButtonPaddingHorizontal,
                     kTrayPopupLabelButtonPaddingVertical,
                     kTrayPopupLabelButtonPaddingHorizontal);
}

////////////////////////////////////////////////////////////////////////////////
// TrayPopupLabelButton

TrayPopupLabelButton::TrayPopupLabelButton(views::ButtonListener* listener,
                                           const string16& text)
    : views::LabelButton(listener, text) {
  set_border(new TrayPopupLabelButtonBorder);
  set_focusable(true);
  set_request_focus_on_press(false);
  set_animate_on_state_change(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

TrayPopupLabelButton::~TrayPopupLabelButton() {}

void TrayPopupLabelButton::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
    canvas->DrawRect(gfx::Rect(1, 1, width() - 3, height() - 3),
                     ash::kFocusBorderColor);
  }
}

////////////////////////////////////////////////////////////////////////////////
// TrayPopupHeaderButton

TrayPopupHeaderButton::TrayPopupHeaderButton(views::ButtonListener* listener,
                                             int enabled_resource_id,
                                             int disabled_resource_id,
                                             int enabled_resource_id_hover,
                                             int disabled_resource_id_hover,
                                             int accessible_name_id)
    : views::ToggleImageButton(listener) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  SetImage(views::CustomButton::STATE_NORMAL,
      bundle.GetImageNamed(enabled_resource_id).ToImageSkia());
  SetToggledImage(views::CustomButton::STATE_NORMAL,
      bundle.GetImageNamed(disabled_resource_id).ToImageSkia());
  SetImage(views::CustomButton::STATE_HOVERED,
      bundle.GetImageNamed(enabled_resource_id_hover).ToImageSkia());
  SetToggledImage(views::CustomButton::STATE_HOVERED,
      bundle.GetImageNamed(disabled_resource_id_hover).ToImageSkia());
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  SetAccessibleName(bundle.GetLocalizedString(accessible_name_id));
  set_focusable(true);
  set_request_focus_on_press(false);
}

TrayPopupHeaderButton::~TrayPopupHeaderButton() {}

gfx::Size TrayPopupHeaderButton::GetPreferredSize() {
  return gfx::Size(ash::kTrayPopupItemHeight, ash::kTrayPopupItemHeight);
}

void TrayPopupHeaderButton::OnPaintBorder(gfx::Canvas* canvas) {
  // Just the left border.
  const int kBorderHeight = 25;
  int padding = (height() - kBorderHeight) / 2;
  canvas->FillRect(gfx::Rect(0, padding, 1, height() - padding * 2),
      ash::kBorderDarkColor);
}

void TrayPopupHeaderButton::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
    canvas->DrawRect(gfx::Rect(2, 1, width() - 4, height() - 3),
                     kFocusBorderColor);
  }
}

void TrayPopupHeaderButton::StateChanged() {
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// TrayBarButtonWithTitle

class TrayBarButtonWithTitle::TrayBarButton
  : public views::View {
 public:
  TrayBarButton(const int bar_active_images[], const int bar_disabled_images[])
    : views::View(),
      bar_active_images_(bar_active_images),
      bar_disabled_images_(bar_disabled_images),
      painter_(new views::HorizontalPainter(bar_active_images_)){
  }
  virtual ~TrayBarButton() {}

  // Overriden from views::View
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    painter_->Paint(canvas, size());
  }

  void Update(bool control_on) {
    painter_.reset(new views::HorizontalPainter(
        control_on ? bar_active_images_ : bar_disabled_images_));
    SchedulePaint();
  }

 private:
  const int* bar_active_images_;
  const int* bar_disabled_images_;
  scoped_ptr<views::HorizontalPainter> painter_;

  DISALLOW_COPY_AND_ASSIGN(TrayBarButton);
};

TrayBarButtonWithTitle::TrayBarButtonWithTitle(views::ButtonListener* listener,
                                               int title_id,
                                               int width)
    : views::CustomButton(listener),
      image_(new TrayBarButton(kBarImagesActive, kBarImagesDisabled)),
      title_(NULL),
      width_(width) {
  AddChildView(image_);
  if (title_id != -1) {
    title_ = new views::Label;
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    string16 text = rb.GetLocalizedString(title_id);
    title_->SetText(text);
    AddChildView(title_);
  }

  image_height_ = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      kBarImagesActive[0]).ToImageSkia()->height();
}

TrayBarButtonWithTitle::~TrayBarButtonWithTitle() {}

gfx::Size TrayBarButtonWithTitle::GetPreferredSize() {
  return gfx::Size(width_, kTrayPopupItemHeight);
}

void TrayBarButtonWithTitle::Layout() {
  gfx::Rect rect(GetContentsBounds());
  int bar_image_y = rect.height() / 2 - image_height_ / 2;
  gfx::Rect bar_image_rect(rect.x(),
                           bar_image_y,
                           rect.width(),
                           image_height_);
  image_->SetBoundsRect(bar_image_rect);
  if (title_) {
    // The image_ has some empty space below the bar image, move the title
    // a little bit up to look closer to the bar.
    gfx::Size title_size = title_->GetPreferredSize();
    title_->SetBounds(rect.x(),
                      bar_image_y + image_height_ - 3,
                      rect.width(),
                      title_size.height());
  }
}

void TrayBarButtonWithTitle::UpdateButton(bool control_on) {
  image_->Update(control_on);
}

////////////////////////////////////////////////////////////////////////////////
// SpecialPopupRow

SpecialPopupRow::SpecialPopupRow()
    : content_(NULL),
      button_container_(NULL) {
  views::Background* background = views::Background::CreateBackgroundPainter(
      true, views::Painter::CreateVerticalGradient(kHeaderBackgroundColorLight,
                                                   kHeaderBackgroundColorDark));
  background->SetNativeControlColor(kHeaderBackgroundColorDark);
  set_background(background);
  set_border(new SpecialPopupRowBorder);
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
}

SpecialPopupRow::~SpecialPopupRow() {
}

void SpecialPopupRow::SetTextLabel(int string_id, ViewClickListener* listener) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  HoverHighlightView* container = new HoverHighlightView(listener);
  container->set_fixed_height(kTrayPopupItemHeight);
  container->SetLayoutManager(new
      views::BoxLayout(views::BoxLayout::kHorizontal, 0, 3, kIconPaddingLeft));

  container->set_highlight_color(SkColorSetARGB(0, 0, 0, 0));
  container->set_default_color(SkColorSetARGB(0, 0, 0, 0));
  container->set_text_highlight_color(kHeaderTextColorHover);
  container->set_text_default_color(kHeaderTextColorNormal);

  container->AddIconAndLabel(
      *rb.GetImageNamed(IDR_AURA_UBER_TRAY_LESS).ToImageSkia(),
      rb.GetLocalizedString(string_id),
      gfx::Font::BOLD);

  container->set_border(views::Border::CreateEmptyBorder(0,
      kTrayPopupPaddingHorizontal, 0, 0));

  container->SetAccessibleName(
      rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_PREVIOUS_MENU));
  SetContent(container);
}

void SpecialPopupRow::SetContent(views::View* view) {
  CHECK(!content_);
  content_ = view;
  AddChildViewAt(content_, 0);
}

void SpecialPopupRow::AddButton(TrayPopupHeaderButton* button) {
  if (!button_container_) {
    button_container_ = CreatePopupHeaderButtonsContainer();
    AddChildView(button_container_);
  }

  button_container_->AddChildView(button);
}

gfx::Size SpecialPopupRow::GetPreferredSize() {
  gfx::Size size = views::View::GetPreferredSize();
  size.set_height(kSpecialPopupRowHeight);
  return size;
}

void SpecialPopupRow::Layout() {
  views::View::Layout();
  gfx::Rect content_bounds = GetContentsBounds();
  if (content_bounds.IsEmpty())
    return;
  if (!button_container_) {
    content_->SetBoundsRect(GetContentsBounds());
    return;
  }

  gfx::Rect bounds(button_container_->GetPreferredSize());
  bounds.set_height(content_bounds.height());
  gfx::Rect container_bounds = content_bounds;
  container_bounds.ClampToCenteredSize(bounds.size());
  container_bounds.set_x(content_bounds.width() - container_bounds.width());
  button_container_->SetBoundsRect(container_bounds);

  bounds = content_->bounds();
  bounds.set_width(button_container_->x());
  content_->SetBoundsRect(bounds);
}

void SetupLabelForTray(views::Label* label) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  label->SetFont(rb.GetFont(ui::ResourceBundle::BoldFont));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(SK_ColorWHITE);
  label->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
  label->SetShadowColors(SkColorSetARGB(64, 0, 0, 0),
                         SkColorSetARGB(64, 0, 0, 0));
  label->SetShadowOffset(0, 1);
}

void SetTrayImageItemBorder(views::View* tray_view,
                            ShelfAlignment alignment) {
  if (alignment == SHELF_ALIGNMENT_BOTTOM) {
    tray_view->set_border(views::Border::CreateEmptyBorder(
        0, kTrayImageItemHorizontalPaddingBottomAlignment,
        0, kTrayImageItemHorizontalPaddingBottomAlignment));
  } else {
    tray_view->set_border(views::Border::CreateEmptyBorder(
        kTrayImageItemVerticalPaddingVerticalAlignment,
        kTrayImageItemHorizontalPaddingVerticalAlignment,
        kTrayImageItemVerticalPaddingVerticalAlignment,
        kTrayImageItemHorizontalPaddingVerticalAlignment));
  }
}

void SetTrayLabelItemBorder(TrayItemView* tray_view,
                            ShelfAlignment alignment) {
  if (alignment == SHELF_ALIGNMENT_BOTTOM) {
    tray_view->set_border(views::Border::CreateEmptyBorder(
        0, kTrayLabelItemHorizontalPaddingBottomAlignment,
        0, kTrayLabelItemHorizontalPaddingBottomAlignment));
  } else {
    // Center the label for vertical launcher alignment.
    int horizontal_padding = (tray_view->GetPreferredSize().width() -
        tray_view->label()->GetPreferredSize().width()) / 2;
    tray_view->set_border(views::Border::CreateEmptyBorder(
        kTrayLabelItemVerticalPaddingVeriticalAlignment,
        horizontal_padding,
        kTrayLabelItemVerticalPaddingVeriticalAlignment,
        horizontal_padding));
  }
}

}  // namespace internal
}  // namespace ash
