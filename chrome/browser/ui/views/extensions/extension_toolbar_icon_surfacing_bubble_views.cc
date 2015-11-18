// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_toolbar_icon_surfacing_bubble_views.h"

#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "chrome/grit/locale_settings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

ExtensionToolbarIconSurfacingBubble::ExtensionToolbarIconSurfacingBubble(
    views::View* anchor_view,
    scoped_ptr<ToolbarActionsBarBubbleDelegate> delegate)
    : views::BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      delegate_(delegate.Pass()),
      acknowledged_(false) {
}

ExtensionToolbarIconSurfacingBubble::~ExtensionToolbarIconSurfacingBubble() {
}

void ExtensionToolbarIconSurfacingBubble::Show() {
  delegate_->OnBubbleShown();
  GetWidget()->Show();
}

void ExtensionToolbarIconSurfacingBubble::Init() {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(views::kPanelVertMargin, views::kPanelHorizMargin,
                    views::kPanelVertMargin, views::kPanelHorizMargin);
  SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING,
                     views::GridLayout::LEADING,
                     1,
                     views::GridLayout::USE_PREF,
                     0,
                     0);

  // Add a header.
  layout->StartRow(0, 0);
  views::Label* heading_label = new views::Label(delegate_->GetHeadingText());
  heading_label->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::MediumFont));
  heading_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(heading_label);
  layout->AddPaddingRow(0, views::kLabelToControlVerticalSpacing);
  int width = views::Widget::GetLocalizedContentsWidth(
      IDS_EXTENSION_TOOLBAR_REDESIGN_NOTIFICATION_BUBBLE_WIDTH_CHARS);

  // Add the content string.
  layout->StartRow(0, 0);
  views::Label* content_label = new views::Label(delegate_->GetBodyText());
  content_label->SetMultiLine(true);
  content_label->SizeToFit(width);
  content_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(content_label);
  layout->AddPaddingRow(0, views::kLabelToControlVerticalSpacing);

  // Add a "got it" button.
  layout->StartRow(0, 0);
  views::LabelButton* button = new views::LabelButton(
      this, delegate_->GetActionButtonText());
  button->SetStyle(views::Button::STYLE_BUTTON);
  layout->AddView(button,
                  1,
                  1,
                  views::GridLayout::TRAILING,
                  views::GridLayout::FILL);
}

void ExtensionToolbarIconSurfacingBubble::OnWidgetDestroying(
    views::Widget* widget) {
  BubbleDelegateView::OnWidgetDestroying(widget);
  if (!acknowledged_) {
    delegate_->OnBubbleClosed(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS);
    acknowledged_ = true;
  }
}

void ExtensionToolbarIconSurfacingBubble::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  delegate_->OnBubbleClosed(ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE);
  acknowledged_ = true;
  GetWidget()->Close();
}
