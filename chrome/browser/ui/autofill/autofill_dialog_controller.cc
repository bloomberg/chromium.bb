// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_template.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

namespace {

// Looks through |input_template| for the types in |requested_data|. Appends
// DetailInput values to |inputs|.
void FilterInputs(const std::vector<AutofillFieldType>& requested_data,
                  const DetailInput* input_template,
                  size_t template_size,
                  DetailInputs* inputs) {
  for (size_t i = 0; i < template_size; ++i) {
    for (size_t j = 0; j < requested_data.size(); ++j) {
      const DetailInput* input = &input_template[i];
      if (requested_data[j] == input->type) {
        inputs->push_back(input);
        break;
      }
    }
  }
}

}  // namespace

AutofillDialogController::AutofillDialogController(
    content::WebContents* contents,
    const std::vector<AutofillFieldType>& requested_data)
    : contents_(contents) {
  // TODO(estade): replace with real data.
  suggested_emails_.AddItem(ASCIIToUTF16("captain.jack@gmail.com"));
  suggested_emails_.AddItem(ASCIIToUTF16("major.major@gmail.com"));
  suggested_emails_.AddItem(ASCIIToUTF16("Enter new email"));
  suggested_billing_.AddItem(ASCIIToUTF16("this one"));
  suggested_billing_.AddItem(ASCIIToUTF16("that one"));
  suggested_billing_.AddItem(ASCIIToUTF16("Enter new billing"));
  suggested_shipping_.AddItem(ASCIIToUTF16("Enter new shipping"));

  // TODO(estade): remove duplicates from |requested_data|.
  FilterInputs(requested_data,
               kEmailInputs,
               kEmailInputsSize,
               &requested_email_fields_);

  FilterInputs(requested_data,
               kBillingInputs,
               kBillingInputsSize,
               &requested_billing_fields_);

  FilterInputs(requested_data,
               kShippingInputs,
               kShippingInputsSize,
               &requested_shipping_fields_);
}

AutofillDialogController::~AutofillDialogController() {}

void AutofillDialogController::Show() {
  // TODO(estade): don't show the dialog if the site didn't specify the right
  // fields. First we must figure out what the "right" fields are.
  view_.reset(AutofillDialogView::Create(this));
  view_->Show();
}

string16 AutofillDialogController::DialogTitle() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("PaY"));
}

string16 AutofillDialogController::IntroText() const {
  // TODO(estade): real strings and l10n.
  return string16(
      ASCIIToUTF16("random.com has requested the following deets:"));
}

string16 AutofillDialogController::EmailSectionLabel() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("Email address fixme"));
}

string16 AutofillDialogController::BillingSectionLabel() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("Billing details fixme"));
}

string16 AutofillDialogController::UseBillingForShippingText() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("also ship here"));
}

string16 AutofillDialogController::ShippingSectionLabel() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("Shipping details fixme"));
}

string16 AutofillDialogController::WalletOptionText() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("I love lamp."));
}

bool AutofillDialogController::ShouldShowInput(const DetailInput& input) const {
  // TODO(estade): filter fields that aren't part of this autofill request.
  return true;
}

string16 AutofillDialogController::CancelButtonText() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("CaNceL"));
}

string16 AutofillDialogController::ConfirmButtonText() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("SuBMiT"));
}

bool AutofillDialogController::ConfirmButtonEnabled() const {
  // TODO(estade): implement.
  return true;
}

const DetailInputs& AutofillDialogController::RequestedFieldsForSection(
    DialogSection section) const {
  switch (section) {
    case SECTION_EMAIL:
      return requested_email_fields_;
    case SECTION_BILLING:
      return requested_billing_fields_;
    case SECTION_SHIPPING:
      return requested_shipping_fields_;
  }

  NOTREACHED();
  return requested_shipping_fields_;
}

ui::ComboboxModel* AutofillDialogController::SuggestionModelForSection(
    DialogSection section) {
  switch (section) {
    case SECTION_EMAIL:
      return &suggested_emails_;
    case SECTION_BILLING:
      return &suggested_billing_;
    case SECTION_SHIPPING:
      return &suggested_shipping_;
  }

  NOTREACHED();
  return NULL;
}

void AutofillDialogController::ViewClosed(DialogAction action) {
  if (action == ACTION_SUBMIT) {
    FillOutputForSection(SECTION_EMAIL);
    FillOutputForSection(SECTION_BILLING);
    if (view_->UseBillingForShipping()) {
      // TODO(estade): fill in shipping info from billing info.
    } else {
      FillOutputForSection(SECTION_SHIPPING);
    }
    // TODO(estade): pass the result along to the page.
  }

  delete this;
}

void AutofillDialogController::FillOutputForSection(DialogSection section) {
  int suggestion_selection = view_->GetSuggestionSelection(section);
  if (suggestion_selection <
          SuggestionModelForSection(section)->GetItemCount() - 1) {
    // TODO(estade): Fill in |output_| from suggestion.
  } else {
    view_->GetUserInput(section, &output_);
  }
}

// SuggestionsComboboxModel ----------------------------------------------------

AutofillDialogController::SuggestionsComboboxModel::SuggestionsComboboxModel() {
}

AutofillDialogController::SuggestionsComboboxModel::
    ~SuggestionsComboboxModel() {}

void AutofillDialogController::SuggestionsComboboxModel::AddItem(
    const string16& item) {
  items_.push_back(item);
}

int AutofillDialogController::SuggestionsComboboxModel::GetItemCount() const {
  return items_.size();
}

string16 AutofillDialogController::SuggestionsComboboxModel::GetItemAt(
    int index) {
  return items_.at(index);
}

}  // namespace autofill

