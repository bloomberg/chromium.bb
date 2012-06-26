// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/test_web_dialog_delegate.h"
#include "chrome/browser/ui/webui/web_dialog_controller.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestDialogClosedDelegate : public test::TestWebDialogDelegate {
 public:
  explicit TestDialogClosedDelegate(Browser* browser)
      : test::TestWebDialogDelegate(
            GURL(chrome::kChromeUIChromeURLsURL)),
        dialog_closed_(false) {
    if (browser) {
      web_dialog_controller_.reset(
          new WebDialogController(this, browser->profile(), browser));
    }
  }

  bool dialog_closed() const { return dialog_closed_; }

  // Overridden from ui::WebDialogDelegate:
  virtual ui::ModalType GetDialogModalType() const OVERRIDE {
    return ui::MODAL_TYPE_NONE;
  }
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE {
    dialog_closed_ = true;
  }

 private:
  scoped_ptr<WebDialogController> web_dialog_controller_;
  bool dialog_closed_;

  DISALLOW_COPY_AND_ASSIGN(TestDialogClosedDelegate);
};

}  // namespace

class WebDialogControllerBrowserTest : public InProcessBrowserTest {
 public:
  WebDialogControllerBrowserTest() {}
};

// Tests that a WebDialog can be shown for an incognito browser and that when
// that browser is closed the dialog created by that browser is closed.
IN_PROC_BROWSER_TEST_F(WebDialogControllerBrowserTest, IncognitoBrowser) {
  Browser* browser = CreateIncognitoBrowser();
  scoped_ptr<TestDialogClosedDelegate> delegate(
      new TestDialogClosedDelegate(browser));

  // Create the dialog and make sure the initial "closed" state is what we
  // expect.
  browser::ShowWebDialog(browser->window()->GetNativeWindow(),
                         browser->profile(),
                         delegate.get());
  ui_test_utils::RunAllPendingInMessageLoop();
  ASSERT_FALSE(delegate->dialog_closed());

  // Closing the browser should close the dialogs associated with that browser.
  chrome::CloseWindow(browser);
  ui_test_utils::RunAllPendingInMessageLoop();
  ASSERT_TRUE(delegate->dialog_closed());
}
