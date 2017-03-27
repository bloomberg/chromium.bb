// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_ADDRESS_EDITOR_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_ADDRESS_EDITOR_VIEW_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/views/payments/editor_view_controller.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"

namespace payments {

class PaymentRequestSpec;
class PaymentRequestState;
class PaymentRequestDialogView;

// Shipping Address editor screen of the Payment Request flow.
class ShippingAddressEditorViewController : public EditorViewController {
 public:
  // Does not take ownership of the arguments, which should outlive this object.
  ShippingAddressEditorViewController(PaymentRequestSpec* spec,
                                      PaymentRequestState* state,
                                      PaymentRequestDialogView* dialog);
  ~ShippingAddressEditorViewController() override;

  // EditorViewController:
  std::unique_ptr<views::View> CreateHeaderView() override;
  std::vector<EditorField> GetFieldDefinitions() override;
  bool ValidateModelAndSave() override;
  std::unique_ptr<ValidationDelegate> CreateValidationDelegate(
      const EditorField& field) override;
  std::unique_ptr<ui::ComboboxModel> GetComboboxModelForType(
      const autofill::ServerFieldType& type) override;
  void OnPerformAction(views::Combobox* combobox) override;
  void UpdateEditorView() override;

  // PaymentRequestSheetController:
  base::string16 GetSheetTitle() override;

 private:
  class ShippingAddressValidationDelegate : public ValidationDelegate {
   public:
    ShippingAddressValidationDelegate(
        ShippingAddressEditorViewController* parent,
        const EditorField& field);
    ~ShippingAddressValidationDelegate() override;

    // ValidationDelegate:
    bool ValidateTextfield(views::Textfield* textfield) override;
    bool ValidateCombobox(views::Combobox* combobox) override;

   private:
    bool ValidateValue(const base::string16& value);

    EditorField field_;
    // Raw pointer back to the owner of this class, therefore will not be null.
    ShippingAddressEditorViewController* controller_;

    DISALLOW_COPY_AND_ASSIGN(ShippingAddressValidationDelegate);
  };
  friend class ShippingAddressValidationDelegate;

  // List of fields, reset everytime the current country changes.
  std::vector<EditorField> editor_fields_;

  // The currently chosen country. Defaults to 0 as the first entry in the
  // combobox, which is the generated default value received from
  // autofill::CountryComboboxModel::countries() which is documented to always
  // have the default country at the top as well as within the sorted list.
  size_t chosen_country_index_{0};

  // The list of country codes as ordered in the country combobox model.
  std::vector<std::string> country_codes_;

  // Updates |editor_fields_| based on the current country.
  void UpdateEditorFields();

  // Called by the validation delegate when the country combobox changed.
  void OnCountryChanged(views::Combobox* combobox);

  DISALLOW_COPY_AND_ASSIGN(ShippingAddressEditorViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_ADDRESS_EDITOR_VIEW_CONTROLLER_H_
