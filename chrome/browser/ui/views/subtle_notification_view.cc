// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/subtle_notification_view.h"

#include <memory>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// Space between the site info label and the link.
const int kMiddlePaddingPx = 30;

const int kOuterPaddingHorizPx = 40;
const int kOuterPaddingVertPx = 8;

// Partially-transparent background color.
const SkColor kBackgroundColor = SkColorSetARGB(0xcc, 0x28, 0x2c, 0x32);

// Spacing around the key name.
const int kKeyNameMarginHorizPx = 7;
const int kKeyNameBorderPx = 1;
const int kKeyNameCornerRadius = 2;
const int kKeyNamePaddingPx = 5;

}  // namespace

// Class containing the instruction text. Contains fancy styling on the keyboard
// key (not just a simple label).
class SubtleNotificationView::InstructionView : public views::View {
 public:
  // Creates an InstructionView with specific text. |text| may contain one or
  // more segments delimited by a pair of pipes ('|'); each of these segments
  // will be displayed as a keyboard key. e.g., "Press |Alt|+|Q| to exit" will
  // have "Alt" and "Q" rendered as keys.
  InstructionView(const base::string16& text,
                  const gfx::FontList& font_list,
                  SkColor foreground_color,
                  SkColor background_color);

  void SetText(const base::string16& text);

 private:
  // Adds a label to the end of the notification text. If |format_as_key|,
  // surrounds the label in a rounded-rect border to indicate that it is a
  // keyboard key.
  void AddTextSegment(const base::string16& text, bool format_as_key);

  const gfx::FontList& font_list_;
  SkColor foreground_color_;
  SkColor background_color_;

  base::string16 text_;

  DISALLOW_COPY_AND_ASSIGN(InstructionView);
};

SubtleNotificationView::InstructionView::InstructionView(
    const base::string16& text,
    const gfx::FontList& font_list,
    SkColor foreground_color,
    SkColor background_color)
    : font_list_(font_list),
      foreground_color_(foreground_color),
      background_color_(background_color) {
  // The |between_child_spacing| is the horizontal margin of the key name.
  views::BoxLayout* layout = new views::BoxLayout(views::BoxLayout::kHorizontal,
                                                  0, 0, kKeyNameMarginHorizPx);
  SetLayoutManager(layout);

  SetText(text);
}

void SubtleNotificationView::InstructionView::SetText(
    const base::string16& text) {
  // Avoid replacing the contents with the same text.
  if (text == text_)
    return;

  RemoveAllChildViews(true);

  // Parse |text|, looking for pipe-delimited segment.
  std::vector<base::string16> segments =
      base::SplitString(text, base::ASCIIToUTF16("|"), base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  // SplitString() returns empty strings for zero-length segments, so given an
  // even number of pipes, there should always be an odd number of segments.
  // The exception is if |text| is entirely empty, in which case the returned
  // list is also empty (rather than containing a single empty string).
  DCHECK(segments.empty() || segments.size() % 2 == 1);

  // Add text segment, alternating between non-key (no border) and key (border)
  // formatting.
  bool format_as_key = false;
  for (const auto& segment : segments) {
    AddTextSegment(segment, format_as_key);
    format_as_key = !format_as_key;
  }

  text_ = text;
}

void SubtleNotificationView::InstructionView::AddTextSegment(
    const base::string16& text, bool format_as_key) {
  views::Label* label = new views::Label(text, font_list_);
  label->SetEnabledColor(foreground_color_);
  label->SetBackgroundColor(background_color_);
  if (!format_as_key) {
    AddChildView(label);
    return;
  }

  views::View* key = new views::View;
  views::BoxLayout* key_name_layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, kKeyNamePaddingPx, 0, 0);
  key_name_layout->set_minimum_cross_axis_size(
      label->GetPreferredSize().height() + kKeyNamePaddingPx * 2);
  key->SetLayoutManager(key_name_layout);
  key->AddChildView(label);
  // The key name has a border around it.
  std::unique_ptr<views::Border> border(views::CreateRoundedRectBorder(
      kKeyNameBorderPx, kKeyNameCornerRadius, foreground_color_));
  key->SetBorder(std::move(border));
  AddChildView(key);
}

SubtleNotificationView::SubtleNotificationView(
    views::LinkListener* link_listener)
    : instruction_view_(nullptr), link_(nullptr) {
  const SkColor kForegroundColor = SK_ColorWHITE;

  std::unique_ptr<views::BubbleBorder> bubble_border(new views::BubbleBorder(
      views::BubbleBorder::NONE, views::BubbleBorder::NO_ASSETS,
      kBackgroundColor));
  set_background(new views::BubbleBackground(bubble_border.get()));
  SetBorder(std::move(bubble_border));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::FontList& font_list =
      rb.GetFontList(ui::ResourceBundle::MediumFont);

  instruction_view_ = new InstructionView(base::string16(), font_list,
                                          kForegroundColor, kBackgroundColor);

  link_ = new views::Link();
  link_->SetFocusBehavior(FocusBehavior::NEVER);
  link_->set_listener(link_listener);
  link_->SetFontList(font_list);
  link_->SetEnabledColor(kForegroundColor);
  link_->SetBackgroundColor(kBackgroundColor);
  link_->SetVisible(false);

  int outer_padding_horiz = kOuterPaddingHorizPx;
  int outer_padding_vert = kOuterPaddingVertPx;
  AddChildView(instruction_view_);
  AddChildView(link_);

  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, outer_padding_horiz,
                           outer_padding_vert, kMiddlePaddingPx);
  SetLayoutManager(layout);
}

SubtleNotificationView::~SubtleNotificationView() {}

void SubtleNotificationView::UpdateContent(
    const base::string16& instruction_text,
    const base::string16& link_text) {
  instruction_view_->SetText(instruction_text);
  instruction_view_->SetVisible(!instruction_text.empty());
  link_->SetText(link_text);
  link_->SetVisible(!link_text.empty());
  Layout();
}

// static
views::Widget* SubtleNotificationView::CreatePopupWidget(
    gfx::NativeView parent_view,
    SubtleNotificationView* view,
    bool accept_events) {
  // Initialize the popup.
  views::Widget* popup = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = parent_view;
  params.accept_events = accept_events;
  popup->Init(params);
  popup->SetContentsView(view);
  // We set layout manager to nullptr to prevent the widget from sizing its
  // contents to the same size as itself. This prevents the widget contents from
  // shrinking while we animate the height of the popup to give the impression
  // that it is sliding off the top of the screen.
  // TODO(mgiuca): This probably isn't necessary now that there is no slide
  // animation. Remove it.
  popup->GetRootView()->SetLayoutManager(nullptr);

  return popup;
}
