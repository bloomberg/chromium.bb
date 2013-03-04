// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/hover_highlight_view.h"

#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "ash/system/tray/view_click_listener.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {

const int kPopupDetailLabelExtraLeftMargin = 8;
const int kCheckLabelPadding = 4;

}  // namespace

namespace ash {
namespace internal {

HoverHighlightView::HoverHighlightView(ViewClickListener* listener)
    : listener_(listener),
      text_label_(NULL),
      highlight_color_(kHoverBackgroundColor),
      default_color_(0),
      text_highlight_color_(0),
      text_default_color_(0),
      hover_(false),
      expandable_(false) {
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
  }
  return AddLabel(text, style);
}

void HoverHighlightView::SetExpandable(bool expandable) {
  if (expandable != expandable_) {
    expandable_ = expandable;
    InvalidateLayout();
  }
}

bool HoverHighlightView::PerformAction(const ui::Event& event) {
  if (!listener_)
    return false;
  listener_->OnViewClicked(this);
  return true;
}

gfx::Size HoverHighlightView::GetPreferredSize() {
  gfx::Size size = ActionableView::GetPreferredSize();
  if (!expandable_ || size.height() < kTrayPopupItemHeight)
    size.set_height(kTrayPopupItemHeight);
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

}  // namespace internal
}  // namespace ash
