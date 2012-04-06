// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/web_intent_picker_test_base.h"
#include "chrome/browser/ui/cocoa/web_intent_picker_cocoa.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "testing/gmock/include/gmock/gmock.h"

class WebIntentSheetControllerBrowserTest : public InProcessBrowserTest,
                                            public WebIntentPickerTestBase {
};

IN_PROC_BROWSER_TEST_F(WebIntentSheetControllerBrowserTest, CloseWillClose) {
  CreateBubble(browser()->GetSelectedTabContentsWrapper());

  EXPECT_CALL(delegate_, OnCancelled()).Times(0);
  EXPECT_CALL(delegate_, OnClosing());
  picker_->Close();

  ignore_result(picker_.release());  // Closing |picker_| will destruct it.
}

IN_PROC_BROWSER_TEST_F(WebIntentSheetControllerBrowserTest,
    DontCancelAfterServiceInvokation) {
  CreateBubble(browser()->GetSelectedTabContentsWrapper());

  GURL url;
  model_.AddInstalledService(string16(), url,
      WebIntentPickerModel::DISPOSITION_WINDOW);

  EXPECT_CALL(delegate_, OnServiceChosen(
      url, WebIntentPickerModel::DISPOSITION_WINDOW));
  EXPECT_CALL(delegate_, OnCancelled()).Times(0);
  EXPECT_CALL(delegate_, OnClosing());

  picker_->OnServiceChosen(0);
  picker_->Close();

  ignore_result(picker_.release());  // Closing |picker_| will destruct it.
}

