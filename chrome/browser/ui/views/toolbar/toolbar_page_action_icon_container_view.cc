// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_page_action_icon_container_view.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/save_card_icon_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"

ToolbarPageActionIconContainerView::ToolbarPageActionIconContainerView(
    CommandUpdater* command_updater,
    Browser* browser)
    : ToolbarIconContainerView(), browser_(browser) {
  local_card_migration_icon_view_ = new autofill::LocalCardMigrationIconView(
      command_updater, browser, this,
      // TODO(932818): The font list and the icon color may not be what we want
      // here. Put placeholders for now.
      views::style::GetFont(CONTEXT_TOOLBAR_BUTTON,
                            views::style::STYLE_PRIMARY));
  local_card_migration_icon_view_->Init();
  local_card_migration_icon_view_->SetVisible(false);
  AddChildView(local_card_migration_icon_view_);
}

ToolbarPageActionIconContainerView::~ToolbarPageActionIconContainerView() =
    default;

void ToolbarPageActionIconContainerView::UpdateAllIcons() {
  if (local_card_migration_icon_view_)
    local_card_migration_icon_view_->Update();
}

PageActionIconView* ToolbarPageActionIconContainerView::GetIconView(
    PageActionIconType icon_type) {
  switch (icon_type) {
    case PageActionIconType::kLocalCardMigration:
      return local_card_migration_icon_view_;
    default:
      return nullptr;
  }
}

void ToolbarPageActionIconContainerView::UpdatePageActionIcon(
    PageActionIconType icon_type) {
  PageActionIconView* icon = GetIconView(icon_type);
  if (icon)
    icon->Update();
}

SkColor ToolbarPageActionIconContainerView::GetPageActionInkDropColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
}

content::WebContents*
ToolbarPageActionIconContainerView::GetWebContentsForPageActionIconView() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
