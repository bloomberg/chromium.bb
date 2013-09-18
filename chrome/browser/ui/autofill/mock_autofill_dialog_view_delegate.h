// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_VIEW_DELEGATE_H_

#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillDialogViewDelegate : public AutofillDialogViewDelegate {
 public:
  MockAutofillDialogViewDelegate();
  virtual ~MockAutofillDialogViewDelegate();

  MOCK_CONST_METHOD0(DialogTitle, string16());
  MOCK_CONST_METHOD0(AccountChooserText, string16());
  MOCK_CONST_METHOD0(SignInLinkText, string16());
  MOCK_CONST_METHOD0(SpinnerText, string16());
  MOCK_CONST_METHOD0(EditSuggestionText, string16());
  MOCK_CONST_METHOD0(CancelButtonText, string16());
  MOCK_CONST_METHOD0(ConfirmButtonText, string16());
  MOCK_CONST_METHOD0(SaveLocallyText, string16());
  MOCK_CONST_METHOD0(SaveLocallyTooltip, string16());
  MOCK_METHOD0(LegalDocumentsText, string16());
  MOCK_CONST_METHOD0(ShouldDisableSignInLink, bool());
  MOCK_CONST_METHOD0(ShouldShowSpinner, bool());
  MOCK_CONST_METHOD0(ShouldOfferToSaveInChrome, bool());
  MOCK_CONST_METHOD0(ShouldSaveInChrome, bool());
  MOCK_METHOD0(MenuModelForAccountChooser, ui::MenuModel*());
  MOCK_METHOD0(AccountChooserImage, gfx::Image());
  MOCK_CONST_METHOD0(ShouldShowProgressBar, bool());
  MOCK_CONST_METHOD0(ButtonStripImage, gfx::Image());
  MOCK_CONST_METHOD0(GetDialogButtons, int());
  MOCK_CONST_METHOD1(IsDialogButtonEnabled, bool(ui::DialogButton button));
  MOCK_METHOD0(GetDialogOverlay, DialogOverlayState());
  MOCK_METHOD0(LegalDocumentLinks, const std::vector<gfx::Range>&());
  MOCK_CONST_METHOD1(SectionIsActive, bool(DialogSection));
  MOCK_CONST_METHOD1(RequestedFieldsForSection,
                     const DetailInputs&(DialogSection));
  MOCK_METHOD1(ComboboxModelForAutofillType,
               ui::ComboboxModel*(ServerFieldType));
  MOCK_METHOD1(MenuModelForSection, ui::MenuModel*(DialogSection));
  MOCK_CONST_METHOD1(LabelForSection, string16(DialogSection section));
  MOCK_METHOD1(SuggestionStateForSection, SuggestionState(DialogSection));
  MOCK_METHOD1(EditClickedForSection, void(DialogSection section));
  MOCK_METHOD1(EditCancelledForSection, void(DialogSection section));
  // TODO(groby): Remove this deprecated method after Mac starts using
  // IconsForFields. http://crbug.com/292876
  MOCK_CONST_METHOD2(IconForField,
                     gfx::Image(ServerFieldType, const string16&));
  MOCK_CONST_METHOD1(IconsForFields, FieldIconMap(const FieldValueMap&));
  MOCK_CONST_METHOD1(FieldControlsIcons, bool(ServerFieldType));
  MOCK_METHOD3(InputValidityMessage,
      string16(DialogSection, ServerFieldType, const string16&));
  MOCK_METHOD3(InputsAreValid, ValidityData(DialogSection,
                                            const DetailOutputMap&,
                                            ValidationType));
  MOCK_METHOD6(UserEditedOrActivatedInput,void(DialogSection,
                                               const DetailInput*,
                                               gfx::NativeView,
                                               const gfx::Rect&,
                                               const string16&,
                                               bool was_edit));
  MOCK_METHOD1(HandleKeyPressEventInInput,
               bool(const content::NativeWebKeyboardEvent& event));
  MOCK_METHOD0(FocusMoved, void());
  MOCK_METHOD0(ViewClosed, void());
  MOCK_METHOD0(CurrentNotifications,std::vector<DialogNotification>());
  MOCK_METHOD1(LinkClicked, void(const GURL&));
  MOCK_METHOD0(SignInLinkClicked, void());
  MOCK_METHOD2(NotificationCheckboxStateChanged,
               void(DialogNotification::Type, bool));
  MOCK_METHOD1(LegalDocumentLinkClicked, void(const gfx::Range&));
  MOCK_METHOD0(OverlayButtonPressed, void());
  MOCK_METHOD0(OnCancel, bool());
  MOCK_METHOD0(OnAccept, bool());
  MOCK_METHOD0(profile, Profile*());
  MOCK_METHOD0(GetWebContents, content::WebContents*());
 private:
  DetailInputs default_inputs_;
  DetailInputs cc_default_inputs_;  // Default inputs for SECTION_CC.
  std::vector<gfx::Range> range_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_VIEW_DELEGATE_H_
