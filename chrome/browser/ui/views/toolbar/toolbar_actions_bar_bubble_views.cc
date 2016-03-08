// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"

#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "chrome/grit/locale_settings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

ToolbarActionsBarBubbleViews::ToolbarActionsBarBubbleViews(
    views::View* anchor_view,
    scoped_ptr<ToolbarActionsBarBubbleDelegate> delegate)
    : views::BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      delegate_(std::move(delegate)),
      heading_label_(nullptr),
      content_label_(nullptr),
      dismiss_button_(nullptr),
      action_button_(nullptr),
      acknowledged_(false) {}

ToolbarActionsBarBubbleViews::~ToolbarActionsBarBubbleViews() {}

void ToolbarActionsBarBubbleViews::Show() {
  delegate_->OnBubbleShown();
  GetWidget()->Show();
}

void ToolbarActionsBarBubbleViews::Init() {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(views::kPanelVertMargin, views::kPanelHorizMargin,
                    views::kPanelVertMargin, views::kPanelHorizMargin);
  SetLayoutManager(layout);

  enum ColumnSetId {
    HEADER_AND_BODY_COLUMN_SET = 0,
    BUTTON_STRIP_COLUMN_SET,
  };

  views::ColumnSet* header_and_body_cs =
      layout->AddColumnSet(HEADER_AND_BODY_COLUMN_SET);
  header_and_body_cs->AddColumn(views::GridLayout::TRAILING,
                                views::GridLayout::LEADING, 1,
                                views::GridLayout::USE_PREF, 0, 0);
  views::ColumnSet* buttons_cs = layout->AddColumnSet(BUTTON_STRIP_COLUMN_SET);
  buttons_cs->AddPaddingColumn(1, 0);
  buttons_cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING,
                        0, views::GridLayout::USE_PREF, 0, 0);
  buttons_cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  buttons_cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING,
                        0, views::GridLayout::USE_PREF, 0, 0);

  // Add a header.
  layout->StartRow(0, HEADER_AND_BODY_COLUMN_SET);
  heading_label_ = new views::Label(delegate_->GetHeadingText());
  heading_label_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::MediumFont));
  heading_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(heading_label_);
  layout->AddPaddingRow(0, views::kLabelToControlVerticalSpacing);
  int width = views::Widget::GetLocalizedContentsWidth(
      IDS_EXTENSION_TOOLBAR_REDESIGN_NOTIFICATION_BUBBLE_WIDTH_CHARS);

  // Add the content string.
  layout->StartRow(0, HEADER_AND_BODY_COLUMN_SET);
  content_label_ = new views::Label(delegate_->GetBodyText());
  content_label_->SetMultiLine(true);
  content_label_->SizeToFit(width);
  content_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(content_label_);
  layout->AddPaddingRow(0, views::kLabelToControlVerticalSpacing);

  layout->StartRow(0, BUTTON_STRIP_COLUMN_SET);
  base::string16 dismiss_button_text = delegate_->GetDismissButtonText();
  if (!dismiss_button_text.empty()) {
    dismiss_button_ = new views::LabelButton(this, dismiss_button_text);
    dismiss_button_->SetStyle(views::Button::STYLE_BUTTON);
    layout->AddView(dismiss_button_, 1, 1, views::GridLayout::TRAILING,
                    views::GridLayout::FILL);
  } else {
    layout->SkipColumns(1);
  }

  action_button_ =
      new views::LabelButton(this, delegate_->GetActionButtonText());
  action_button_->SetStyle(views::Button::STYLE_BUTTON);
  layout->AddView(action_button_, 1, 1, views::GridLayout::TRAILING,
                  views::GridLayout::FILL);
}

void ToolbarActionsBarBubbleViews::OnWidgetDestroying(views::Widget* widget) {
  BubbleDelegateView::OnWidgetDestroying(widget);
  if (!acknowledged_) {
    ToolbarActionsBarBubbleDelegate::CloseAction close_action =
        close_reason() == CloseReason::DEACTIVATION
            ? ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_DEACTIVATION
            : ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION;
    delegate_->OnBubbleClosed(close_action);
    acknowledged_ = true;
  }
}

void ToolbarActionsBarBubbleViews::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  delegate_->OnBubbleClosed(ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE);
  acknowledged_ = true;
  GetWidget()->Close();
}
