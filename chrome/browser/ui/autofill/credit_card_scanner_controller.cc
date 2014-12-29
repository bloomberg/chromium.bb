// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/credit_card_scanner_controller.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/credit_card_scanner_view.h"
#include "chrome/browser/ui/autofill/credit_card_scanner_view_delegate.h"

namespace autofill {

namespace {

// Controller for the credit card scanner UI. The controller deletes itself
// after the view is dismissed.
class Controller : public CreditCardScannerViewDelegate,
                   public base::SupportsWeakPtr<Controller> {
 public:
  Controller(content::WebContents* web_contents,
             const AutofillClient::CreditCardScanCallback& callback)
      : view_(CreditCardScannerView::Create(AsWeakPtr(), web_contents)),
        callback_(callback) {
    DCHECK(view_);
  }

  // Shows the UI to scan the credit card.
  void Show() {
    view_->Show();
  }

 private:
  ~Controller() override {}

  // CreditCardScannerViewDelegate implementation.
  void ScanCancelled() override {
    delete this;
  }

  // CreditCardScannerViewDelegate implementation.
  void ScanCompleted(const base::string16& card_number,
                     int expiration_month,
                     int expiration_year) override {
    callback_.Run(card_number, expiration_month, expiration_year);
    delete this;
  }

  // The view for the credit card scanner.
  scoped_ptr<CreditCardScannerView> view_;

  // The callback to be invoked when scanning completes successfully.
  AutofillClient::CreditCardScanCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

}  // namespace

// static
bool CreditCardScannerController::HasCreditCardScanFeature() {
  return CreditCardScannerView::CanShow();
}

// static
void CreditCardScannerController::ScanCreditCard(
    content::WebContents* web_contents,
    const AutofillClient::CreditCardScanCallback& callback) {
  (new Controller(web_contents, callback))->Show();
}

}  // namespace autofill
