// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_CREDIT_CARD_EDITOR_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_CREDIT_CARD_EDITOR_VIEW_CONTROLLER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/payments/editor_view_controller.h"
#include "chrome/browser/ui/views/payments/validation_delegate.h"
#include "ui/base/models/simple_combobox_model.h"

namespace autofill {
class CreditCard;
}  // namespace autofill

namespace payments {

class PaymentRequestSpec;
class PaymentRequestState;
class PaymentRequestDialogView;

// Credit card editor screen of the Payment Request flow.
class CreditCardEditorViewController : public EditorViewController {
 public:
  // Does not take ownership of the arguments, which should outlive this object.
  // Additionally, |credit_card| could be nullptr if we are adding a card. Else,
  // it's a valid pointer to a card that needs to be updated, and which will
  // outlive this controller.
  CreditCardEditorViewController(
      PaymentRequestSpec* spec,
      PaymentRequestState* state,
      PaymentRequestDialogView* dialog,
      base::OnceClosure on_edited,
      base::OnceCallback<void(const autofill::CreditCard&)> on_added,
      autofill::CreditCard* credit_card);
  ~CreditCardEditorViewController() override;

  // EditorViewController:
  std::unique_ptr<views::View> CreateHeaderView() override;
  std::vector<EditorField> GetFieldDefinitions() override;
  base::string16 GetInitialValueForType(
      autofill::ServerFieldType type) override;
  bool ValidateModelAndSave() override;
  std::unique_ptr<ValidationDelegate> CreateValidationDelegate(
      const EditorField& field) override;
  std::unique_ptr<ui::ComboboxModel> GetComboboxModelForType(
      const autofill::ServerFieldType& type) override;

 protected:
  // PaymentRequestSheetController:
  base::string16 GetSheetTitle() override;

 private:
  class CreditCardValidationDelegate : public ValidationDelegate {
   public:
    // Used to validate |field| type. A reference to the |controller| should
    // outlive this delegate, and a list of |supported_card_networks| can be
    // passed in to validate |field| (the data will be copied to the delegate).
    CreditCardValidationDelegate(
        const EditorField& field,
        EditorViewController* controller,
        const std::vector<std::string>& supported_card_networks);
    ~CreditCardValidationDelegate() override;

    // ValidationDelegate:
    bool ValidateTextfield(views::Textfield* textfield) override;
    bool ValidateCombobox(views::Combobox* combobox) override;

   private:
    // Validates a specific |value|.
    bool ValidateValue(const base::string16& value);

    EditorField field_;
    // Outlives this class.
    EditorViewController* controller_;
    // The list of supported basic card networks.
    std::set<std::string> supported_card_networks_;

    DISALLOW_COPY_AND_ASSIGN(CreditCardValidationDelegate);
  };

  // Called when |credit_card_to_edit_| was successfully edited.
  base::OnceClosure on_edited_;
  // Called when a new card was added. The const reference is short-lived, and
  // the callee should make a copy.
  base::OnceCallback<void(const autofill::CreditCard&)> on_added_;

  // If non-nullptr, a pointer to an object to be edited. Must outlive this
  // controller.
  autofill::CreditCard* credit_card_to_edit_;

  DISALLOW_COPY_AND_ASSIGN(CreditCardEditorViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_CREDIT_CARD_EDITOR_VIEW_CONTROLLER_H_
