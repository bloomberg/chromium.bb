// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "ui/base/models/combobox_model.h"

namespace content {
class WebContents;
}

namespace autofill {

class AutofillDialogView;
struct DetailInput;

// This class drives the dialog that appears when a site uses the imperative
// autocomplete API to fill out a form.
class AutofillDialogController {
 public:
  enum Action {
    AUTOFILL_ACTION_ABORT,
    AUTOFILL_ACTION_SUBMIT,
  };

  explicit AutofillDialogController(content::WebContents* contents);
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

  ui::ComboboxModel* suggested_emails() {
    return &suggested_emails_;
  }
  ui::ComboboxModel* suggested_billing() {
    return &suggested_billing_;
  }
  ui::ComboboxModel* suggested_shipping() {
    return &suggested_shipping_;
  }

  // Called when the view has been closed. The value for |action| indicates
  // whether the Autofill operation should be aborted.
  void ViewClosed(Action action);

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

  // The WebContents where the Autofill action originated.
  content::WebContents* const contents_;

  // Models for the suggestion views.
  SuggestionsComboboxModel suggested_emails_;
  SuggestionsComboboxModel suggested_billing_;
  SuggestionsComboboxModel suggested_shipping_;

  scoped_ptr<AutofillDialogView> view_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
