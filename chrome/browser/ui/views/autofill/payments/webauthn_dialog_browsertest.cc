// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/webauthn_dialog_view_impl.h"

namespace autofill {

namespace {
// If ShowAndVerifyUi() is used, the tests must be named as "InvokeUi_"
// appending these names.
constexpr char kOfferDialogName[] = "Offer";
constexpr char kVerifyDialogName[] = "Verify";
}  // namespace

class WebauthnDialogBrowserTest : public DialogBrowserTest {
 public:
  WebauthnDialogBrowserTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Do lazy initialization of WebauthnDialogControllerImpl.
    WebauthnDialogControllerImpl::CreateForWebContents(web_contents);

    if (name == kOfferDialogName) {
      controller()->ShowOfferDialog(base::DoNothing());
    } else if (name == kVerifyDialogName) {
      controller()->ShowVerifyPendingDialog(base::DoNothing());
    }
  }

  WebauthnDialogViewImpl* GetWebauthnDialog() {
    if (!controller())
      return nullptr;

    WebauthnDialogView* dialog_view = controller()->dialog_view();
    if (!dialog_view)
      return nullptr;

    return static_cast<WebauthnDialogViewImpl*>(dialog_view);
  }

  WebauthnDialogControllerImpl* controller() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents())
      return nullptr;

    return WebauthnDialogControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebauthnDialogBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest, InvokeUi_Offer) {
  ShowAndVerifyUi();
}

// Ensures closing tab while dialog being visible is correctly handled.
// RunUntilIdle() makes sure that nothing crashes after browser tab is closed.
IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       OfferDialog_CanCloseTabWhileDialogShowing) {
  ShowUi(kOfferDialogName);
  VerifyUi();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures closing browser while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       OfferDialog_CanCloseBrowserWhileDialogShowing) {
  ShowUi(kOfferDialogName);
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures dialog is closed when cancel button is clicked.
IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       OfferDialog_ClickCancelButton) {
  ShowUi(kOfferDialogName);
  VerifyUi();
  GetWebauthnDialog()->CancelDialog();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest, InvokeUi_Verify) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       VerifyPendingDialog_CanCloseTabWhileDialogShowing) {
  ShowUi(kVerifyDialogName);
  VerifyUi();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures closing browser while dialog being visible is correctly handled.
IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       VerifyPendingDialog_CanCloseBrowserWhileDialogShowing) {
  ShowUi(kVerifyDialogName);
  VerifyUi();
  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();
}

// Ensures dialog can be closed when verification finishes.
IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       VerifyPendingDialog_VerificationFinishes) {
  ShowUi(kVerifyDialogName);
  VerifyUi();
  controller()->CloseDialog();
  base::RunLoop().RunUntilIdle();
}

// Ensures dialog is closed when cancel button is clicked.
IN_PROC_BROWSER_TEST_F(WebauthnDialogBrowserTest,
                       VerifyPendingDialog_ClickCancelButton) {
  ShowUi(kVerifyDialogName);
  VerifyUi();
  GetWebauthnDialog()->CancelDialog();
  base::RunLoop().RunUntilIdle();
}

}  // namespace autofill
