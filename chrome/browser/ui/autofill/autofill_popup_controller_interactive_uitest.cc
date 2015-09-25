// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_external_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace autofill {
namespace {

class TestAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  TestAutofillExternalDelegate(content::WebContents* web_contents,
                               AutofillManager* autofill_manager,
                               AutofillDriver* autofill_driver)
      : AutofillExternalDelegate(autofill_manager, autofill_driver),
        popup_hidden_(true) {}
  ~TestAutofillExternalDelegate() override {}

  void OnPopupShown() override {
    popup_hidden_ = false;

    AutofillExternalDelegate::OnPopupShown();
  }

  void OnPopupHidden() override {
    popup_hidden_ = true;

    if (message_loop_runner_.get())
      message_loop_runner_->Quit();
  }

  void WaitForPopupHidden() {
    if (popup_hidden_)
      return;

    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

  bool popup_hidden() const { return popup_hidden_; }

 private:
  bool popup_hidden_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillExternalDelegate);
};

}  // namespace

class AutofillPopupControllerBrowserTest
    : public InProcessBrowserTest,
      public content::WebContentsObserver {
 public:
  AutofillPopupControllerBrowserTest() {}
  ~AutofillPopupControllerBrowserTest() override {}

  void SetUpOnMainThread() override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents != NULL);
    Observe(web_contents);

    ContentAutofillDriver* driver =
        ContentAutofillDriverFactory::FromWebContents(web_contents)
            ->DriverForFrame(web_contents->GetMainFrame());
    autofill_external_delegate_.reset(
       new TestAutofillExternalDelegate(
           web_contents,
           driver->autofill_manager(),
           driver));
  }

  // Normally the WebContents will automatically delete the delegate, but here
  // the delegate is owned by this test, so we have to manually destroy.
  void RenderFrameDeleted(content::RenderFrameHost* rfh) override {
    if (!rfh->GetParent())
      autofill_external_delegate_.reset();
  }

 protected:
  scoped_ptr<TestAutofillExternalDelegate> autofill_external_delegate_;
};

#if defined(OS_MACOSX)
// Fails on Mac OS. http://crbug.com/453256
#define MAYBE_HidePopupOnWindowMove DISABLED_HidePopupOnWindowMove
#else
#define MAYBE_HidePopupOnWindowMove HidePopupOnWindowMove
#endif
IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest,
                       MAYBE_HidePopupOnWindowMove) {
  GenerateTestAutofillPopup(autofill_external_delegate_.get());

  EXPECT_FALSE(autofill_external_delegate_->popup_hidden());

  // Move the window, which should close the popup.
  gfx::Rect new_bounds = browser()->window()->GetBounds() - gfx::Vector2d(1, 1);
  browser()->window()->SetBounds(new_bounds);

  autofill_external_delegate_->WaitForPopupHidden();
  EXPECT_TRUE(autofill_external_delegate_->popup_hidden());
}

IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest,
                       HidePopupOnWindowResize) {
  GenerateTestAutofillPopup(autofill_external_delegate_.get());

  EXPECT_FALSE(autofill_external_delegate_->popup_hidden());

  // Resize the window, which should cause the popup to hide.
  gfx::Rect new_bounds = browser()->window()->GetBounds();
  new_bounds.Inset(1, 1);
  browser()->window()->SetBounds(new_bounds);

  autofill_external_delegate_->WaitForPopupHidden();
  EXPECT_TRUE(autofill_external_delegate_->popup_hidden());
}

// This test checks that the browser doesn't crash if the delegate is deleted
// before the popup is hidden.
IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest,
                       DeleteDelegateBeforePopupHidden){
  GenerateTestAutofillPopup(autofill_external_delegate_.get());

  // Delete the external delegate here so that is gets deleted before popup is
  // hidden. This can happen if the web_contents are destroyed before the popup
  // is hidden. See http://crbug.com/232475
  autofill_external_delegate_.reset();
}

}  // namespace autofill
