// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_external_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "grit/component_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillExternalDelegate::AutofillExternalDelegate(
    AutofillManager* manager,
    AutofillDriver* driver)
    : manager_(manager),
      driver_(driver),
      password_manager_(driver),
      query_id_(0),
      display_warning_if_disabled_(false),
      has_suggestion_(false),
      has_shown_popup_for_current_edit_(false),
      weak_ptr_factory_(this) {
  DCHECK(manager);
}

AutofillExternalDelegate::~AutofillExternalDelegate() {}

void AutofillExternalDelegate::OnQuery(int query_id,
                                       const FormData& form,
                                       const FormFieldData& field,
                                       const gfx::RectF& element_bounds,
                                       bool display_warning_if_disabled) {
  query_form_ = form;
  query_field_ = field;
  display_warning_if_disabled_ = display_warning_if_disabled;
  query_id_ = query_id;
  element_bounds_ = element_bounds;
}

void AutofillExternalDelegate::OnSuggestionsReturned(
    int query_id,
    const std::vector<base::string16>& suggested_values,
    const std::vector<base::string16>& suggested_labels,
    const std::vector<base::string16>& suggested_icons,
    const std::vector<int>& suggested_unique_ids) {
  if (query_id != query_id_)
    return;

  std::vector<base::string16> values(suggested_values);
  std::vector<base::string16> labels(suggested_labels);
  std::vector<base::string16> icons(suggested_icons);
  std::vector<int> ids(suggested_unique_ids);

  // Add or hide warnings as appropriate.
  ApplyAutofillWarnings(&values, &labels, &icons, &ids);

  // Add a separator to go between the values and menu items.
  values.push_back(base::string16());
  labels.push_back(base::string16());
  icons.push_back(base::string16());
  ids.push_back(POPUP_ITEM_ID_SEPARATOR);

  // Only include "Autofill Options" special menu item if we have Autofill
  // suggestions.
  has_suggestion_ = false;
  for (size_t i = 0; i < ids.size(); ++i) {
    if (ids[i] > 0) {
      has_suggestion_ = true;
      break;
    }
  }

  if (has_suggestion_)
    ApplyAutofillOptions(&values, &labels, &icons, &ids);

  // Remove the separator if it is the last element.
  DCHECK_GT(ids.size(), 0U);
  if (ids.back() == POPUP_ITEM_ID_SEPARATOR) {
    values.pop_back();
    labels.pop_back();
    icons.pop_back();
    ids.pop_back();
  }

  // If anything else is added to modify the values after inserting the data
  // list, AutofillPopupControllerImpl::UpdateDataListValues will need to be
  // updated to match.
  InsertDataListValues(&values, &labels, &icons, &ids);

  if (values.empty()) {
    // No suggestions, any popup currently showing is obsolete.
    manager_->delegate()->HideAutofillPopup();
    return;
  }

  // Send to display.
  if (query_field_.is_focusable) {
    manager_->delegate()->ShowAutofillPopup(
        element_bounds_,
        query_field_.text_direction,
        values,
        labels,
        icons,
        ids,
        GetWeakPtr());
  }
}

void AutofillExternalDelegate::OnShowPasswordSuggestions(
    const std::vector<base::string16>& suggestions,
    const std::vector<base::string16>& realms,
    const FormFieldData& field,
    const gfx::RectF& element_bounds) {
  query_field_ = field;
  element_bounds_ = element_bounds;

  if (suggestions.empty()) {
    manager_->delegate()->HideAutofillPopup();
    return;
  }

  std::vector<base::string16> empty(suggestions.size());
  std::vector<int> password_ids(suggestions.size(),
                                POPUP_ITEM_ID_PASSWORD_ENTRY);
  manager_->delegate()->ShowAutofillPopup(
      element_bounds_,
      query_field_.text_direction,
      suggestions,
      realms,
      empty,
      password_ids,
      GetWeakPtr());
}

void AutofillExternalDelegate::SetCurrentDataListValues(
    const std::vector<base::string16>& data_list_values,
    const std::vector<base::string16>& data_list_labels) {
  data_list_values_ = data_list_values;
  data_list_labels_ = data_list_labels;

  manager_->delegate()->UpdateAutofillPopupDataListValues(
      data_list_values,
      data_list_labels);
}

void AutofillExternalDelegate::OnPopupShown() {
  manager_->DidShowSuggestions(
      has_suggestion_ && !has_shown_popup_for_current_edit_);
  has_shown_popup_for_current_edit_ |= has_suggestion_;
}

void AutofillExternalDelegate::OnPopupHidden() {
}

void AutofillExternalDelegate::DidSelectSuggestion(
    const base::string16& value,
    int identifier) {
  ClearPreviewedForm();

  // Only preview the data if it is a profile.
  if (identifier > 0)
    FillAutofillFormData(identifier, true);
  else if (identifier == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY)
    driver_->RendererShouldPreviewFieldWithValue(value);
}

void AutofillExternalDelegate::DidAcceptSuggestion(const base::string16& value,
                                                   int identifier) {
  if (identifier == POPUP_ITEM_ID_AUTOFILL_OPTIONS) {
    // User selected 'Autofill Options'.
    manager_->ShowAutofillSettings();
  } else if (identifier == POPUP_ITEM_ID_CLEAR_FORM) {
    // User selected 'Clear form'.
    driver_->RendererShouldClearFilledForm();
  } else if (identifier == POPUP_ITEM_ID_PASSWORD_ENTRY) {
    bool success = password_manager_.DidAcceptAutofillSuggestion(
        query_field_, value);
    DCHECK(success);
  } else if (identifier == POPUP_ITEM_ID_DATALIST_ENTRY) {
    driver_->RendererShouldAcceptDataListSuggestion(value);
  } else if (identifier == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY) {
    // User selected an Autocomplete, so we fill directly.
    driver_->RendererShouldFillFieldWithValue(value);
  } else {
    FillAutofillFormData(identifier, false);
  }

  manager_->delegate()->HideAutofillPopup();
}

