// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/contact_info_editor_view_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"

namespace payments {

ContactInfoEditorViewController::ContactInfoEditorViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog,
    autofill::AutofillProfile* profile)
    : EditorViewController(spec, state, dialog), profile_to_edit_(profile) {}

ContactInfoEditorViewController::~ContactInfoEditorViewController() {}

std::unique_ptr<views::View>
ContactInfoEditorViewController::CreateHeaderView() {
  return base::MakeUnique<views::View>();
}

std::vector<EditorField>
ContactInfoEditorViewController::GetFieldDefinitions() {
  std::vector<EditorField> fields;
  if (spec()->request_payer_name()) {
    fields.push_back(EditorField(
        autofill::NAME_FULL,
        l10n_util::GetStringUTF16(IDS_PAYMENTS_NAME_FIELD_IN_CONTACT_DETAILS),
        EditorField::LengthHint::HINT_LONG, /*required=*/true));
  }
  if (spec()->request_payer_phone()) {
    fields.push_back(EditorField(
        autofill::PHONE_HOME_WHOLE_NUMBER,
        l10n_util::GetStringUTF16(IDS_PAYMENTS_PHONE_FIELD_IN_CONTACT_DETAILS),
        EditorField::LengthHint::HINT_LONG, /*required=*/true));
  }
  if (spec()->request_payer_email()) {
    fields.push_back(EditorField(
        autofill::EMAIL_ADDRESS,
        l10n_util::GetStringUTF16(IDS_PAYMENTS_EMAIL_FIELD_IN_CONTACT_DETAILS),
        EditorField::LengthHint::HINT_LONG, /*required=*/true));
  }
  return fields;
}

base::string16 ContactInfoEditorViewController::GetInitialValueForType(
    autofill::ServerFieldType type) {
  if (!profile_to_edit_)
    return base::string16();

  return profile_to_edit_->GetInfo(autofill::AutofillType(type),
                                   state()->GetApplicationLocale());
}

bool ContactInfoEditorViewController::ValidateModelAndSave() {
  // TODO(crbug.com/712224): Move this method and its helpers to a base class
  // shared with the Shipping Address editor.
  if (!ValidateModel())
    return false;

  if (profile_to_edit_) {
    PopulateProfile(profile_to_edit_);
    state()->GetPersonalDataManager()->UpdateProfile(*profile_to_edit_);
    state()->profile_comparator()->Invalidate(*profile_to_edit_);
  } else {
    std::unique_ptr<autofill::AutofillProfile> profile =
        base::MakeUnique<autofill::AutofillProfile>();
    PopulateProfile(profile.get());
    state()->GetPersonalDataManager()->AddProfile(*profile);
    // TODO(crbug.com/712224): Add to profile cache in state_.
  }
  return true;
}

std::unique_ptr<ValidationDelegate>
ContactInfoEditorViewController::CreateValidationDelegate(
    const EditorField& field) {
  return base::MakeUnique<ContactInfoValidationDelegate>(
      field, state()->GetApplicationLocale(), this);
}

std::unique_ptr<ui::ComboboxModel>
ContactInfoEditorViewController::GetComboboxModelForType(
    const autofill::ServerFieldType& type) {
  NOTREACHED();
  return nullptr;
}

base::string16 ContactInfoEditorViewController::GetSheetTitle() {
  // TODO(crbug.com/712074): Title should reflect the missing information, if
  // applicable.
  return profile_to_edit_ ? l10n_util::GetStringUTF16(
                                IDS_PAYMENTS_EDIT_CONTACT_DETAILS_LABEL)
                          : l10n_util::GetStringUTF16(
                                IDS_PAYMENTS_ADD_CONTACT_DETAILS_LABEL);
}

bool ContactInfoEditorViewController::ValidateModel() {
  for (const auto& field : text_fields()) {
    // Force a blur, as validation only occurs after the first blur.
    field.first->OnBlur();
    if (field.first->invalid())
      return false;
  }
  return true;
}

void ContactInfoEditorViewController::PopulateProfile(
    autofill::AutofillProfile* profile) {
  for (const auto& field : text_fields()) {
    profile->SetInfo(autofill::AutofillType(field.second.type),
                     field.first->text(), state()->GetApplicationLocale());
  }
  profile->set_origin(autofill::kSettingsOrigin);
}

ContactInfoEditorViewController::ContactInfoValidationDelegate::
    ContactInfoValidationDelegate(const EditorField& field,
                                  const std::string& locale,
                                  EditorViewController* controller)
    : field_(field), controller_(controller), locale_(locale) {}

ContactInfoEditorViewController::ContactInfoValidationDelegate::
    ~ContactInfoValidationDelegate() {}

bool ContactInfoEditorViewController::ContactInfoValidationDelegate::
    ValidateTextfield(views::Textfield* textfield) {
  bool is_valid = true;
  base::string16 error_message;

  if (textfield->text().empty()) {
    is_valid = false;
    error_message = l10n_util::GetStringUTF16(
        IDS_PAYMENTS_FIELD_REQUIRED_VALIDATION_MESSAGE);
  } else {
    switch (field_.type) {
      case autofill::PHONE_HOME_WHOLE_NUMBER: {
        const std::string default_region_code =
            autofill::AutofillCountry::CountryCodeForLocale(locale_);
        if (!autofill::IsValidPhoneNumber(textfield->text(),
                                          default_region_code)) {
          is_valid = false;
          error_message = l10n_util::GetStringUTF16(
              IDS_PAYMENTS_PHONE_INVALID_VALIDATION_MESSAGE);
        }
        break;
      }

      case autofill::EMAIL_ADDRESS: {
        if (!autofill::IsValidEmailAddress(textfield->text())) {
          is_valid = false;
          error_message = l10n_util::GetStringUTF16(
              IDS_PAYMENTS_EMAIL_INVALID_VALIDATION_MESSAGE);
        }
        break;
      }

      case autofill::NAME_FULL: {
        // We have already determined that name is nonempty, which is the only
        // requirement.
        break;
      }

      default: {
        NOTREACHED();
        break;
      }
    }
  }

  controller_->DisplayErrorMessageForField(field_, error_message);
  return is_valid;
}

bool ContactInfoEditorViewController::ContactInfoValidationDelegate::
    ValidateCombobox(views::Combobox* combobox) {
  // This UI doesn't contain any comboboxes.
  NOTREACHED();
  return true;
}

}  // namespace payments
