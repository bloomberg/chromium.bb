// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_CREDIT_CARD_EDITOR_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_CREDIT_CARD_EDITOR_VIEW_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/payments/editor_view_controller.h"

namespace payments {

class PaymentRequest;
class PaymentRequestDialogView;

// Credit card editor screen of the Payment Request flow.
class CreditCardEditorViewController : public EditorViewController {
 public:
  // Does not take ownership of the arguments, which should outlive this object.
  CreditCardEditorViewController(PaymentRequest* request,
                                 PaymentRequestDialogView* dialog);
  ~CreditCardEditorViewController() override;

  // EditorViewController implementation.
  std::vector<EditorField> GetFieldDefinitions() override;
  bool ValidateModelAndSave() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CreditCardEditorViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_CREDIT_CARD_EDITOR_VIEW_CONTROLLER_H_
