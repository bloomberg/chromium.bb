// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/mock_autofill_dialog_controller.h"

namespace autofill {
MockAutofillDialogController::MockAutofillDialogController() {}

MockAutofillDialogController::~MockAutofillDialogController() {}

string16 MockAutofillDialogController::DialogTitle() const {
  return string16();
}

string16 MockAutofillDialogController::AccountChooserText() const {
  return string16();
}

string16 MockAutofillDialogController::SignInLinkText() const {
  return string16();
}

string16 MockAutofillDialogController::EditSuggestionText() const {
  return string16();
}

string16 MockAutofillDialogController::CancelButtonText() const {
  return string16();
}

string16 MockAutofillDialogController::ConfirmButtonText() const {
  return string16();
}

string16 MockAutofillDialogController::CancelSignInText() const {
  return string16();
}

string16 MockAutofillDialogController::SaveLocallyText() const {
  return string16();
}

string16 MockAutofillDialogController::ProgressBarText() const {
  return string16();
}

string16 MockAutofillDialogController::LegalDocumentsText() {
  return string16();
}

DialogSignedInState MockAutofillDialogController::SignedInState() const {
   return REQUIRES_RESPONSE;
}

bool MockAutofillDialogController::ShouldShowSpinner() const { return false; }

bool MockAutofillDialogController::ShouldOfferToSaveInChrome() const {
   return false;
}

gfx::Image MockAutofillDialogController::AccountChooserImage() {
  return gfx::Image();
}

bool MockAutofillDialogController::AutocheckoutIsRunning() const {
  return false;
}

bool MockAutofillDialogController::HadAutocheckoutError() const {
  return false;
}

bool MockAutofillDialogController::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return false;
}

const std::vector<ui::Range>&
    MockAutofillDialogController::LegalDocumentLinks() {
  return range_;
}

bool MockAutofillDialogController::SectionIsActive(
    DialogSection section) const {
  return false;
}

const DetailInputs& MockAutofillDialogController::RequestedFieldsForSection(
     DialogSection section) const {
  return inputs_;
}

ui::ComboboxModel* MockAutofillDialogController::ComboboxModelForAutofillType(
     AutofillFieldType type) {
  return NULL;
}

ui::MenuModel* MockAutofillDialogController::MenuModelForSection(
    DialogSection section) {
  return NULL;
}

string16 MockAutofillDialogController::LabelForSection(
    DialogSection section) const {
  return string16();
}

SuggestionState MockAutofillDialogController::SuggestionStateForSection(
    DialogSection section) {
  return SuggestionState(string16(),
                         gfx::Font::NORMAL,
                         gfx::Image(),
                         string16(),
                         gfx::Image(),
                         false);
}

void MockAutofillDialogController::EditClickedForSection(
    DialogSection section) {}

void MockAutofillDialogController::EditCancelledForSection(
    DialogSection section) {}

gfx::Image MockAutofillDialogController::IconForField(
    AutofillFieldType type, const string16& user_input) const {
   return gfx::Image();
}

bool MockAutofillDialogController::InputIsValid(AutofillFieldType type,
                                                const string16& value) const {
  return false;
}

ValidityData MockAutofillDialogController::InputsAreValid(
     const DetailOutputMap& inputs,
     ValidationType validation_type) const {
  return ValidityData();
}

void MockAutofillDialogController::UserEditedOrActivatedInput(
    const DetailInput* input,
    gfx::NativeView parent_view,
    const gfx::Rect& content_bounds,
    const string16& field_contents,
    bool was_edit) {
}

bool MockAutofillDialogController::HandleKeyPressEventInInput(
     const content::NativeWebKeyboardEvent& event) {
  return false;
}

void MockAutofillDialogController::FocusMoved() {
}

void MockAutofillDialogController::ViewClosed() {
}

std::vector<DialogNotification>
    MockAutofillDialogController::CurrentNotifications() const {
  return std::vector<DialogNotification>();
}

void MockAutofillDialogController::StartSignInFlow() {}

void MockAutofillDialogController::EndSignInFlow() {}

void MockAutofillDialogController::NotificationCheckboxStateChanged(
    DialogNotification::Type type,
    bool checked) {}

void MockAutofillDialogController::LegalDocumentLinkClicked(
    const ui::Range& range) {}

void MockAutofillDialogController::OnCancel() {}

void MockAutofillDialogController::OnAccept() {}

content::WebContents* MockAutofillDialogController::web_contents() {
  return NULL;
}

}  // namespace autofill