void AutofillExternalDelegate::RemoveSuggestion(const base::string16& value,
                                                int identifier) {
  if (identifier > 0)
    manager_->RemoveAutofillProfileOrCreditCard(identifier);
  else
    manager_->RemoveAutocompleteEntry(query_field_.name, value);
}

void AutofillExternalDelegate::DidEndTextFieldEditing() {
  manager_->delegate()->HideAutofillPopup();

  has_shown_popup_for_current_edit_ = false;
}

void AutofillExternalDelegate::ClearPreviewedForm() {
  driver_->RendererShouldClearPreviewedForm();
}

void AutofillExternalDelegate::Reset() {
  manager_->delegate()->HideAutofillPopup();

  password_manager_.Reset();
}

void AutofillExternalDelegate::AddPasswordFormMapping(
      const FormFieldData& username_field,
      const PasswordFormFillData& fill_data) {
  password_manager_.AddPasswordFormMapping(username_field, fill_data);
}

base::WeakPtr<AutofillExternalDelegate> AutofillExternalDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillExternalDelegate::FillAutofillFormData(int unique_id,
                                                    bool is_preview) {
  // If the selected element is a warning we don't want to do anything.
  if (unique_id == POPUP_ITEM_ID_WARNING_MESSAGE)
    return;

  AutofillDriver::RendererFormDataAction renderer_action = is_preview ?
      AutofillDriver::FORM_DATA_ACTION_PREVIEW :
      AutofillDriver::FORM_DATA_ACTION_FILL;

  DCHECK(driver_->RendererIsAvailable());
  // Fill the values for the whole form.
  manager_->FillOrPreviewForm(renderer_action,
                              query_id_,
                              query_form_,
                              query_field_,
                              unique_id);
}

void AutofillExternalDelegate::ApplyAutofillWarnings(
    std::vector<base::string16>* values,
    std::vector<base::string16>* labels,
    std::vector<base::string16>* icons,
    std::vector<int>* unique_ids) {
  if (!query_field_.should_autocomplete) {
    // Autofill is disabled.  If there were some profile or credit card
    // suggestions to show, show a warning instead.  Otherwise, clear out the
    // list of suggestions.
    if (!unique_ids->empty() && (*unique_ids)[0] > 0) {
      // If autofill is disabled and we had suggestions, show a warning instead.
      values->assign(
          1, l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_FORM_DISABLED));
      labels->assign(1, base::string16());
      icons->assign(1, base::string16());
      unique_ids->assign(1, POPUP_ITEM_ID_WARNING_MESSAGE);
    } else {
      values->clear();
      labels->clear();
      icons->clear();
      unique_ids->clear();
    }
  } else if (unique_ids->size() > 1 &&
             (*unique_ids)[0] == POPUP_ITEM_ID_WARNING_MESSAGE) {
    // If we received a warning instead of suggestions from autofill but regular
    // suggestions from autocomplete, don't show the autofill warning.
    values->erase(values->begin());
    labels->erase(labels->begin());
    icons->erase(icons->begin());
    unique_ids->erase(unique_ids->begin());
  }

  // If we were about to show a warning and we shouldn't, don't.
  if (!unique_ids->empty() &&
      (*unique_ids)[0] == POPUP_ITEM_ID_WARNING_MESSAGE &&
      !display_warning_if_disabled_) {
    values->clear();
    labels->clear();
    icons->clear();
    unique_ids->clear();
  }
}

void AutofillExternalDelegate::ApplyAutofillOptions(
    std::vector<base::string16>* values,
    std::vector<base::string16>* labels,
    std::vector<base::string16>* icons,
    std::vector<int>* unique_ids) {
  // The form has been auto-filled, so give the user the chance to clear the
  // form.  Append the 'Clear form' menu item.
  if (query_field_.is_autofilled) {
    values->push_back(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM));
    labels->push_back(base::string16());
    icons->push_back(base::string16());
    unique_ids->push_back(POPUP_ITEM_ID_CLEAR_FORM);
  }

  // Append the 'Chrome Autofill options' menu item;
  values->push_back(l10n_util::GetStringUTF16(IDS_AUTOFILL_OPTIONS_POPUP));
  labels->push_back(base::string16());
  icons->push_back(base::string16());
  unique_ids->push_back(POPUP_ITEM_ID_AUTOFILL_OPTIONS);
}

void AutofillExternalDelegate::InsertDataListValues(
    std::vector<base::string16>* values,
    std::vector<base::string16>* labels,
    std::vector<base::string16>* icons,
    std::vector<int>* unique_ids) {
  if (data_list_values_.empty())
    return;

  // Insert the separator between the datalist and Autofill values (if there
  // are any).
  if (!values->empty()) {
    values->insert(values->begin(), base::string16());
    labels->insert(labels->begin(), base::string16());
    icons->insert(icons->begin(), base::string16());
    unique_ids->insert(unique_ids->begin(), POPUP_ITEM_ID_SEPARATOR);
  }

  // Insert the datalist elements.
  values->insert(values->begin(),
                 data_list_values_.begin(),
                 data_list_values_.end());
  labels->insert(labels->begin(),
                 data_list_labels_.begin(),
                 data_list_labels_.end());

  // Set the values that all datalist elements share.
  icons->insert(icons->begin(),
                data_list_values_.size(),
                base::string16());
  unique_ids->insert(unique_ids->begin(),
                     data_list_values_.size(),
                     POPUP_ITEM_ID_DATALIST_ENTRY);
}

}  // namespace autofill
