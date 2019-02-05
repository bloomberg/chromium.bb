// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_message_bubble_browsertest.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// static
ToolbarActionsBarBubbleViews*
ExtensionMessageBubbleBrowserTest::GetViewsBubbleForBrowser(Browser* browser) {
  return static_cast<ToolbarActionsBarBubbleViews*>(
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetBrowserActionsContainer()
          ->active_bubble());
}

// static
gfx::Rect ExtensionMessageBubbleBrowserTest::GetAnchorReferenceBoundsForBrowser(
    Browser* browser,
    AnchorPosition anchor) {
  auto* toolbar_button_provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar_button_provider();
  auto* browser_actions_container =
      toolbar_button_provider->GetBrowserActionsContainer();
  views::View* anchor_view = nullptr;
  switch (anchor) {
    case ExtensionMessageBubbleBrowserTest::ANCHOR_BROWSER_ACTION:
      EXPECT_GT(browser_actions_container->num_toolbar_actions(), 0u);
      if (browser_actions_container->num_toolbar_actions() == 0)
        return gfx::Rect();
      anchor_view = browser_actions_container->GetToolbarActionViewAt(0);
      break;
    case ExtensionMessageBubbleBrowserTest::ANCHOR_APP_MENU:
      anchor_view = toolbar_button_provider->GetAppMenuButton();
      break;
  }

  EXPECT_TRUE(anchor_view);
  EXPECT_EQ(anchor_view,
            browser_actions_container->active_bubble()->GetAnchorView());
  return anchor_view->GetBoundsInScreen();
}
