// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_CONTROLLER_H_

#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillDialogController : public AutofillDialogController {
 public:
  MockAutofillDialogController();
  virtual ~MockAutofillDialogController();

  virtual string16 DialogTitle() const OVERRIDE;
  virtual string16 AccountChooserText() const OVERRIDE;
  virtual string16 SignInLinkText() const OVERRIDE;
  virtual string16 EditSuggestionText() const OVERRIDE;
  virtual string16 CancelButtonText() const OVERRIDE;
  virtual string16 ConfirmButtonText() const OVERRIDE;
  virtual string16 CancelSignInText() const OVERRIDE;
  virtual string16 SaveLocallyText() const OVERRIDE;
  virtual string16 ProgressBarText() const OVERRIDE;
  virtual string16 LegalDocumentsText() OVERRIDE;
  virtual DialogSignedInState SignedInState() const OVERRIDE;
  virtual bool ShouldShowSpinner() const OVERRIDE;
  virtual bool ShouldOfferToSaveInChrome() const OVERRIDE;
  MOCK_METHOD0(MenuModelForAccountChooser, ui::MenuModel*());
  virtual gfx::Image AccountChooserImage() OVERRIDE;
  virtual bool AutocheckoutIsRunning() const OVERRIDE;
  virtual bool HadAutocheckoutError() const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;
  virtual const std::vector<ui::Range>& LegalDocumentLinks() OVERRIDE;
  virtual bool SectionIsActive(DialogSection section) const OVERRIDE;
  virtual const DetailInputs& RequestedFieldsForSection(
      DialogSection section) const OVERRIDE;
  virtual ui::ComboboxModel* ComboboxModelForAutofillType(
      AutofillFieldType type) OVERRIDE;
  virtual ui::MenuModel* MenuModelForSection(DialogSection section) OVERRIDE;
  virtual string16 LabelForSection(DialogSection section) const OVERRIDE;
  virtual SuggestionState SuggestionStateForSection(
      DialogSection section) OVERRIDE;
  virtual void EditClickedForSection(DialogSection section) OVERRIDE;
  virtual void EditCancelledForSection(DialogSection section) OVERRIDE;
  virtual gfx::Image IconForField(AutofillFieldType type,
                                  const string16& user_input) const OVERRIDE;
  virtual bool InputIsValid(AutofillFieldType type,
                            const string16& value) const OVERRIDE;
  virtual ValidityData InputsAreValid(
      const DetailOutputMap& inputs,
      ValidationType validation_type) const OVERRIDE;
  virtual void UserEditedOrActivatedInput(const DetailInput* input,
                                          gfx::NativeView parent_view,
                                          const gfx::Rect& content_bounds,
                                          const string16& field_contents,
                                          bool was_edit) OVERRIDE;
  virtual bool HandleKeyPressEventInInput(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

  virtual void FocusMoved() OVERRIDE;

  virtual void ViewClosed() OVERRIDE;

  virtual std::vector<DialogNotification>
      CurrentNotifications() const OVERRIDE;

  virtual void StartSignInFlow() OVERRIDE;
  virtual void EndSignInFlow() OVERRIDE;
  virtual void NotificationCheckboxStateChanged(DialogNotification::Type type,
                                                bool checked) OVERRIDE;

  virtual void LegalDocumentLinkClicked(const ui::Range& range) OVERRIDE;
  virtual void OnCancel() OVERRIDE;
  virtual void OnAccept() OVERRIDE;

  MOCK_METHOD0(profile, Profile*());
  virtual content::WebContents* web_contents() OVERRIDE;
 private:
  std::vector<ui::Range> range_;
  DetailInputs inputs_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_CONTROLLER_H_
