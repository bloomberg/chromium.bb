// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/test_html_dialog_ui_delegate.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestDialogClosedDelegate : public test::TestHtmlDialogUIDelegate {
 public:
  TestDialogClosedDelegate()
      : test::TestHtmlDialogUIDelegate(GURL(chrome::kChromeUIChromeURLsURL)),
        dialog_closed_(false) {
  }

  // Overridden from HtmlDialogUIDelegate:
  virtual ui::ModalType GetDialogModalType() const OVERRIDE {
    return ui::MODAL_TYPE_NONE;
  }

  // Overridden from HtmlDialogUIDelegate:
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE {
    dialog_closed_ = true;
  }

  bool dialog_closed() {
    return dialog_closed_;
  }

 private:
  bool dialog_closed_;
};

}  // namespace

class HtmlDialogControllerBrowserTest : public InProcessBrowserTest {
 public:
  HtmlDialogControllerBrowserTest() {}
};

// Tests that an HtmlDialog can be shown for an incognito browser and that when
// that browser is closed the dialog created by that browser is closed.
IN_PROC_BROWSER_TEST_F(HtmlDialogControllerBrowserTest, IncognitoBrowser) {
  Browser* browser = CreateIncognitoBrowser();
  scoped_ptr<TestDialogClosedDelegate> delegate(new TestDialogClosedDelegate());

  // Create the dialog and make sure the initial "closed" state is what we
  // expect.
  browser->BrowserShowHtmlDialog(delegate.get(), NULL, STYLE_GENERIC);
  ui_test_utils::RunAllPendingInMessageLoop();
  ASSERT_FALSE(delegate->dialog_closed());

  // Closing the browser should close the dialogs associated with that browser.
  browser->CloseWindow();
  ui_test_utils::RunAllPendingInMessageLoop();
  ASSERT_TRUE(delegate->dialog_closed());
}
