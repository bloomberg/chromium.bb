// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_views.h"

#include "ash/system/tray/tray_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace internal {

namespace {
const int kIconPaddingLeft = 5;
const int kPaddingAroundButtons = 5;
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

ActionableView::ActionableView() {
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
  return PerformAction(event);
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

void ActionableView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = accessible_name_;
}

////////////////////////////////////////////////////////////////////////////////
// HoverHighlightView

HoverHighlightView::HoverHighlightView(ViewClickListener* listener)
    : listener_(listener),
      highlight_color_(kHoverBackgroundColor),
      default_color_(0),
      fixed_height_(0),
      hover_(false) {
  set_notify_enter_exit_on_child(true);
}

HoverHighlightView::~HoverHighlightView() {
}

void HoverHighlightView::AddIconAndLabel(const SkBitmap& image,
                                         const string16& text,
                                         gfx::Font::FontStyle style) {
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 3, kIconPaddingLeft));
  views::ImageView* image_view =
      new FixedSizedImageView(kTrayPopupDetailsIconWidth, 0);
  image_view->SetImage(image);
  AddChildView(image_view);

  views::Label* label = new views::Label(text);
  label->SetFont(label->font().DeriveFont(0, style));
  AddChildView(label);

  SetAccessibleName(text);
}

void HoverHighlightView::AddLabel(const string16& text,
                                  gfx::Font::FontStyle style) {
  SetLayoutManager(new views::FillLayout());
  views::Label* label = new views::Label(text);
  label->set_border(views::Border::CreateEmptyBorder(
      5, kTrayPopupDetailsIconWidth + kIconPaddingLeft, 5, 0));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label->SetFont(label->font().DeriveFont(0, style));
  label->SetDisabledColor(SkColorSetARGB(127, 0, 0, 0));
  AddChildView(label);

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
  SchedulePaint();
}

void HoverHighlightView::OnMouseExited(const views::MouseEvent& event) {
  hover_ = false;
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

gfx::Size FixedSizedScrollView::GetPreferredSize() {
  return fixed_size_;
}

void FixedSizedScrollView::Layout() {
  views::View* contents = GetContents();
  gfx::Rect bounds = gfx::Rect(contents->GetPreferredSize());
  bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
  contents->SetBoundsRect(bounds);

  views::ScrollView::Layout();
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
  size.Enlarge(0, 16);
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
  views::BoxLayout *layout = new
    views::BoxLayout(views::BoxLayout::kHorizontal,
        kPaddingAroundButtons,
        kPaddingAroundButtons,
        -1);
  layout->set_spread_blank_space(true);
  SetLayoutManager(layout);
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
// TrayPopupTextButtonContainer

TrayPopupHeaderButton::TrayPopupHeaderButton(views::ButtonListener* listener,
                                             int enabled_resource_id,
                                             int disabled_resource_id)
    : views::ToggleImageButton(listener) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  SetImage(views::CustomButton::BS_NORMAL,
      bundle.GetImageNamed(enabled_resource_id).ToSkBitmap());
  SetToggledImage(views::CustomButton::BS_NORMAL,
      bundle.GetImageNamed(disabled_resource_id).ToSkBitmap());
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  set_background(views::Background::CreateSolidBackground(
      ash::kHeaderBackgroundColor));
  set_focusable(true);
}

TrayPopupHeaderButton::~TrayPopupHeaderButton() {}

gfx::Size TrayPopupHeaderButton::GetPreferredSize() {
  return gfx::Size(ash::kTrayPopupItemHeight, ash::kTrayPopupItemHeight);
}

void TrayPopupHeaderButton::OnPaintBorder(gfx::Canvas* canvas) {
  // Left border.
  canvas->FillRect(gfx::Rect(0, 0, 1, height()), ash::kBorderDarkColor);
}

void TrayPopupHeaderButton::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
    canvas->DrawRect(gfx::Rect(2, 1, width() - 4, height() - 3),
                     kFocusBorderColor);
  }
}

void TrayPopupHeaderButton::StateChanged() {
  set_background(views::Background::CreateSolidBackground(
      IsHotTracked() ? ash::kHeaderHoverBackgroundColor :
                       ash::kHeaderBackgroundColor));
}

views::View* CreateDetailedHeaderEntry(int string_id,
                                       ViewClickListener* listener) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  HoverHighlightView* container = new HoverHighlightView(listener);
  container->set_fixed_height(kTrayPopupItemHeight);
  container->SetLayoutManager(new
      views::BoxLayout(views::BoxLayout::kHorizontal, 0, 3, kIconPaddingLeft));
  views::ImageView* back =
      new FixedSizedImageView(kTrayPopupDetailsIconWidth, 0);
  back->EnableCanvasFlippingForRTLUI(true);
  back->SetImage(rb.GetImageNamed(IDR_AURA_UBER_TRAY_LESS).ToSkBitmap());
  container->AddChildView(back);
  views::Label* header = new views::Label(rb.GetLocalizedString(string_id));
  header->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  header->SetFont(header->font().DeriveFont(4));
  container->AddChildView(header);
  container->SetAccessibleName(
      rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_PREVIOUS_MENU));
  container->set_highlight_color(kHeaderHoverBackgroundColor);
  container->set_default_color(kHeaderBackgroundColor);
  return container;
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
