// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/mock_autofill_dialog_view_delegate.h"
#include "content/public/browser/native_web_keyboard_event.h"  // For gmock.
#include "grit/generated_resources.h"
#include "ui/gfx/rect.h"  // Only needed because gmock needs complete types.

namespace autofill {

MockAutofillDialogViewDelegate::MockAutofillDialogViewDelegate() {
  using testing::DefaultValue;
  using testing::_;
  using testing::Return;
  using testing::ReturnRef;

  // N.B. Setting DefaultValue in the ctor and deleting it in the dtor will
  // only work if this Mock is not used together with other mock code that
  // sets different defaults. If tests utilizing the MockController start
  // breaking because of this, use ON_CALL instead.
  DefaultValue<const DetailInputs&>::Set(default_inputs_);
  DefaultValue<string16>::Set(string16());
  DefaultValue<ValidityData>::Set(ValidityData());
  DefaultValue<gfx::Image>::Set(gfx::Image());
  DefaultValue<SuggestionState>::Set(SuggestionState(false,
                                                     string16(),
                                                     string16(),
                                                     gfx::Image(),
                                                     string16(),
                                                     gfx::Image()));

  // SECTION_CC *must* have a CREDIT_CARD_VERIFICATION_CODE field.
  const DetailInput kCreditCardInputs[] = {
    { 2, CREDIT_CARD_VERIFICATION_CODE, IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC }
  };
  cc_default_inputs_.push_back(kCreditCardInputs[0]);
  ON_CALL(*this, RequestedFieldsForSection(SECTION_CC))
      .WillByDefault(ReturnRef(cc_default_inputs_));

  ON_CALL(*this, GetDialogButtons())
      .WillByDefault(Return(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL));
  ON_CALL(*this, LegalDocumentLinks()).WillByDefault(ReturnRef(range_));

  // Activate all sections but CC_BILLING - default for the real
  // controller implementation, too.
  ON_CALL(*this, SectionIsActive(_)).WillByDefault(Return(true));
  ON_CALL(*this, SectionIsActive(SECTION_CC_BILLING))
      .WillByDefault(Return(false));
}

MockAutofillDialogViewDelegate::~MockAutofillDialogViewDelegate() {
  using testing::DefaultValue;

  DefaultValue<SuggestionState>::Clear();
  DefaultValue<gfx::Image>::Clear();
  DefaultValue<ValidityData>::Clear();
  DefaultValue<string16>::Clear();
  DefaultValue<const DetailInputs&>::Clear();
}

}  // namespace autofill
