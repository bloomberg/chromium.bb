// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/shipping_address_editor_view_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/validating_combobox.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_combobox_model.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/region_combobox_model.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/messages.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/textfield/textfield.h"

namespace payments {

namespace {

// Converts a field type in string format as returned by
// autofill::GetAddressComponents into the appropriate autofill::ServerFieldType
// enum.
autofill::ServerFieldType GetFieldTypeFromString(const std::string& type) {
  if (type == autofill::kFullNameField)
    return autofill::NAME_FULL;
  if (type == autofill::kCompanyNameField)
    return autofill::COMPANY_NAME;
  if (type == autofill::kAddressLineField)
    return autofill::ADDRESS_HOME_STREET_ADDRESS;
  if (type == autofill::kDependentLocalityField)
    return autofill::ADDRESS_HOME_DEPENDENT_LOCALITY;
  if (type == autofill::kCityField)
    return autofill::ADDRESS_HOME_CITY;
  if (type == autofill::kStateField)
    return autofill::ADDRESS_HOME_STATE;
  if (type == autofill::kPostalCodeField)
    return autofill::ADDRESS_HOME_ZIP;
  if (type == autofill::kSortingCodeField)
    return autofill::ADDRESS_HOME_SORTING_CODE;
  if (type == autofill::kCountryField)
    return autofill::ADDRESS_HOME_COUNTRY;
  NOTREACHED();
  return autofill::UNKNOWN_TYPE;
}

}  // namespace

ShippingAddressEditorViewController::ShippingAddressEditorViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog,
    BackNavigationType back_navigation_type,
    base::OnceClosure on_edited,
    base::OnceCallback<void(const autofill::AutofillProfile&)> on_added,
    autofill::AutofillProfile* profile)
    : EditorViewController(spec, state, dialog, back_navigation_type),
      on_edited_(std::move(on_edited)),
      on_added_(std::move(on_added)),
      profile_to_edit_(profile),
      chosen_country_index_(0),
      failed_to_load_region_data_(false) {
  UpdateEditorFields();
}

ShippingAddressEditorViewController::~ShippingAddressEditorViewController() {}

std::vector<EditorField>
ShippingAddressEditorViewController::GetFieldDefinitions() {
  return editor_fields_;
}

base::string16 ShippingAddressEditorViewController::GetInitialValueForType(
    autofill::ServerFieldType type) {
  // Temporary profile has precedence over profile to edit since its existence
  // is based on having unsaved stated to restore.
  if (temporary_profile_.get())
    return temporary_profile_->GetInfo(autofill::AutofillType(type),
                                       state()->GetApplicationLocale());

  if (!profile_to_edit_)
    return base::string16();

  return profile_to_edit_->GetInfo(autofill::AutofillType(type),
                                   state()->GetApplicationLocale());
}

bool ShippingAddressEditorViewController::ValidateModelAndSave() {
  // To validate the profile first, we use a temporary object.
  autofill::AutofillProfile profile;
  if (!SaveFieldsToProfile(&profile, /*ignore_errors=*/false))
    return false;
  if (!profile_to_edit_) {
    // Add the profile (will not add a duplicate).
    profile.set_origin(autofill::kSettingsOrigin);
    state()->GetPersonalDataManager()->AddProfile(profile);
    std::move(on_added_).Run(profile);
    on_edited_.Reset();
  } else {
    // Copy the temporary object's data to the object to be edited. Prefer this
    // method to copying |profile| into |profile_to_edit_|, because the latter
    // object needs to retain other properties (use count, use date, guid,
    // etc.).
    bool success = SaveFieldsToProfile(profile_to_edit_,
                                       /*ignore_errors=*/false);
    DCHECK(success);
    profile_to_edit_->set_origin(autofill::kSettingsOrigin);
    state()->GetPersonalDataManager()->UpdateProfile(*profile_to_edit_);
    state()->profile_comparator()->Invalidate(*profile_to_edit_);
    std::move(on_edited_).Run();
    on_added_.Reset();
  }

  return true;
}

