// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/hover_highlight_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/tray/tri_view.h"
#include "ash/common/system/tray/view_click_listener.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/resources/grit/views_resources.h"

namespace {

const int kCheckLabelPadding = 4;

const gfx::FontList& GetFontList(bool highlight) {
  return ui::ResourceBundle::GetSharedInstance().GetFontList(
      highlight ? ui::ResourceBundle::BoldFont : ui::ResourceBundle::BaseFont);
}

}  // namespace

namespace ash {

HoverHighlightView::HoverHighlightView(ViewClickListener* listener)
    : ActionableView(nullptr, TrayPopupInkDropStyle::FILL_BOUNDS),
      listener_(listener),
      highlight_color_(kHoverBackgroundColor) {
  set_notify_enter_exit_on_child(true);
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    SetInkDropMode(InkDropHostView::InkDropMode::ON);
}

HoverHighlightView::~HoverHighlightView() {}

bool HoverHighlightView::GetTooltipText(const gfx::Point& p,
                                        base::string16* tooltip) const {
  if (tooltip_.empty())
    return false;
  *tooltip = tooltip_;
  return true;
}

void HoverHighlightView::AddRightIcon(const gfx::ImageSkia& image,
                                      int icon_size) {
  DCHECK(!right_view_);

  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    views::ImageView* right_icon = TrayPopupUtils::CreateMainImageView();
    right_icon->SetImage(image);
    AddRightView(right_icon);
    return;
  }

  views::ImageView* right_icon = new FixedSizedImageView(icon_size, icon_size);
  right_icon->SetImage(image);
  AddRightView(right_icon);
}

void HoverHighlightView::AddRightView(views::View* view) {
  DCHECK(!right_view_);

  right_view_ = view;
  right_view_->SetEnabled(enabled());
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    DCHECK(tri_view_);
    tri_view_->AddView(TriView::Container::END, right_view_);
    tri_view_->SetContainerVisible(TriView::Container::END, true);
    return;
  }
  DCHECK(box_layout_);
  AddChildView(right_view_);
}

// TODO(tdanderson): Ensure all checkable detailed view rows use this
// mechanism, and share the code that sets the accessible state for
// a checkbox. See crbug.com/652674.
void HoverHighlightView::SetRightViewVisible(bool visible) {
  if (!right_view_)
    return;

  right_view_->SetVisible(visible);
  Layout();
}

void HoverHighlightView::AddIconAndLabel(const gfx::ImageSkia& image,
                                         const base::string16& text,
                                         bool highlight) {
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    DoAddIconAndLabelMd(image, text,
                        TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
    return;
  }

  box_layout_ = new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 3,
                                     kTrayPopupPaddingBetweenItems);
  SetLayoutManager(box_layout_);
  DoAddIconAndLabel(image, kTrayPopupDetailsIconWidth, text, highlight);
}

void HoverHighlightView::AddIconAndLabels(const gfx::ImageSkia& image,
                                          const base::string16& text,
                                          const base::string16& sub_text) {
  DCHECK(MaterialDesignController::IsSystemTrayMenuMaterial());
  DoAddIconAndLabelsMd(image, text,
                       TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL,
                       sub_text);
}

void HoverHighlightView::AddIconAndLabelCustomSize(const gfx::ImageSkia& image,
                                                   const base::string16& text,
                                                   bool highlight,
                                                   int icon_size,
                                                   int indent,
                                                   int space_between_items) {
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    DoAddIconAndLabelMd(image, text,
                        TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
    return;
  }

  box_layout_ = new views::BoxLayout(views::BoxLayout::kHorizontal, indent, 0,
                                     space_between_items);
  SetLayoutManager(box_layout_);
  DoAddIconAndLabel(image, icon_size, text, highlight);
}

void HoverHighlightView::AddIconAndLabelForDefaultView(
    const gfx::ImageSkia& image,
    const base::string16& text,
    bool highlight) {
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    DoAddIconAndLabelMd(image, text,
                        TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
    return;
  }

  // For non-MD, call AddIconAndLabel() so that |box_layout_| is instantiated
  // and installed as the layout manager.
  AddIconAndLabel(image, text, highlight);
}

void HoverHighlightView::DoAddIconAndLabel(const gfx::ImageSkia& image,
                                           int icon_size,
                                           const base::string16& text,
                                           bool highlight) {
  DCHECK(!MaterialDesignController::IsSystemTrayMenuMaterial());
  DCHECK(box_layout_);

  views::ImageView* image_view = new FixedSizedImageView(icon_size, 0);
  image_view->SetImage(image);
  image_view->SetEnabled(enabled());
  AddChildView(image_view);

  text_label_ = new views::Label(text);
  text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label_->SetFontList(GetFontList(highlight));
  if (text_default_color_)
    text_label_->SetEnabledColor(text_default_color_);
  text_label_->SetEnabled(enabled());
  AddChildView(text_label_);
  box_layout_->SetFlexForView(text_label_, 1);

  SetAccessibleName(text);
}

