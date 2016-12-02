// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
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

  tab->GetController().Reload(false);
  content::WaitForLoadStop(tab);

  // If the WaitForLoadStop doesn't hang forever, we've passed.
}
