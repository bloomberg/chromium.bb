// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_VIEW_DELEGATE_H_

#include "base/basictypes.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/range/range.h"

namespace autofill {

class MockAutofillDialogViewDelegate : public AutofillDialogViewDelegate {
 public:
  MockAutofillDialogViewDelegate();
  virtual ~MockAutofillDialogViewDelegate();

  MOCK_CONST_METHOD0(DialogTitle, base::string16());
  MOCK_CONST_METHOD0(AccountChooserText, base::string16());
  MOCK_CONST_METHOD0(SignInLinkText, base::string16());
  MOCK_CONST_METHOD0(SpinnerText, base::string16());
  MOCK_CONST_METHOD0(EditSuggestionText, base::string16());
  MOCK_CONST_METHOD0(CancelButtonText, base::string16());
  MOCK_CONST_METHOD0(ConfirmButtonText, base::string16());
  MOCK_CONST_METHOD0(SaveLocallyText, base::string16());
  MOCK_CONST_METHOD0(SaveLocallyTooltip, base::string16());
  MOCK_METHOD0(LegalDocumentsText, base::string16());
  MOCK_CONST_METHOD0(ShouldShowSpinner, bool());
  MOCK_CONST_METHOD0(ShouldShowAccountChooser, bool());
  MOCK_CONST_METHOD0(ShouldShowSignInWebView, bool());
  MOCK_CONST_METHOD0(SignInUrl, GURL());
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
  MOCK_CONST_METHOD1(LabelForSection, base::string16(DialogSection section));
  MOCK_METHOD1(SuggestionStateForSection, SuggestionState(DialogSection));
  MOCK_METHOD1(EditClickedForSection, void(DialogSection section));
  MOCK_METHOD1(EditCancelledForSection, void(DialogSection section));
  MOCK_CONST_METHOD1(IconsForFields, FieldIconMap(const FieldValueMap&));
  MOCK_CONST_METHOD1(FieldControlsIcons, bool(ServerFieldType));
  MOCK_CONST_METHOD1(TooltipForField, base::string16(ServerFieldType));
  MOCK_METHOD2(InputIsEditable, bool(const DetailInput& input,
                                     DialogSection section));
  MOCK_METHOD3(InputValidityMessage,
      base::string16(DialogSection, ServerFieldType, const base::string16&));
  MOCK_METHOD2(InputsAreValid, ValidityMessages(DialogSection,
                                                const FieldValueMap&));
  MOCK_METHOD6(UserEditedOrActivatedInput, void(DialogSection,
                                                ServerFieldType,
                                                gfx::NativeView,
                                                const gfx::Rect&,
                                                const base::string16&,
                                                bool was_edit));
  MOCK_METHOD1(HandleKeyPressEventInInput,
               bool(const content::NativeWebKeyboardEvent& event));
  MOCK_METHOD0(FocusMoved, void());
  MOCK_CONST_METHOD0(ShouldShowErrorBubble, bool());
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

  // Set which web contents initiated showing the dialog.
  void SetWebContents(content::WebContents* contents);

  // Set which profile to use for mock |profile()| calls.
  void SetProfile(Profile* profile);

 private:
  DetailInputs default_inputs_;
  DetailInputs cc_default_inputs_;  // Default inputs for SECTION_CC.
  std::vector<gfx::Range> range_;

  DISALLOW_COPY_AND_ASSIGN(MockAutofillDialogViewDelegate);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_DIALOG_VIEW_DELEGATE_H_
