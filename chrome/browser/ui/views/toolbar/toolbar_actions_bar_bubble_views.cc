// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"

#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/grit/locale_settings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {
const int kListPadding = 10;
}

ToolbarActionsBarBubbleViews::ToolbarActionsBarBubbleViews(
    views::View* anchor_view,
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> delegate)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::TOP_RIGHT),
      delegate_(std::move(delegate)),
      item_list_(nullptr),
      learn_more_button_(nullptr) {
  set_close_on_deactivate(delegate_->ShouldCloseOnDeactivate());
}

ToolbarActionsBarBubbleViews::~ToolbarActionsBarBubbleViews() {}

void ToolbarActionsBarBubbleViews::Show() {
  delegate_->OnBubbleShown();
  GetWidget()->Show();
}

views::View* ToolbarActionsBarBubbleViews::CreateExtraView() {
  base::string16 text = delegate_->GetLearnMoreButtonText();
  if (text.empty())
    return nullptr;

  learn_more_button_ = new views::Link(text);
  learn_more_button_->set_listener(this);
  return learn_more_button_;
}

base::string16 ToolbarActionsBarBubbleViews::GetWindowTitle() const {
  return delegate_->GetHeadingText();
}

bool ToolbarActionsBarBubbleViews::Cancel() {
  delegate_->OnBubbleClosed(
      ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION);
  return true;
}

bool ToolbarActionsBarBubbleViews::Accept() {
  delegate_->OnBubbleClosed(ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE);
  return true;
}

bool ToolbarActionsBarBubbleViews::Close() {
  if (delegate_) {
    delegate_->OnBubbleClosed(
        ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_DEACTIVATION);
  }
  return true;
}

void ToolbarActionsBarBubbleViews::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                                        views::kRelatedControlVerticalSpacing));

  // Add the content string.
  views::Label* content_label = new views::Label(
      delegate_->GetBodyText(GetAnchorView()->id() == VIEW_ID_BROWSER_ACTION));
  content_label->SetMultiLine(true);
  int width = views::Widget::GetLocalizedContentsWidth(
        IDS_EXTENSION_TOOLBAR_REDESIGN_NOTIFICATION_BUBBLE_WIDTH_CHARS);
  content_label->SizeToFit(width);
  content_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(content_label);

  base::string16 item_list = delegate_->GetItemListText();
  if (!item_list.empty()) {
    item_list_ = new views::Label(item_list);
    item_list_->SetBorder(
        views::Border::CreateEmptyBorder(0, kListPadding, 0, 0));
    item_list_->SetMultiLine(true);
    item_list_->SizeToFit(width);
    item_list_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(item_list_);
  }
}

int ToolbarActionsBarBubbleViews::GetDialogButtons() const {
  int buttons = ui::DIALOG_BUTTON_OK;
  if (!delegate_->GetDismissButtonText().empty())
    buttons |= ui::DIALOG_BUTTON_CANCEL;
  return buttons;
}

int ToolbarActionsBarBubbleViews::GetDefaultDialogButton() const {
  // TODO(estade): we should set a default where approprite. See
  // http://crbug.com/621122
  return ui::DIALOG_BUTTON_NONE;
}

base::string16 ToolbarActionsBarBubbleViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? delegate_->GetActionButtonText()
                                        : delegate_->GetDismissButtonText();
}

void ToolbarActionsBarBubbleViews::LinkClicked(views::Link* link,
                                               int event_flags) {
  delegate_->OnBubbleClosed(ToolbarActionsBarBubbleDelegate::CLOSE_LEARN_MORE);
  // Reset delegate so we don't send extra OnBubbleClosed()s.
  delegate_.reset();
  GetWidget()->Close();
}
