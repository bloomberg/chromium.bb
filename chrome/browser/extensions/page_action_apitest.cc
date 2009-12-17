// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/test/ui_test_utils.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PageAction) {
  StartHTTPServer();
  ASSERT_TRUE(RunExtensionTest("page_action/basics")) << message_;

  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  ASSERT_EQ(1u, service->extensions()->size());
  Extension* extension = service->extensions()->at(0);
  ASSERT_TRUE(extension);

  {
    // Tell the extension to update the page action state.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        GURL(extension->GetResourceURL("update.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Test that we received the changes.
  int tab_id =
      browser()->GetSelectedTabContents()->controller().session_id().id();
  ExtensionAction* action = extension->page_action();
  ASSERT_TRUE(action);
  EXPECT_EQ("Modified", action->GetTitle(tab_id));

  {
    // Simulate the page action being clicked.
    ResultCatcher catcher;
    int tab_id =
        ExtensionTabUtil::GetTabId(browser()->GetSelectedTabContents());
    ExtensionBrowserEventRouter::GetInstance()->PageActionExecuted(
        browser()->profile(), extension->id(), "", tab_id, "", 0);
    EXPECT_TRUE(catcher.GetNextResult());
  }

  {
    // Tell the extension to update the page action state again.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        GURL(extension->GetResourceURL("update2.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Test that we received the changes.
  tab_id = browser()->GetSelectedTabContents()->controller().session_id().id();
  EXPECT_FALSE(action->GetIcon(tab_id).isNull());
}

// Tests old-style pageActions API that is deprecated but we don't want to
// break.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, OldPageActions) {
  ASSERT_TRUE(RunExtensionTest("page_action/old_api")) << message_;

  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  ASSERT_EQ(1u, service->extensions()->size());
  Extension* extension = service->extensions()->at(0);
  ASSERT_TRUE(extension);

  // Have the extension enable the page action.
  {
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        GURL(extension->GetResourceURL("page.html")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Simulate the page action being clicked.
  {
    ResultCatcher catcher;
    int tab_id =
        ExtensionTabUtil::GetTabId(browser()->GetSelectedTabContents());
    ExtensionBrowserEventRouter::GetInstance()->PageActionExecuted(
        browser()->profile(), extension->id(), "action", tab_id, "", 1);
    EXPECT_TRUE(catcher.GetNextResult());
  }
}

class PageActionPopupTest : public ExtensionApiTest {
public:
  bool RunExtensionTest(const char* extension_name) {
    last_action_ = NULL;
    last_visibility_ = false;
    waiting_ = false;

    return ExtensionApiTest::RunExtensionTest(extension_name);
  };

  void Observe(NotificationType type, const NotificationSource& source,
               const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED: {
        last_action_ = Source<ExtensionAction>(source).ptr();
        TabContents* contents = Details<TabContents>(details).ptr();
        int tab_id = ExtensionTabUtil::GetTabId(contents);
        last_visibility_ = last_action_->GetIsVisible(tab_id);
        if (waiting_)
          MessageLoopForUI::current()->Quit();
        break;
      }
      default:
        ExtensionBrowserTest::Observe(type, source, details);
        break;
    }
  }

  void WaitForPopupVisibilityChange() {
    // Wait for EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED to come in.
    if (!last_action_) {
      waiting_ = true;
      ui_test_utils::RunMessageLoop();
      waiting_ = false;
    }
  }

 protected:
  bool waiting_;
  ExtensionAction* last_action_;
  bool last_visibility_;
};

#if defined(OS_MACOSX)
// The following test fails on Mac because some of page action implementation is
// missing in LocationBarView (see http://crbug.com/29898).
#define MAYBE_Show DISABLED_Show
#else
#define MAYBE_Show Show
#endif

// Tests popups in page actions.
IN_PROC_BROWSER_TEST_F(PageActionPopupTest, MAYBE_Show) {
  NotificationRegistrar registrar;
  registrar.Add(this,
                NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
                NotificationService::AllSources());

  ASSERT_TRUE(RunExtensionTest("page_action/popup")) << message_;

  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  ASSERT_EQ(1u, service->extensions()->size());
  Extension* extension = service->extensions()->at(0);
  ASSERT_TRUE(extension);

  // Wait for The page action to actually become visible.
  if (!last_visibility_)
    last_action_ = NULL;
  WaitForPopupVisibilityChange();
  ASSERT_TRUE(last_visibility_);

  LocationBarTesting* location_bar =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();
  ASSERT_EQ(1, location_bar->PageActionVisibleCount());
  ExtensionAction* action = location_bar->GetVisiblePageAction(0);
  EXPECT_EQ(action, last_action_);

  {
    ResultCatcher catcher;
    location_bar->TestPageActionPressed(0);
    ASSERT_TRUE(catcher.GetNextResult());
  }
}