std::unique_ptr<ValidationDelegate>
ShippingAddressEditorViewController::CreateValidationDelegate(
    const EditorField& field) {
  return base::MakeUnique<
      ShippingAddressEditorViewController::ShippingAddressValidationDelegate>(
      this, field);
}

std::unique_ptr<ui::ComboboxModel>
ShippingAddressEditorViewController::GetComboboxModelForType(
    const autofill::ServerFieldType& type) {
  switch (type) {
    case autofill::ADDRESS_HOME_COUNTRY: {
      std::unique_ptr<autofill::CountryComboboxModel> model =
          base::MakeUnique<autofill::CountryComboboxModel>();
      model->SetCountries(*state()->GetPersonalDataManager(),
                          base::Callback<bool(const std::string&)>(),
                          state()->GetApplicationLocale());
      country_codes_.clear();
      for (size_t i = 0; i < model->countries().size(); ++i) {
        if (model->countries()[i].get())
          country_codes_.push_back(model->countries()[i]->country_code());
        else
          country_codes_.push_back("");  // Separator.
      }
      return std::move(model);
    }
    case autofill::ADDRESS_HOME_STATE: {
      std::unique_ptr<autofill::RegionComboboxModel> model =
          base::MakeUnique<autofill::RegionComboboxModel>();
      model->LoadRegionData(country_codes_[chosen_country_index_],
                            state()->GetRegionDataLoader(),
                            /*timeout_ms=*/5000);
      if (!model->IsPendingRegionDataLoad()) {
        // If the data was already pre-loaded, the observer won't get notified
        // so we have to check for failure here.
        failed_to_load_region_data_ = model->failed_to_load_data();
        if (failed_to_load_region_data_) {
          // We can update the view synchronously while building the view.
          OnDataChanged(/*synchronous=*/false);
        }
      }
      return std::move(model);
    }
    default:
      NOTREACHED();
      break;
  }
  return std::unique_ptr<ui::ComboboxModel>();
}

void ShippingAddressEditorViewController::OnPerformAction(
    views::Combobox* sender) {
  EditorViewController::OnPerformAction(sender);
  if (sender->id() != autofill::ADDRESS_HOME_COUNTRY)
    return;
  DCHECK_GE(sender->selected_index(), 0);
  if (chosen_country_index_ != static_cast<size_t>(sender->selected_index())) {
    chosen_country_index_ = sender->selected_index();
    failed_to_load_region_data_ = false;
    // View update must be asynchronous to let the combobox finish performing
    // the action.
    OnDataChanged(/*synchronous=*/false);
  }
}

void ShippingAddressEditorViewController::UpdateEditorView() {
  EditorViewController::UpdateEditorView();
  if (chosen_country_index_ > 0UL) {
    views::Combobox* country_combo_box = static_cast<views::Combobox*>(
        dialog()->GetViewByID(autofill::ADDRESS_HOME_COUNTRY));
    DCHECK(country_combo_box);
    country_combo_box->SetSelectedIndex(chosen_country_index_);
  }
  // Ignore temporary profile once the editor view has been updated.
  temporary_profile_.reset(nullptr);
}

base::string16 ShippingAddressEditorViewController::GetSheetTitle() {
  // TODO(crbug.com/712074): Editor title should reflect the missing information
  // in the case that one or more fields are missing.
  return profile_to_edit_ ? l10n_util::GetStringUTF16(IDS_PAYMENTS_EDIT_ADDRESS)
                          : l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_ADDRESS);
}

std::unique_ptr<views::Button>
ShippingAddressEditorViewController::CreatePrimaryButton() {
  std::unique_ptr<views::Button> button(
      EditorViewController::CreatePrimaryButton());
  button->set_id(static_cast<int>(DialogViewID::SAVE_ADDRESS_BUTTON));
  return button;
}

