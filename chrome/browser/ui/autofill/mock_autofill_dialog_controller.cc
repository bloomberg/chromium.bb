// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/mock_autofill_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

MockAutofillDialogController::MockAutofillDialogController() {
  testing::DefaultValue<const DetailInputs&>::Set(default_inputs_);
  testing::DefaultValue<ui::ComboboxModel*>::Set(NULL);
}

MockAutofillDialogController::~MockAutofillDialogController() {
  testing::DefaultValue<ui::ComboboxModel*>::Clear();
  testing::DefaultValue<const DetailInputs&>::Clear();
}

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

string16 MockAutofillDialogController::SaveLocallyText() const {
  return string16();
}

string16 MockAutofillDialogController::LegalDocumentsText() {
  return string16();
}

DialogSignedInState MockAutofillDialogController::SignedInState() const {
   return REQUIRES_RESPONSE;
}

bool MockAutofillDialogController::ShouldShowSpinner() const {
  return false;
}

bool MockAutofillDialogController::ShouldOfferToSaveInChrome() const {
   return false;
}

gfx::Image MockAutofillDialogController::AccountChooserImage() {
  return gfx::Image();
}

bool MockAutofillDialogController::ShouldShowProgressBar() const {
  return false;
}

int MockAutofillDialogController::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

bool MockAutofillDialogController::ShouldShowDetailArea() const {
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
                         gfx::Image());
}

void MockAutofillDialogController::EditClickedForSection(
    DialogSection section) {}

void MockAutofillDialogController::EditCancelledForSection(
    DialogSection section) {}

gfx::Image MockAutofillDialogController::IconForField(
    AutofillFieldType type, const string16& user_input) const {
   return gfx::Image();
}

string16 MockAutofillDialogController::InputValidityMessage(
    DialogSection section,
    AutofillFieldType type,
    const string16& value) {
  return string16();
}

ValidityData MockAutofillDialogController::InputsAreValid(
     DialogSection section,
     const DetailOutputMap& inputs,
     ValidationType validation_type) {
  return ValidityData();
}

void MockAutofillDialogController::UserEditedOrActivatedInput(
    DialogSection section,
    const DetailInput* input,
    gfx::NativeView parent_view,
    const gfx::Rect& content_bounds,
    const string16& field_contents,
    bool was_edit) {}

bool MockAutofillDialogController::HandleKeyPressEventInInput(
     const content::NativeWebKeyboardEvent& event) {
  return false;
}

void MockAutofillDialogController::FocusMoved() {}

gfx::Image MockAutofillDialogController::SplashPageImage() const {
  return gfx::Image();
}

void MockAutofillDialogController::ViewClosed() {}

std::vector<DialogNotification> MockAutofillDialogController::
    CurrentNotifications() {
  return std::vector<DialogNotification>();
}

std::vector<DialogAutocheckoutStep> MockAutofillDialogController::
    CurrentAutocheckoutSteps() const {
  return std::vector<DialogAutocheckoutStep>();
}

void MockAutofillDialogController::SignInLinkClicked() {}

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
