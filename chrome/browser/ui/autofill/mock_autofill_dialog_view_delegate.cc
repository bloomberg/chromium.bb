// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/mock_autofill_dialog_view_delegate.h"
#include "content/public/browser/native_web_keyboard_event.h"  // For gmock.
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
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
  DefaultValue<base::string16>::Set(base::string16());
  DefaultValue<GURL>::Set(GURL());
  DefaultValue<ValidityMessages>::Set(ValidityMessages());
  DefaultValue<gfx::Image>::Set(gfx::Image());
  DefaultValue<SuggestionState>::Set(SuggestionState(false,
                                                     base::string16(),
                                                     base::string16(),
                                                     gfx::Image(),
                                                     base::string16(),
                                                     gfx::Image()));
  DefaultValue<FieldIconMap>::Set(FieldIconMap());
  DefaultValue<std::vector<DialogNotification> >::Set(
      std::vector<DialogNotification>());

  // SECTION_CC *must* have a CREDIT_CARD_VERIFICATION_CODE field.
  const DetailInput kCreditCardInputs[] = {
    { DetailInput::SHORT,
      CREDIT_CARD_VERIFICATION_CODE,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC) }
  };
  cc_default_inputs_.push_back(kCreditCardInputs[0]);
  ON_CALL(*this, RequestedFieldsForSection(SECTION_CC))
      .WillByDefault(ReturnRef(cc_default_inputs_));
  ON_CALL(*this, RequestedFieldsForSection(SECTION_CC_BILLING))
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

void MockAutofillDialogViewDelegate::SetWebContents(
    content::WebContents* contents) {
  testing::DefaultValue<content::WebContents*>::Set(contents);
}

void MockAutofillDialogViewDelegate::SetProfile(Profile* profile) {
  testing::DefaultValue<Profile*>::Set(profile);
}

MockAutofillDialogViewDelegate::~MockAutofillDialogViewDelegate() {
  using testing::DefaultValue;

  DefaultValue<SuggestionState>::Clear();
  DefaultValue<gfx::Image>::Clear();
  DefaultValue<ValidityMessages>::Clear();
  DefaultValue<base::string16>::Clear();
  DefaultValue<GURL>::Clear();
  DefaultValue<const DetailInputs&>::Clear();
  DefaultValue<FieldIconMap>::Clear();
  DefaultValue<std::vector<DialogNotification> >::Clear();
  DefaultValue<content::WebContents*>::Clear();
  DefaultValue<Profile*>::Clear();
}

}  // namespace autofill
