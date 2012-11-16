// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_

#include <map>
#include <set>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/autofill/field_types.h"
#include "ui/base/models/combobox_model.h"

namespace content {
class WebContents;
}

namespace autofill {

class AutofillDialogView;
struct DetailInput;

// Sections of the dialog --- all fields that may be shown to the user fit under
// one of these sections. TODO(estade): add telephone number.
enum DialogSection {
  SECTION_EMAIL,
  SECTION_BILLING,
  SECTION_SHIPPING,
};

// Termination actions for the dialog.
enum DialogAction {
  ACTION_ABORT,
  ACTION_SUBMIT,
};

typedef std::vector<const DetailInput*> DetailInputs;
typedef std::map<AutofillFieldType, string16> DetailOutputMap;

// This class drives the dialog that appears when a site uses the imperative
// autocomplete API to fill out a form.
class AutofillDialogController {
 public:
  AutofillDialogController(
      content::WebContents* contents,
      const std::vector<AutofillFieldType>& requested_data);
  ~AutofillDialogController();

  void Show();

  // Called by the view.
  string16 DialogTitle() const;
  string16 IntroText() const;
  string16 EmailSectionLabel() const;
  string16 BillingSectionLabel() const;
  string16 UseBillingForShippingText() const;
  string16 ShippingSectionLabel() const;
  string16 WalletOptionText() const;
  bool ShouldShowInput(const DetailInput& input) const;
  string16 CancelButtonText() const;
  string16 ConfirmButtonText() const;
  bool ConfirmButtonEnabled() const;
  // Returns the set of inputs the page has requested which fall under
  // |section|.
  const DetailInputs& RequestedFieldsForSection(DialogSection section) const;
  // Returns the model for suggestions for fields that fall under |section|.
  ui::ComboboxModel* SuggestionModelForSection(DialogSection section);

  // Called when the view has been closed. The value for |action| indicates
  // whether the Autofill operation should be aborted.
  void ViewClosed(DialogAction action);

  content::WebContents* web_contents() { return contents_; }

 private:
  // A model for the comboboxes that allow the user to select known data.
  class SuggestionsComboboxModel : public ui::ComboboxModel {
   public:
    SuggestionsComboboxModel();
    virtual ~SuggestionsComboboxModel();

    void AddItem(const string16& item);

    // ui::Combobox implementation:
    virtual int GetItemCount() const OVERRIDE;
    virtual string16 GetItemAt(int index) OVERRIDE;

   private:
    std::vector<string16> items_;

    DISALLOW_COPY_AND_ASSIGN(SuggestionsComboboxModel);
  };

  // Fills in |section|-related fields in |output_| according to the state of
  // |view_|.
  void FillOutputForSection(DialogSection section);

  // The WebContents where the Autofill action originated.
  content::WebContents* const contents_;

  // The fields for billing and shipping which the page has actually requested.
  DetailInputs requested_email_fields_;
  DetailInputs requested_billing_fields_;
  DetailInputs requested_shipping_fields_;

  // Models for the suggestion views.
  SuggestionsComboboxModel suggested_emails_;
  SuggestionsComboboxModel suggested_billing_;
  SuggestionsComboboxModel suggested_shipping_;

  scoped_ptr<AutofillDialogView> view_;

  // The data collected from the user, built up after the user presses 'submit'.
  DetailOutputMap output_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
