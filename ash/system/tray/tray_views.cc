// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_views.h"

#include "ash/system/tray/tray_constants.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources_standard.h"
#include "grit/ui_resources_standard.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
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
const int kPaddingAroundButtons = 5;

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
  virtual void Paint(const views::View& view,
                     gfx::Canvas* canvas) const OVERRIDE {
    views::Painter::PaintPainterAt(canvas, painter_.get(),
        gfx::Rect(gfx::Size(view.width(), kBorderHeight)));
  }

  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE {
    insets->Set(kBorderHeight, 0, 0, 0);
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

bool ActionableView::OnKeyPressed(const views::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    return PerformAction(event);
  }
  return false;
}

bool ActionableView::OnMousePressed(const views::MouseEvent& event) {
  // Return true so that this view starts capturing the events.
  has_capture_ = true;
  return true;
}

void ActionableView::OnMouseReleased(const views::MouseEvent& event) {
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
  if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
    canvas->DrawRect(gfx::Rect(1, 1, width() - 3, height() - 3),
                     kFocusBorderColor);
  }
}

ui::GestureStatus ActionableView::OnGestureEvent(
    const views::GestureEvent& event) {
  if (event.type() == ui::ET_GESTURE_TAP) {
    return PerformAction(event) ? ui::GESTURE_STATUS_CONSUMED :
                                  ui::GESTURE_STATUS_UNKNOWN;
  }
  return ui::GESTURE_STATUS_UNKNOWN;
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
      views::BoxLayout::kHorizontal, 0, 3, kIconPaddingLeft));
  views::ImageView* image_view =
      new FixedSizedImageView(kTrayPopupDetailsIconWidth, 0);
  image_view->SetImage(image);
  AddChildView(image_view);

  text_label_ = new views::Label(text);
  text_label_->SetFont(text_label_->font().DeriveFont(0, style));
  if (text_default_color_)
    text_label_->SetEnabledColor(text_default_color_);
  AddChildView(text_label_);

  SetAccessibleName(text);
}

void HoverHighlightView::AddLabel(const string16& text,
                                  gfx::Font::FontStyle style) {
  SetLayoutManager(new views::FillLayout());
  text_label_ = new views::Label(text);
  text_label_->set_border(views::Border::CreateEmptyBorder(
      5, kTrayPopupDetailsIconWidth + kIconPaddingLeft, 5, 0));
  text_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  text_label_->SetFont(text_label_->font().DeriveFont(0, style));
  text_label_->SetDisabledColor(SkColorSetARGB(127, 0, 0, 0));
  if (text_default_color_)
    text_label_->SetEnabledColor(text_default_color_);
  AddChildView(text_label_);

  SetAccessibleName(text);
}

bool HoverHighlightView::PerformAction(const views::Event& event) {
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

void HoverHighlightView::OnMouseEntered(const views::MouseEvent& event) {
  hover_ = true;
  if (text_highlight_color_ && text_label_)
    text_label_->SetEnabledColor(text_highlight_color_);
  SchedulePaint();
}

void HoverHighlightView::OnMouseExited(const views::MouseEvent& event) {
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

void FixedSizedScrollView::SetFixedSize(gfx::Size size) {
  if (fixed_size_ == size)
    return;
  fixed_size_ = size;
  PreferredSizeChanged();
}

gfx::Size FixedSizedScrollView::GetPreferredSize() {
  gfx::Size size = fixed_size_.IsEmpty() ?
      GetContents()->GetPreferredSize() : fixed_size_;
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void FixedSizedScrollView::Layout() {
  views::View* contents = GetContents();
  gfx::Rect bounds = gfx::Rect(contents->GetPreferredSize());
  bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
  contents->SetBoundsRect(bounds);

  views::ScrollView::Layout();
  if (!vertical_scroll_bar()->visible()) {
    gfx::Rect bounds = contents->bounds();
    bounds.set_width(bounds.width() + GetScrollBarWidth());
    contents->SetBoundsRect(bounds);
  }
}

void FixedSizedScrollView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::View* contents = GetContents();
  gfx::Rect bounds = gfx::Rect(contents->GetPreferredSize());
  bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
  contents->SetBoundsRect(bounds);
}

void FixedSizedScrollView::OnMouseEntered(const views::MouseEvent& event) {
  // TODO(sad): This is done to make sure that the scroll view scrolls on
  // mouse-wheel events. This is ugly, and Ben thinks this is weird. There
  // should be a better fix for this.
  RequestFocus();
}

void FixedSizedScrollView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  // Do not paint the focus border.
}

////////////////////////////////////////////////////////////////////////////////
// TrayPopupTextButton

TrayPopupTextButton::TrayPopupTextButton(views::ButtonListener* listener,
                                         const string16& text)
    : views::TextButton(listener, text),
      hover_(false),
      hover_bg_(views::Background::CreateSolidBackground(SkColorSetARGB(
             10, 0, 0, 0))),
      hover_border_(views::Border::CreateSolidBorder(1, kButtonStrokeColor)) {
  set_alignment(ALIGN_CENTER);
  set_border(NULL);
  set_focusable(true);
}

TrayPopupTextButton::~TrayPopupTextButton() {}

gfx::Size TrayPopupTextButton::GetPreferredSize() {
  gfx::Size size = views::TextButton::GetPreferredSize();
  size.Enlarge(32, 16);
  return size;
}

void TrayPopupTextButton::OnMouseEntered(const views::MouseEvent& event) {
  hover_ = true;
  SchedulePaint();
}

void TrayPopupTextButton::OnMouseExited(const views::MouseEvent& event) {
  hover_ = false;
  SchedulePaint();
}

void TrayPopupTextButton::OnPaintBackground(gfx::Canvas* canvas) {
  if (hover_)
    hover_bg_->Paint(canvas, this);
  else
    views::TextButton::OnPaintBackground(canvas);
}

void TrayPopupTextButton::OnPaintBorder(gfx::Canvas* canvas) {
  if (hover_)
    hover_border_->Paint(*this, canvas);
  else
    views::TextButton::OnPaintBorder(canvas);
}

void TrayPopupTextButton::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
    canvas->DrawRect(gfx::Rect(1, 1, width() - 3, height() - 3),
                     ash::kFocusBorderColor);
  }
}