void ShippingAddressEditorViewController::UpdateEditorFields() {
  editor_fields_.clear();
  std::string chosen_country_code;
  if (chosen_country_index_ < country_codes_.size())
    chosen_country_code = country_codes_[chosen_country_index_];

  std::unique_ptr<base::ListValue> components(new base::ListValue);
  std::string unused;
  autofill::GetAddressComponents(chosen_country_code,
                                 state()->GetApplicationLocale(),
                                 components.get(), &unused);
  for (size_t line_index = 0; line_index < components->GetSize();
       ++line_index) {
    const base::ListValue* line = nullptr;
    if (!components->GetList(line_index, &line)) {
      NOTREACHED();
      return;
    }
    DCHECK_NE(nullptr, line);
    for (size_t component_index = 0; component_index < line->GetSize();
         ++component_index) {
      const base::DictionaryValue* component = nullptr;
      if (!line->GetDictionary(component_index, &component)) {
        NOTREACHED();
        return;
      }
      std::string field_type;
      if (!component->GetString(autofill::kFieldTypeKey, &field_type)) {
        NOTREACHED();
        return;
      }
      std::string field_name;
      if (!component->GetString(autofill::kFieldNameKey, &field_name)) {
        NOTREACHED();
        return;
      }
      std::string field_length;
      if (!component->GetString(autofill::kFieldLengthKey, &field_length)) {
        NOTREACHED();
        return;
      }
      EditorField::LengthHint length_hint = EditorField::LengthHint::HINT_SHORT;
      if (field_length == autofill::kLongField)
        length_hint = EditorField::LengthHint::HINT_LONG;
      else
        DCHECK_EQ(autofill::kShortField, field_length);
      autofill::ServerFieldType server_field_type =
          GetFieldTypeFromString(field_type);
      EditorField::ControlType control_type =
          EditorField::ControlType::TEXTFIELD;
      if (server_field_type == autofill::ADDRESS_HOME_COUNTRY ||
          (server_field_type == autofill::ADDRESS_HOME_STATE &&
           !failed_to_load_region_data_)) {
        control_type = EditorField::ControlType::COMBOBOX;
      }
      editor_fields_.emplace_back(
          server_field_type, base::UTF8ToUTF16(field_name), length_hint,
          /*required=*/server_field_type != autofill::COMPANY_NAME,
          control_type);
      // Insert the Country combobox right after NAME_FULL.
      if (server_field_type == autofill::NAME_FULL) {
        editor_fields_.emplace_back(
            autofill::ADDRESS_HOME_COUNTRY,
            l10n_util::GetStringUTF16(
                IDS_LIBADDRESSINPUT_COUNTRY_OR_REGION_LABEL),
            EditorField::LengthHint::HINT_SHORT, /*required=*/true,
            EditorField::ControlType::COMBOBOX);
      }
    }
  }
  // Always add phone number at the end.
  editor_fields_.emplace_back(
      autofill::PHONE_HOME_WHOLE_NUMBER,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_PHONE),
      EditorField::LengthHint::HINT_SHORT, /*required=*/true,
      EditorField::ControlType::TEXTFIELD);
}

void ShippingAddressEditorViewController::OnDataChanged(bool synchronous) {
  temporary_profile_.reset(new autofill::AutofillProfile);
  SaveFieldsToProfile(temporary_profile_.get(), /*ignore_errors*/ true);

  UpdateEditorFields();
  if (synchronous) {
    UpdateEditorView();
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&ShippingAddressEditorViewController::UpdateEditorView,
                       base::Unretained(this)));
  }
}

