// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/message_bubble.h"

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/widget/widget.h"

namespace chromeos {

static const int kBorderSize = 4;
static const int kMaxLabelWidth = 250;

MessageBubble::MessageBubble(views::WidgetGtk::Type type, views::Widget* parent,
    SkBitmap* image, const std::wstring& text, bool grab_enabled)
    : InfoBubble(type),
      parent_(parent),
      grab_enabled_(grab_enabled) {
  using views::GridLayout;

  views::View* control_view = new views::View();
  GridLayout* layout = new GridLayout(control_view);
  control_view->SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kBorderSize);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kBorderSize);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  layout->StartRow(0, 0);
  icon_ = new views::ImageView();
  icon_->SetImage(*image);
  layout->AddView(icon_);

  text_ = new views::Label(text);
  text_->SetMultiLine(true);
  text_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  text_->SizeToFit(kMaxLabelWidth);
  layout->AddView(text_);

  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
      rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::CustomButton::BS_HOT,
      rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
      rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  layout->AddView(close_button_);
}

void MessageBubble::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
  if (sender == close_button_) {
    Close();
  } else {
    NOTREACHED() << "Unknown view";
  }
}

// static
MessageBubble* MessageBubble::Show(views::Widget* parent,
                                   const gfx::Rect& position_relative_to,
                                   BubbleBorder::ArrowLocation arrow_location,
                                   SkBitmap* image,
                                   const std::wstring& text,
                                   InfoBubbleDelegate* delegate) {
  // The bubble will be destroyed when it is closed.
  MessageBubble* bubble = new MessageBubble(
      views::WidgetGtk::TYPE_WINDOW, parent, image, text, true);
  bubble->Init(parent, position_relative_to, arrow_location,
               bubble->text_->GetParent(), delegate);
  return bubble;
}

// static
MessageBubble* MessageBubble::ShowNoGrab(
    views::Widget* parent,
    const gfx::Rect& position_relative_to,
    BubbleBorder::ArrowLocation arrow_location,
    SkBitmap* image,
    const std::wstring& text,
    InfoBubbleDelegate* delegate) {
  // The bubble will be destroyed when it is closed.
  MessageBubble* bubble = new MessageBubble(
      views::WidgetGtk::TYPE_CHILD, parent, image, text, false);
  bubble->Init(parent, position_relative_to, arrow_location,
               bubble->text_->GetParent(), delegate);
  return bubble;
}

void MessageBubble::IsActiveChanged() {
  // Active parent instead.
  if (parent_ && IsActive()) {
    gtk_window_present_with_time(
        GTK_WINDOW(static_cast<WidgetGtk*>(parent_)->GetNativeView()),
                   gtk_get_current_event_time());
  }
}

void MessageBubble::DoGrab() {
  if (grab_enabled_)
    WidgetGtk::DoGrab();
}

void MessageBubble::Close() {
  parent_ = NULL;
  InfoBubble::Close();
}

}  // namespace chromeos
