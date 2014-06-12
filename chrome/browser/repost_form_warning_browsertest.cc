// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

using web_modal::WebContentsModalDialogManager;

typedef InProcessBrowserTest RepostFormWarningTest;

// If becomes flaky, disable on Windows and use http://crbug.com/47228
IN_PROC_BROWSER_TEST_F(RepostFormWarningTest, TestDoubleReload) {
  ASSERT_TRUE(test_server()->Start());

  // Load a form.
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("files/form.html"));
  // Submit it.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("javascript:document.getElementById('form').submit()"));

  // Try to reload it twice, checking for repost.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetController().Reload(true);
  web_contents->GetController().Reload(true);

  // There should only be one dialog open.
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  // Navigate away from the page (this is when the test usually crashes).
  ui_test_utils::NavigateToURL(browser(), test_server()->GetURL("bar"));

  // The dialog should've been closed.
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());
}

// If becomes flaky, disable on Windows and use http://crbug.com/47228
IN_PROC_BROWSER_TEST_F(RepostFormWarningTest, TestLoginAfterRepost) {
  ASSERT_TRUE(test_server()->Start());

  // Load a form.
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("files/form.html"));
  // Submit it.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("javascript:document.getElementById('form').submit()"));

  // Try to reload it, checking for repost.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetController().Reload(true);

  // Navigate to a page that requires authentication, bringing up another
  // tab-modal sheet.
  content::NavigationController& controller = web_contents->GetController();
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_AUTH_NEEDED,
      content::Source<content::NavigationController>(&controller));
  browser()->OpenURL(content::OpenURLParams(
        test_server()->GetURL("auth-basic"), content::Referrer(), CURRENT_TAB,
        content::PAGE_TRANSITION_TYPED, false));
  observer.Wait();

  // Try to reload it again.
  web_contents->GetController().Reload(true);

  // Navigate away from the page. We can't use ui_test_utils:NavigateToURL
  // because that waits for the current page to stop loading first, which won't
  // happen while the auth dialog is up.
  content::TestNavigationObserver navigation_observer(web_contents);
  browser()->OpenURL(content::OpenURLParams(
        test_server()->GetURL("bar"), content::Referrer(), CURRENT_TAB,
        content::PAGE_TRANSITION_TYPED, false));
  navigation_observer.Wait();
}