void HoverHighlightView::DoAddIconAndLabelMd(
    const gfx::ImageSkia& image,
    const base::string16& text,
    TrayPopupItemStyle::FontStyle font_style) {
  DoAddIconAndLabelsMd(image, text, font_style, base::string16());
}

void HoverHighlightView::DoAddIconAndLabelsMd(
    const gfx::ImageSkia& image,
    const base::string16& text,
    TrayPopupItemStyle::FontStyle font_style,
    const base::string16& sub_text) {
  DCHECK(MaterialDesignController::IsSystemTrayMenuMaterial());

  SetLayoutManager(new views::FillLayout);
  tri_view_ = TrayPopupUtils::CreateDefaultRowView();
  AddChildView(tri_view_);

  left_icon_ = TrayPopupUtils::CreateMainImageView();
  left_icon_->SetImage(image);
  left_icon_->SetEnabled(enabled());
  tri_view_->AddView(TriView::Container::START, left_icon_);

  text_label_ = TrayPopupUtils::CreateDefaultLabel();
  text_label_->SetText(text);
  text_label_->SetEnabled(enabled());
  TrayPopupItemStyle style(font_style);
  style.SetupLabel(text_label_);

  tri_view_->AddView(TriView::Container::CENTER, text_label_);
  if (!sub_text.empty()) {
    sub_text_label_ = TrayPopupUtils::CreateDefaultLabel();
    sub_text_label_->SetText(sub_text);
    TrayPopupItemStyle sub_style(TrayPopupItemStyle::FontStyle::CAPTION);
    sub_style.set_color_style(TrayPopupItemStyle::ColorStyle::INACTIVE);
    sub_style.SetupLabel(sub_text_label_);
    tri_view_->AddView(TriView::Container::CENTER, sub_text_label_);
  }

  tri_view_->SetContainerVisible(TriView::Container::END, false);

  SetAccessibleName(text);
}

views::Label* HoverHighlightView::AddLabel(const base::string16& text,
                                           gfx::HorizontalAlignment alignment,
                                           bool highlight) {
  box_layout_ = new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
  SetLayoutManager(box_layout_);
  text_label_ = new views::Label(text);
  int left_margin = kTrayPopupPaddingHorizontal;
  int right_margin = kTrayPopupPaddingHorizontal;
  if (alignment != gfx::ALIGN_CENTER) {
    if (base::i18n::IsRTL())
      right_margin += kTrayPopupDetailsLabelExtraLeftMargin;
    else
      left_margin += kTrayPopupDetailsLabelExtraLeftMargin;
  }
  text_label_->SetBorder(
      views::CreateEmptyBorder(5, left_margin, 5, right_margin));
  text_label_->SetHorizontalAlignment(alignment);
  text_label_->SetFontList(GetFontList(highlight));
  // Do not set alpha value in disable color. It will have issue with elide
  // blending filter in disabled state for rendering label text color.
  text_label_->SetDisabledColor(SkColorSetARGB(255, 127, 127, 127));
  if (text_default_color_)
    text_label_->SetEnabledColor(text_default_color_);
  text_label_->SetEnabled(enabled());
  AddChildView(text_label_);
  box_layout_->SetFlexForView(text_label_, 1);

  SetAccessibleName(text);
  return text_label_;
}

views::Label* HoverHighlightView::AddCheckableLabel(const base::string16& text,
                                                    bool highlight,
                                                    bool checked) {
  DCHECK(!MaterialDesignController::IsSystemTrayMenuMaterial());

  if (checked) {
    accessibility_state_ = AccessibilityState::CHECKED_CHECKBOX;
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::ImageSkia* check =
        rb.GetImageNamed(IDR_MENU_CHECK).ToImageSkia();
    int margin = kTrayPopupPaddingHorizontal +
                 kTrayPopupDetailsLabelExtraLeftMargin - kCheckLabelPadding;
    box_layout_ = new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 3,
                                       kCheckLabelPadding);
    SetLayoutManager(box_layout_);
    views::ImageView* image_view = new FixedSizedImageView(margin, 0);
    image_view->SetImage(check);
    image_view->SetHorizontalAlignment(views::ImageView::TRAILING);
    image_view->SetEnabled(enabled());
    AddChildView(image_view);

    text_label_ = new views::Label(text);
    text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    text_label_->SetFontList(GetFontList(highlight));
    text_label_->SetDisabledColor(SkColorSetARGB(127, 0, 0, 0));
    if (text_default_color_)
      text_label_->SetEnabledColor(text_default_color_);
    text_label_->SetEnabled(enabled());
    AddChildView(text_label_);

    SetAccessibleName(text);
    return text_label_;
  }

  accessibility_state_ = AccessibilityState::UNCHECKED_CHECKBOX;
  return AddLabel(text, gfx::ALIGN_LEFT, highlight);
}

