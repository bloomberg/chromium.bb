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
  virtual string16 SaveLocallyText() const OVERRIDE;
  virtual string16 LegalDocumentsText() OVERRIDE;
  virtual DialogSignedInState SignedInState() const OVERRIDE;
  virtual bool ShouldShowSpinner() const OVERRIDE;
  virtual bool ShouldOfferToSaveInChrome() const OVERRIDE;
  MOCK_METHOD0(MenuModelForAccountChooser, ui::MenuModel*());
  virtual gfx::Image AccountChooserImage() OVERRIDE;
  virtual bool ShouldShowProgressBar() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual bool ShouldShowDetailArea() const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;
  virtual const std::vector<ui::Range>& LegalDocumentLinks() OVERRIDE;
  virtual bool SectionIsActive(DialogSection section) const OVERRIDE;
  MOCK_CONST_METHOD1(RequestedFieldsForSection,
                     const DetailInputs&(DialogSection));
  MOCK_METHOD1(ComboboxModelForAutofillType,
               ui::ComboboxModel*(AutofillFieldType));
  MOCK_METHOD1(MenuModelForSection, ui::MenuModel*(DialogSection));
  virtual string16 LabelForSection(DialogSection section) const OVERRIDE;
  virtual SuggestionState SuggestionStateForSection(
      DialogSection section) OVERRIDE;
  virtual void EditClickedForSection(DialogSection section) OVERRIDE;
  virtual void EditCancelledForSection(DialogSection section) OVERRIDE;
  virtual gfx::Image IconForField(AutofillFieldType type,
                                  const string16& user_input) const OVERRIDE;
  virtual string16 InputValidityMessage(
      DialogSection section,
      AutofillFieldType type,
      const string16& value) OVERRIDE;
  virtual ValidityData InputsAreValid(
      DialogSection section,
      const DetailOutputMap& inputs,
      ValidationType validation_type) OVERRIDE;
  virtual void UserEditedOrActivatedInput(DialogSection section,
                                          const DetailInput* input,
                                          gfx::NativeView parent_view,
                                          const gfx::Rect& content_bounds,
                                          const string16& field_contents,
                                          bool was_edit) OVERRIDE;
  virtual bool HandleKeyPressEventInInput(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

  virtual void FocusMoved() OVERRIDE;

  virtual gfx::Image SplashPageImage() const OVERRIDE;

  virtual void ViewClosed() OVERRIDE;

  virtual std::vector<DialogNotification> CurrentNotifications() OVERRIDE;

  virtual std::vector<DialogAutocheckoutStep> CurrentAutocheckoutSteps()
      const OVERRIDE;

  virtual void SignInLinkClicked() OVERRIDE;
  virtual void NotificationCheckboxStateChanged(DialogNotification::Type type,
                                                bool checked) OVERRIDE;

  virtual void LegalDocumentLinkClicked(const ui::Range& range) OVERRIDE;
  virtual void OnCancel() OVERRIDE;
  virtual void OnAccept() OVERRIDE;

  MOCK_METHOD0(profile, Profile*());
  virtual content::WebContents* web_contents() OVERRIDE;
 private:
  DetailInputs default_inputs_;
  std::vector<ui::Range> range_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_CONTROLLER_H_
