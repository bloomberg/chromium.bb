// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_PAGE_ACTION_ICON_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_PAGE_ACTION_ICON_CONTAINER_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

class Browser;
class CommandUpdater;

namespace autofill {
class LocalCardMigrationIconView;
}  // namespace autofill

// A container view for user-account-related PageActionIconViews and the profile
// avatar icon. This container view is in the toolbar while the other
// PageActionIconContainerView is in the omnibox and it is used to handle other
// non user-account-related page action icons.
class ToolbarPageActionIconContainerView : public ToolbarIconContainerView,
                                           public PageActionIconContainer,
                                           public PageActionIconView::Delegate {
 public:
  ToolbarPageActionIconContainerView(CommandUpdater* command_updater,
                                     Browser* browser);
  ~ToolbarPageActionIconContainerView() override;

  PageActionIconView* GetIconView(PageActionIconType icon_type);

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;

  // PageActionIconContainer:
  void UpdatePageActionIcon(PageActionIconType icon_type) override;

  // PageActionIconView::Delegate:
  SkColor GetPageActionInkDropColor() const override;
  content::WebContents* GetWebContentsForPageActionIconView() override;

  autofill::LocalCardMigrationIconView* local_card_migration_icon_view() {
    return local_card_migration_icon_view_;
  }

 private:
  autofill::LocalCardMigrationIconView* local_card_migration_icon_view_ =
      nullptr;

  Browser* const browser_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarPageActionIconContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_PAGE_ACTION_ICON_CONTAINER_VIEW_H_
