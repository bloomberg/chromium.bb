// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"

#include "chrome/browser/ui/views/harmony/layout_delegate.h"
#include "chrome/grit/locale_settings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"

namespace {
const int kIconSize = 16;
}

ToolbarActionsBarBubbleViews::ToolbarActionsBarBubbleViews(
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    bool anchored_to_action,
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> delegate)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::TOP_RIGHT),
      delegate_(std::move(delegate)),
      close_reason_(
          ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_DEACTIVATION),
      item_list_(nullptr),
      link_(nullptr),
      anchored_to_action_(anchored_to_action) {
  set_close_on_deactivate(delegate_->ShouldCloseOnDeactivate());
  if (!anchor_view)
    SetAnchorRect(gfx::Rect(anchor_point, gfx::Size()));
}

ToolbarActionsBarBubbleViews::~ToolbarActionsBarBubbleViews() {}

void ToolbarActionsBarBubbleViews::Show() {
  delegate_->OnBubbleShown();
  GetWidget()->Show();
}

views::View* ToolbarActionsBarBubbleViews::CreateExtraView() {
  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
      extra_view_info = delegate_->GetExtraViewInfo();

  if (!extra_view_info)
    return nullptr;

  std::unique_ptr<views::ImageView> icon;
  if (extra_view_info->resource) {
    icon.reset(new views::ImageView);
    icon->SetImage(gfx::CreateVectorIcon(*extra_view_info->resource, kIconSize,
                                         gfx::kChromeIconGrey));
  }

  std::unique_ptr<views::Label> label;
  const base::string16& text = extra_view_info->text;
  if (!text.empty()) {
    if (extra_view_info->is_text_linked) {
      link_ = new views::Link(text);
      link_->set_listener(this);
      label.reset(link_);
    } else {
      label.reset(new views::Label(text));
    }
  }

  if (icon && label) {
    views::View* parent = new views::View();
    parent->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0,
        LayoutDelegate::Get()->GetMetric(
            LayoutDelegate::Metric::RELATED_CONTROL_VERTICAL_SPACING)));
    parent->AddChildView(icon.release());
    parent->AddChildView(label.release());
    return parent;
  }

  return icon ? static_cast<views::View*>(icon.release())
              : static_cast<views::View*>(label.release());
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
  delegate_->OnBubbleClosed(close_reason_);
  return true;
}

void ToolbarActionsBarBubbleViews::Init() {
  LayoutDelegate* delegate = LayoutDelegate::Get();
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0,
      delegate->GetMetric(
          LayoutDelegate::Metric::RELATED_CONTROL_VERTICAL_SPACING)));

  // Add the content string.
  views::Label* content_label =
      new views::Label(delegate_->GetBodyText(anchored_to_action_));
  content_label->SetMultiLine(true);
  int width = views::Widget::GetLocalizedContentsWidth(
        IDS_EXTENSION_TOOLBAR_REDESIGN_NOTIFICATION_BUBBLE_WIDTH_CHARS);
  content_label->SizeToFit(width);
  content_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(content_label);

  base::string16 item_list = delegate_->GetItemListText();

  if (!item_list.empty()) {
    item_list_ = new views::Label(item_list);
    item_list_->SetBorder(views::CreateEmptyBorder(
        0,
        delegate->GetMetric(
            LayoutDelegate::Metric::RELATED_CONTROL_HORIZONTAL_SPACING),
        0, 0));
    item_list_->SetMultiLine(true);
    item_list_->SizeToFit(width);
    item_list_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(item_list_);
  }
}

int ToolbarActionsBarBubbleViews::GetDialogButtons() const {
  int buttons = ui::DIALOG_BUTTON_NONE;
  if (!delegate_->GetActionButtonText().empty())
    buttons |= ui::DIALOG_BUTTON_OK;
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
  close_reason_ = ToolbarActionsBarBubbleDelegate::CLOSE_LEARN_MORE;
  GetWidget()->Close();
}