////////////////////////////////////////////////////////////////////////////////
// TrayPopupTextButtonContainer

TrayPopupTextButtonContainer::TrayPopupTextButtonContainer() {
  layout_ = new
    views::BoxLayout(views::BoxLayout::kHorizontal,
        kPaddingAroundButtons,
        kPaddingAroundButtons,
        -1);
  layout_->set_spread_blank_space(true);
  SetLayoutManager(layout_);
}

TrayPopupTextButtonContainer::~TrayPopupTextButtonContainer() {
}

void TrayPopupTextButtonContainer::AddTextButton(TrayPopupTextButton* button) {
  if (has_children() && !button->border()) {
    button->set_border(views::Border::CreateSolidSidedBorder(0, 1, 0, 0,
        kButtonStrokeColor));
  }
  AddChildView(button);
}

////////////////////////////////////////////////////////////////////////////////
// TrayPopupHeaderButton

TrayPopupHeaderButton::TrayPopupHeaderButton(views::ButtonListener* listener,
                                             int enabled_resource_id,
                                             int disabled_resource_id,
                                             int enabled_resource_id_hover,
                                             int disabled_resource_id_hover)
    : views::ToggleImageButton(listener) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  SetImage(views::CustomButton::BS_NORMAL,
      bundle.GetImageNamed(enabled_resource_id).ToImageSkia());
  SetToggledImage(views::CustomButton::BS_NORMAL,
      bundle.GetImageNamed(disabled_resource_id).ToImageSkia());
  SetImage(views::CustomButton::BS_HOT,
      bundle.GetImageNamed(enabled_resource_id_hover).ToImageSkia());
  SetToggledImage(views::CustomButton::BS_HOT,
      bundle.GetImageNamed(disabled_resource_id_hover).ToImageSkia());
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  set_focusable(true);
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
  const int kFixedHeight = 55;
  gfx::Size size = views::View::GetPreferredSize();
  size.set_height(kFixedHeight);
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
  bounds = content_bounds.Center(bounds.size());
  bounds.set_x(content_bounds.width() - bounds.width());
  button_container_->SetBoundsRect(bounds);

  bounds = content_->bounds();
  bounds.set_width(button_container_->x());
  content_->SetBoundsRect(bounds);
}

////////////////////////////////////////////////////////////////////////////////
// TrayNotificationView

TrayNotificationView::TrayNotificationView(int icon_id) : icon_id_(icon_id) {
}

TrayNotificationView::~TrayNotificationView() {
}

void TrayNotificationView::InitView(views::View* contents) {
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  views::ImageButton* close_button = new views::ImageButton(this);
  close_button->SetImage(views::CustomButton::BS_NORMAL,
                         ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                             IDR_AURA_WINDOW_CLOSE));

  icon_ = new views::ImageView;
  if (icon_id_ != 0) {
    icon_->SetImage(
        ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_id_));
  }

  views::ColumnSet* columns = layout->AddColumnSet(0);

  columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal/2);

  // Icon
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                     0, /* resize percent */
                     views::GridLayout::FIXED,
                     kNotificationIconWidth, kNotificationIconWidth);

  columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal/2);

  // Contents
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     0, /* resize percent */
                     views::GridLayout::FIXED,
                     kTrayNotificationContentsWidth,
                     kTrayNotificationContentsWidth);

  columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal/2);

  // Close button
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                     0, /* resize percent */
                     views::GridLayout::FIXED,
                     kNotificationIconWidth, kNotificationIconWidth);

  columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal/2);

  // Layout rows
  layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);
  layout->StartRow(0, 0);
  layout->AddView(icon_);
  layout->AddView(contents);
  layout->AddView(close_button);
  layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);
}

void TrayNotificationView::SetIconImage(const gfx::ImageSkia& image) {
  icon_->SetImage(image);
  SchedulePaint();
}

void TrayNotificationView::UpdateView(views::View* new_contents) {
  RemoveAllChildViews(true);
  InitView(new_contents);
  Layout();
  PreferredSizeChanged();
  SchedulePaint();
}

void TrayNotificationView::UpdateViewAndImage(views::View* new_contents,
                                              const gfx::ImageSkia& image) {
  RemoveAllChildViews(true);
  InitView(new_contents);
  icon_->SetImage(image);
  Layout();
  PreferredSizeChanged();
  SchedulePaint();
}

void TrayNotificationView::ButtonPressed(views::Button* sender,
                                         const views::Event& event) {
  OnClose();
}

void SetupLabelForTray(views::Label* label) {
  label->SetFont(
      label->font().DeriveFont(2, gfx::Font::BOLD));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(SK_ColorWHITE);
  label->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
  label->SetShadowColors(SkColorSetARGB(64, 0, 0, 0),
                         SkColorSetARGB(64, 0, 0, 0));
  label->SetShadowOffset(0, 1);
}

}  // namespace internal
}  // namespace ash
