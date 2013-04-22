// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/browser/autofill_manager.h"
#include "components/autofill/browser/test_autofill_external_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"

namespace autofill {
namespace {

class TestAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  TestAutofillExternalDelegate(content::WebContents* web_contents,
                               AutofillManager* autofill_manager)
      : AutofillExternalDelegate(web_contents, autofill_manager),
        popup_hidden_(true) {}
  virtual ~TestAutofillExternalDelegate() {}

  virtual void OnPopupShown(content::KeyboardListener* listener) OVERRIDE {
    popup_hidden_ = false;

    AutofillExternalDelegate::OnPopupShown(listener);
  }

  virtual void OnPopupHidden(content::KeyboardListener* listener) OVERRIDE {
    popup_hidden_ = true;

    if (message_loop_runner_)
      message_loop_runner_->Quit();

    AutofillExternalDelegate::OnPopupHidden(listener);
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
  virtual ~AutofillPopupControllerBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents_ != NULL);
    Observe(web_contents_);

    autofill_external_delegate_.reset(
       new TestAutofillExternalDelegate(
           web_contents_,
           AutofillManager::FromWebContents(web_contents_)));
  }

  // Normally the WebContents will automatically delete the delegate, but here
  // the delegate is owned by this test, so we have to manually destroy.
  virtual void WebContentsDestroyed(content::WebContents* web_contents)
      OVERRIDE {
    DCHECK_EQ(web_contents_, web_contents);

    autofill_external_delegate_.reset();
  }

 protected:
  content::WebContents* web_contents_;

  scoped_ptr<TestAutofillExternalDelegate> autofill_external_delegate_;
};

// Autofill UI isn't currently hidden on window move on Mac.
// http://crbug.com/180566
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(AutofillPopupControllerBrowserTest,
                       HidePopupOnWindowConfiguration) {
  GenerateTestAutofillPopup(autofill_external_delegate_.get());

  EXPECT_FALSE(autofill_external_delegate_->popup_hidden());

  // Resize the window, which should cause the popup to hide.
  gfx::Rect new_bounds = browser()->window()->GetBounds() - gfx::Vector2d(1, 1);
  browser()->window()->SetBounds(new_bounds);

  autofill_external_delegate_->WaitForPopupHidden();
  EXPECT_TRUE(autofill_external_delegate_->popup_hidden());
}
#endif // !defined(OS_MACOSX)

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
