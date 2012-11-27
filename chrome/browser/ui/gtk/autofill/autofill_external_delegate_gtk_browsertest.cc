// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/autofill/autofill_external_delegate_gtk.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/test_autofill_external_delegate.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockAutofillExternalDelegateGtk : public AutofillExternalDelegateGtk {
 public:
  explicit MockAutofillExternalDelegateGtk(content::WebContents* web_contents)
      : AutofillExternalDelegateGtk(
            web_contents,
            AutofillManager::FromWebContents(web_contents)) {}
  ~MockAutofillExternalDelegateGtk() {}

  virtual void ClearPreviewedForm() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillExternalDelegateGtk);
};

}  // namespace

class AutofillExternalDelegateGtkBrowserTest : public InProcessBrowserTest {
 public:
  AutofillExternalDelegateGtkBrowserTest() {}
  virtual ~AutofillExternalDelegateGtkBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    web_contents_ = chrome::GetActiveWebContents(browser());
    ASSERT_TRUE(web_contents_ != NULL);

    autofill_external_delegate_.reset(
        new MockAutofillExternalDelegateGtk(web_contents_));
  }

 protected:
  content::WebContents* web_contents_;
  scoped_ptr<MockAutofillExternalDelegateGtk> autofill_external_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillExternalDelegateGtkBrowserTest);
};

// Ensure that the Autofill code doesn't crash if the window is closed while
// the Autofill popup is still visible. See http://crbug.com/160866
IN_PROC_BROWSER_TEST_F(AutofillExternalDelegateGtkBrowserTest,
                       CloseBrowserWithPopupOpen) {
  autofill::GenerateTestAutofillPopup(autofill_external_delegate_.get());

  chrome::CloseWindow(browser());
}
