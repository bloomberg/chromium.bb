// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

using JavaScriptDialogTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest, ReloadDoesntHang) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAutoDismissingDialogs);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  JavaScriptDialogTabHelper* js_helper =
      JavaScriptDialogTabHelper::FromWebContents(tab);

  // Show a dialog.
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  js_helper->SetDialogShownCallbackForTesting(runner->QuitClosure());
  tab->GetMainFrame()->ExecuteJavaScriptForTests(base::UTF8ToUTF16("alert()"));
  runner->Run();

  // Try reloading.
  tab->GetController().Reload(content::ReloadType::NORMAL, false);
  content::WaitForLoadStop(tab);

  // If the WaitForLoadStop doesn't hang forever, we've passed.
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest,
                       ClosingPageSharingRendererDoesntHang) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAutoDismissingDialogs);

  // Turn off popup blocking.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisablePopupBlocking);

  // Two tabs, one render process.
  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContentsAddedObserver new_wc_observer;
  tab1->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16("window.open('about:blank');"));
  content::WebContents* tab2 = new_wc_observer.GetWebContents();
  ASSERT_NE(tab1, tab2);
  ASSERT_EQ(tab1->GetRenderProcessHost(), tab2->GetRenderProcessHost());

  // Tab two shows a dialog.
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  JavaScriptDialogTabHelper* js_helper2 =
      JavaScriptDialogTabHelper::FromWebContents(tab2);
  js_helper2->SetDialogShownCallbackForTesting(runner->QuitClosure());
  tab2->GetMainFrame()->ExecuteJavaScriptForTests(base::UTF8ToUTF16("alert()"));
  runner->Run();

  // Tab two is closed while the dialog is up.
  int tab2_index = browser()->tab_strip_model()->GetIndexOfWebContents(tab2);
  browser()->tab_strip_model()->CloseWebContentsAt(tab2_index,
                                                   TabStripModel::CLOSE_NONE);

  // Try reloading tab one.
  tab1->GetController().Reload(content::ReloadType::NORMAL, false);
  content::WaitForLoadStop(tab1);

  // If the WaitForLoadStop doesn't hang forever, we've passed.
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest,
                       ClosingPageWithSubframeAlertingDoesntCrash) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAutoDismissingDialogs);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  JavaScriptDialogTabHelper* js_helper =
      JavaScriptDialogTabHelper::FromWebContents(tab);

  // A subframe shows a dialog.
  std::string dialog_url = "data:text/html,<script>alert(\"hi\");</script>";
  std::string script = "var iframe = document.createElement('iframe');"
                       "iframe.src = '" + dialog_url + "';"
                       "document.body.appendChild(iframe);";
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  js_helper->SetDialogShownCallbackForTesting(runner->QuitClosure());
  tab->GetMainFrame()->ExecuteJavaScriptForTests(base::UTF8ToUTF16(script));
  runner->Run();

  // The tab is closed while the dialog is up.
  int tab_index = browser()->tab_strip_model()->GetIndexOfWebContents(tab);
  browser()->tab_strip_model()->CloseWebContentsAt(tab_index,
                                                   TabStripModel::CLOSE_NONE);

  // No crash is good news.
}
