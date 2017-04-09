// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/hover_highlight_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/tray/view_click_listener.h"
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

const gfx::FontList& GetFontList(bool highlight) {
  return ui::ResourceBundle::GetSharedInstance().GetFontList(
      highlight ? ui::ResourceBundle::BoldFont : ui::ResourceBundle::BaseFont);
}

}  // namespace

namespace ash {

HoverHighlightView::HoverHighlightView(ViewClickListener* listener)
    : ActionableView(nullptr, TrayPopupInkDropStyle::FILL_BOUNDS),
      listener_(listener) {
  set_notify_enter_exit_on_child(true);
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

  views::ImageView* right_icon = TrayPopupUtils::CreateMainImageView();
  right_icon->SetImage(image);
  AddRightView(right_icon);
}

void HoverHighlightView::AddRightView(views::View* view) {
  DCHECK(!right_view_);

  right_view_ = view;
  right_view_->SetEnabled(enabled());
  tri_view_->AddView(TriView::Container::END, right_view_);
  tri_view_->SetContainerVisible(TriView::Container::END, true);
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
                                         const base::string16& text) {
  DoAddIconAndLabel(image, text,
                    TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
}

void HoverHighlightView::AddIconAndLabels(const gfx::ImageSkia& image,
                                          const base::string16& text,
                                          const base::string16& sub_text) {
  DoAddIconAndLabels(image, text,
                     TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL,
                     sub_text);
}

void HoverHighlightView::AddIconAndLabelForDefaultView(
    const gfx::ImageSkia& image,
    const base::string16& text) {
  DoAddIconAndLabel(image, text,
                    TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
}

void HoverHighlightView::DoAddIconAndLabel(
    const gfx::ImageSkia& image,
    const base::string16& text,
    TrayPopupItemStyle::FontStyle font_style) {
  DoAddIconAndLabels(image, text, font_style, base::string16());
}

void HoverHighlightView::DoAddIconAndLabels(
    const gfx::ImageSkia& image,
    const base::string16& text,
    TrayPopupItemStyle::FontStyle font_style,
    const base::string16& sub_text) {
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

views::Label* HoverHighlightView::AddLabelDeprecated(
    const base::string16& text,
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

void HoverHighlightView::AddLabelRow(const base::string16& text) {
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

void HoverHighlightView::SetAccessiblityState(
    AccessibilityState accessibility_state) {
  accessibility_state_ = accessibility_state;
  if (accessibility_state_ != AccessibilityState::DEFAULT)
    NotifyAccessibilityEvent(ui::AX_EVENT_CHECKED_STATE_CHANGED, true);
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

void HoverHighlightView::OnEnabledChanged() {
  if (left_icon_)
    left_icon_->SetEnabled(enabled());
  if (text_label_)
    text_label_->SetEnabled(enabled());
  if (right_view_)
    right_view_->SetEnabled(enabled());
}

void HoverHighlightView::OnFocus() {
  ScrollRectToVisible(gfx::Rect(gfx::Point(), size()));
  ActionableView::OnFocus();
}

}  // namespace ash
