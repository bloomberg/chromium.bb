// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/message_bubble.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/layout/grid_layout.h"
#include "views/widget/widget.h"

namespace chromeos {

static const int kBorderSize = 4;
static const int kMaxLabelWidth = 250;

MessageBubble::MessageBubble(views::WidgetGtk::Type type,
                             views::Widget* parent,
                             SkBitmap* image,
                             const std::wstring& text,
                             const std::wstring& help,
                             bool grab_enabled,
                             MessageBubbleDelegate* delegate)
    : InfoBubble(type, false),  // don't show while screen is locked
      parent_(parent),
      help_link_(NULL),
      message_delegate_(delegate),
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
  if (!help.empty()) {
    column_set = layout->AddColumnSet(1);
    column_set->AddPaddingColumn(0, kBorderSize + image->width());
    column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 1,
                          GridLayout::USE_PREF, 0, 0);
  }

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

  if (!help.empty()) {
    layout->StartRowWithPadding(0, 1, 0, kBorderSize);
    help_link_ = new views::Link(help);
    help_link_->SetController(this);
    help_link_->SetNormalColor(login::kLinkColor);
    help_link_->SetHighlightedColor(login::kLinkColor);
    layout->AddView(help_link_);
  }
}

void MessageBubble::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
  if (sender == close_button_) {
    Close();
  } else {
    NOTREACHED() << "Unknown view";
  }
}

void MessageBubble::LinkActivated(views::Link* source, int event_flags) {
  if (source == help_link_) {
    if (message_delegate_)
      message_delegate_->OnHelpLinkActivated();
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
                                   const std::wstring& help,
                                   MessageBubbleDelegate* delegate) {
  // The bubble will be destroyed when it is closed.
  MessageBubble* bubble = new MessageBubble(
      views::WidgetGtk::TYPE_WINDOW, parent, image, text, help, true, delegate);
  bubble->InitBubble(parent, position_relative_to, arrow_location,
                     bubble->text_->parent(), delegate);
  return bubble;
}

// static
MessageBubble* MessageBubble::ShowNoGrab(
    views::Widget* parent,
    const gfx::Rect& position_relative_to,
    BubbleBorder::ArrowLocation arrow_location,
    SkBitmap* image,
    const std::wstring& text,
    const std::wstring& help,
    MessageBubbleDelegate* delegate) {
  // The bubble will be destroyed when it is closed.
  MessageBubble* bubble = new MessageBubble(
      views::WidgetGtk::TYPE_CHILD, parent, image, text, help, false, delegate);
  bubble->InitBubble(parent, position_relative_to, arrow_location,
                     bubble->text_->parent(), delegate);
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

void MessageBubble::SetNativeCapture() {
  if (grab_enabled_)
    WidgetGtk::SetNativeCapture();
}

void MessageBubble::Close() {
  parent_ = NULL;
  InfoBubble::Close();
}

}  // namespace chromeos
