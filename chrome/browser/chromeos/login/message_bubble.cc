// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/message_bubble.h"

#include <vector>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

static const int kBorderSize = 4;
static const int kMaxLabelWidth = 250;

MessageBubble::MessageBubble(views::Widget::InitParams::Type type,
                             views::Widget* parent,
                             SkBitmap* image,
                             const std::wstring& text,
                             const std::vector<std::wstring>& links,
                             bool grab_enabled,
                             MessageBubbleDelegate* delegate)
    : Bubble(type, false),  // don't show while screen is locked
      parent_(parent),
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
  if (!links.empty()) {
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

  text_ = new views::Label(WideToUTF16Hack(text));
  text_->SetMultiLine(true);
  text_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  text_->SetBackgroundColor(Bubble::kBackgroundColor);
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

  for (size_t i = 0; i < links.size(); ++i) {
    layout->StartRowWithPadding(0, 1, 0, kBorderSize);
    views::Link* help_link_ = new views::Link(WideToUTF16Hack(links[i]));
    help_links_.push_back(help_link_);
    help_link_->set_listener(this);
    help_link_->SetBackgroundColor(Bubble::kBackgroundColor);
    help_link_->SetEnabledColor(login::kLinkColor);
    help_link_->SetPressedColor(login::kLinkColor);
    layout->AddView(help_link_);
  }
}

MessageBubble::~MessageBubble() {
}

void MessageBubble::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
  if (sender == close_button_) {
    Close();
  } else {
    NOTREACHED() << "Unknown view";
  }
}

void MessageBubble::LinkClicked(views::Link* source, int event_flags) {
  if (!message_delegate_)
    return;
  for (size_t i = 0; i < help_links_.size(); ++i) {
    if (source == help_links_[i]) {
      message_delegate_->OnLinkActivated(i);
      return;
    }
  }
  NOTREACHED() << "Unknown view";
}

// static
MessageBubble* MessageBubble::Show(
    views::Widget* parent,
    const gfx::Rect& position_relative_to,
    views::BubbleBorder::ArrowLocation arrow_location,
    SkBitmap* image,
    const std::wstring& text,
    const std::wstring& help,
    MessageBubbleDelegate* delegate) {
  std::vector<std::wstring> links;
  if (!help.empty())
    links.push_back(help);
  return MessageBubble::ShowWithLinks(parent,
                                      position_relative_to,
                                      arrow_location,
                                      image,
                                      text,
                                      links,
                                      delegate);
}

// static
MessageBubble* MessageBubble::ShowWithLinks(
    views::Widget* parent,
    const gfx::Rect& position_relative_to,
    views::BubbleBorder::ArrowLocation arrow_location,
    SkBitmap* image,
    const std::wstring& text,
    const std::vector<std::wstring>& links,
    MessageBubbleDelegate* delegate) {
  // The bubble will be destroyed when it is closed.
  MessageBubble* bubble = new MessageBubble(
      views::Widget::InitParams::TYPE_POPUP, parent, image, text, links,
      true, delegate);
  bubble->InitBubble(parent, position_relative_to, arrow_location,
                     views::BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR,
                     bubble->text_->parent(), delegate);
  return bubble;
}

// static
MessageBubble* MessageBubble::ShowNoGrab(
    views::Widget* parent,
    const gfx::Rect& position_relative_to,
    views::BubbleBorder::ArrowLocation arrow_location,
    SkBitmap* image,
    const std::wstring& text,
    const std::wstring& help,
    MessageBubbleDelegate* delegate) {
  std::vector<std::wstring> links;
  if (!help.empty())
    links.push_back(help);
  // The bubble will be destroyed when it is closed.
  MessageBubble* bubble = new MessageBubble(
      views::Widget::InitParams::TYPE_CONTROL, parent, image, text, links,
      false, delegate);
  bubble->InitBubble(parent, position_relative_to, arrow_location,
                     views::BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR,
                     bubble->text_->parent(), delegate);
  return bubble;
}

#if defined(TOOLKIT_USES_GTK)
// TODO(saintlou): Unclear if we need this for the !gtk case.
void MessageBubble::OnActiveChanged() {
  if (parent_ && IsActive()) {
    // Show the parent.
    gtk_window_present_with_time(parent_->GetNativeWindow(),
                                 gtk_get_current_event_time());
  }
}

void MessageBubble::SetMouseCapture() {
  if (grab_enabled_)
    NativeWidgetGtk::SetMouseCapture();
}
#endif

void MessageBubble::Close() {
  parent_ = NULL;
  Bubble::Close();
}

#if defined(TOOLKIT_USES_GTK)
gboolean MessageBubble::OnButtonPress(GtkWidget* widget,
                                      GdkEventButton* event) {
  NativeWidgetGtk::OnButtonPress(widget, event);
  // Never propagate event to parent.
  return true;
}
#endif

}  // namespace chromeos
