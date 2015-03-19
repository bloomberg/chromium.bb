// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_toolbar_icon_surfacing_bubble_views.h"

#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"

ExtensionToolbarIconSurfacingBubble::ExtensionToolbarIconSurfacingBubble(
    views::View* anchor_view,
    ToolbarActionsBarBubbleDelegate* delegate)
    : views::BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      delegate_(delegate),
      acknowledged_(false) {
  delegate_->OnToolbarActionsBarBubbleShown();
}

ExtensionToolbarIconSurfacingBubble::~ExtensionToolbarIconSurfacingBubble() {
}

void ExtensionToolbarIconSurfacingBubble::Init() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  // Initialize a basic column set of |<padding> <content> <padding>|.
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddPaddingColumn(0, 10);
  columns->AddColumn(views::GridLayout::LEADING,
                     views::GridLayout::LEADING,
                     1,
                     views::GridLayout::USE_PREF,
                     0,
                     0);
  columns->AddPaddingColumn(0, 10);

  // Add a header.
  layout->StartRow(0, 0);
  views::Label* heading_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_EXTENSION_TOOLBAR_BUBBLE_HEADING));
  heading_label->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::MediumFont));
  heading_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(heading_label);
  layout->AddPaddingRow(0, 10);
  int width = heading_label->GetPreferredSize().width();

  // Add the content string.
  layout->StartRow(0, 0);
  views::Label* content_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_EXTENSION_TOOLBAR_BUBBLE_CONTENT));
  content_label->SetMultiLine(true);
  content_label->SizeToFit(width);
  content_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(content_label);
  layout->AddPaddingRow(0, 10);

  // Add a "got it" button.
  layout->StartRow(0, 0);
  views::LabelButton* button = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_EXTENSION_TOOLBAR_BUBBLE_OK));
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
    delegate_->OnToolbarActionsBarBubbleClosed(
        ToolbarActionsBarBubbleDelegate::DISMISSED);
    acknowledged_ = true;
  }
}

void ExtensionToolbarIconSurfacingBubble::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  delegate_->OnToolbarActionsBarBubbleClosed(
      ToolbarActionsBarBubbleDelegate::ACKNOWLEDGED);
  acknowledged_ = true;
  GetWidget()->Close();
}