void HoverHighlightView::AddLabelRowMd(const base::string16& text) {
  DCHECK(MaterialDesignController::IsSystemTrayMenuMaterial());

  SetLayoutManager(new views::FillLayout);
  tri_view_ = TrayPopupUtils::CreateDefaultRowView();
  AddChildView(tri_view_);

  text_label_ = TrayPopupUtils::CreateDefaultLabel();
  text_label_->SetText(text);

  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
  style.SetupLabel(text_label_);
  tri_view_->AddView(TriView::Container::CENTER, text_label_);

  SetAccessibleName(text);
}

void HoverHighlightView::SetExpandable(bool expandable) {
  if (expandable != expandable_) {
    expandable_ = expandable;
    InvalidateLayout();
  }
}

void HoverHighlightView::SetHighlight(bool highlight) {
  // Do not change the font styling for a highlighted row in material design.
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    return;

  DCHECK(text_label_);
  text_label_->SetFontList(GetFontList(highlight));
  text_label_->InvalidateLayout();
}

void HoverHighlightView::SetAccessiblityState(
    AccessibilityState accessibility_state) {
  accessibility_state_ = accessibility_state;
  if (accessibility_state_ != AccessibilityState::DEFAULT)
    NotifyAccessibilityEvent(ui::AX_EVENT_CHECKED_STATE_CHANGED, true);
}

void HoverHighlightView::SetHoverHighlight(bool hover) {
  // We do not show any hover effects for material design.
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    return;

  if (!enabled() && hover)
    return;
  if (hover_ == hover)
    return;
  hover_ = hover;
  if (!text_label_)
    return;
  if (hover_ && text_highlight_color_)
    text_label_->SetEnabledColor(text_highlight_color_);
  if (!hover_ && text_default_color_)
    text_label_->SetEnabledColor(text_default_color_);
  SchedulePaint();
}

bool HoverHighlightView::PerformAction(const ui::Event& event) {
  if (!listener_)
    return false;
  listener_->OnViewClicked(this);
  return true;
}

void HoverHighlightView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ActionableView::GetAccessibleNodeData(node_data);

  if (accessibility_state_ == AccessibilityState::CHECKED_CHECKBOX ||
      accessibility_state_ == AccessibilityState::UNCHECKED_CHECKBOX) {
    node_data->role = ui::AX_ROLE_CHECK_BOX;
  }

  if (accessibility_state_ == AccessibilityState::CHECKED_CHECKBOX)
    node_data->AddStateFlag(ui::AX_STATE_CHECKED);
}

gfx::Size HoverHighlightView::GetPreferredSize() const {
  gfx::Size size = ActionableView::GetPreferredSize();

  if (custom_height_)
    size.set_height(custom_height_);
  else if (!expandable_ || size.height() < kTrayPopupItemMinHeight)
    size.set_height(kTrayPopupItemMinHeight);

  return size;
}

int HoverHighlightView::GetHeightForWidth(int width) const {
  return GetPreferredSize().height();
}

void HoverHighlightView::OnMouseEntered(const ui::MouseEvent& event) {
  SetHoverHighlight(true);
}

void HoverHighlightView::OnMouseExited(const ui::MouseEvent& event) {
  SetHoverHighlight(false);
}

void HoverHighlightView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    SetHoverHighlight(true);
  } else if (event->type() == ui::ET_GESTURE_TAP_CANCEL ||
             event->type() == ui::ET_GESTURE_TAP) {
    SetHoverHighlight(false);
  }
  ActionableView::OnGestureEvent(event);
}

void HoverHighlightView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SetHoverHighlight(IsMouseHovered());
}

void HoverHighlightView::OnEnabledChanged() {
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    if (left_icon_)
      left_icon_->SetEnabled(enabled());
    if (text_label_)
      text_label_->SetEnabled(enabled());
    if (right_view_)
      right_view_->SetEnabled(enabled());
  } else {
    if (!enabled())
      SetHoverHighlight(false);
    for (int i = 0; i < child_count(); ++i)
      child_at(i)->SetEnabled(enabled());
  }
}

void HoverHighlightView::OnPaintBackground(gfx::Canvas* canvas) {
  canvas->DrawColor(hover_ ? highlight_color_ : default_color_);
}

void HoverHighlightView::OnFocus() {
  ScrollRectToVisible(gfx::Rect(gfx::Point(), size()));
  ActionableView::OnFocus();
}

}  // namespace ash