bool ShippingAddressEditorViewController::SaveFieldsToProfile(
    autofill::AutofillProfile* profile,
    bool ignore_errors) {
  const std::string& locale = state()->GetApplicationLocale();
  bool success = true;
  for (const auto& field : text_fields()) {
    // Force a blur in case the value was left untouched.
    field.first->OnBlur();
    // ValidatingTextfield* is the key, EditorField is the value.
    if (field.first->invalid()) {
      success = false;
    } else {
      success = profile->SetInfo(autofill::AutofillType(field.second.type),
                                 field.first->text(), locale);
    }
    LOG_IF(ERROR, success || ignore_errors)
        << "Can't setinfo(" << field.second.type << ", " << field.first->text();
    if (!success && !ignore_errors)
      return false;
  }
  for (const auto& field : comboboxes()) {
    // ValidatingCombobox* is the key, EditorField is the value.
    ValidatingCombobox* combobox = field.first;
    if (combobox->invalid()) {
      success = false;
    } else {
      if (combobox->id() == autofill::ADDRESS_HOME_COUNTRY) {
        success = profile->SetInfo(
            autofill::AutofillType(field.second.type),
            base::UTF8ToUTF16(country_codes_[combobox->selected_index()]),
            locale);
      } else {
        success = profile->SetInfo(
            autofill::AutofillType(field.second.type),
            combobox->GetTextForRow(combobox->selected_index()), locale);
      }
    }
    LOG_IF(ERROR, success || ignore_errors)
        << "Can't setinfo(" << field.second.type << ", "
        << combobox->GetTextForRow(combobox->selected_index());
    if (!success && !ignore_errors)
      return false;
  }
  return success;
}

void ShippingAddressEditorViewController::OnComboboxModelChanged(
    views::Combobox* combobox) {
  if (combobox->id() != autofill::ADDRESS_HOME_STATE)
    return;
  autofill::RegionComboboxModel* model =
      static_cast<autofill::RegionComboboxModel*>(combobox->model());
  if (model->IsPendingRegionDataLoad())
    return;
  if (model->failed_to_load_data()) {
    failed_to_load_region_data_ = true;
    // It is safe to update synchronously since the change comes from the model
    // and not from the UI.
    OnDataChanged(/*synchronous=*/true);
  }
}

bool ShippingAddressEditorViewController::GetSheetId(DialogViewID* sheet_id) {
  *sheet_id = DialogViewID::SHIPPING_ADDRESS_EDITOR_SHEET;
  return true;
}

ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    ShippingAddressValidationDelegate(
        ShippingAddressEditorViewController* controller,
        const EditorField& field)
    : field_(field), controller_(controller) {}

ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    ~ShippingAddressValidationDelegate() {}

bool ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    ValidateTextfield(views::Textfield* textfield) {
  return ValidateValue(textfield->text());
}

bool ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    ValidateCombobox(views::Combobox* combobox) {
  return ValidateValue(combobox->GetTextForRow(combobox->selected_index()));
}

void ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    ComboboxModelChanged(views::Combobox* combobox) {
  controller_->OnComboboxModelChanged(combobox);
}

bool ShippingAddressEditorViewController::ShippingAddressValidationDelegate::
    ValidateValue(const base::string16& value) {
  if (!value.empty()) {
    if (field_.type == autofill::PHONE_HOME_WHOLE_NUMBER &&
        !autofill::IsValidPhoneNumber(
            value,
            controller_->country_codes_[controller_->chosen_country_index_])) {
      controller_->DisplayErrorMessageForField(
          field_, l10n_util::GetStringUTF16(
                      IDS_PAYMENTS_PHONE_INVALID_VALIDATION_MESSAGE));
      return false;
    }
    // As long as other field types are non-empty, they are valid.
    controller_->DisplayErrorMessageForField(field_, base::ASCIIToUTF16(""));
    return true;
  }
  bool is_required_valid = !field_.required;
  const base::string16 displayed_message =
      is_required_valid ? base::ASCIIToUTF16("")
                        : l10n_util::GetStringUTF16(
                              IDS_PAYMENTS_FIELD_REQUIRED_VALIDATION_MESSAGE);
  controller_->DisplayErrorMessageForField(field_, displayed_message);
  return is_required_valid;
}

}  // namespace payments
