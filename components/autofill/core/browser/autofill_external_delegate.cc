// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_external_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "grit/component_strings.h"
#include "third_party/WebKit/public/web/WebAutofillClient.h"
#include "ui/base/l10n/l10n_util.h"

using blink::WebAutofillClient;

namespace autofill {

AutofillExternalDelegate::AutofillExternalDelegate(
    AutofillManager* autofill_manager,
    AutofillDriver* autofill_driver)
    : autofill_manager_(autofill_manager),
      autofill_driver_(autofill_driver),
      password_autofill_manager_(autofill_driver),
      autofill_query_id_(0),
      display_warning_if_disabled_(false),
      has_autofill_suggestion_(false),
      has_shown_autofill_popup_for_current_edit_(false),
      weak_ptr_factory_(this) {
  DCHECK(autofill_manager);
}

AutofillExternalDelegate::~AutofillExternalDelegate() {}

void AutofillExternalDelegate::OnQuery(int query_id,
                                       const FormData& form,
                                       const FormFieldData& field,
                                       const gfx::RectF& element_bounds,
                                       bool display_warning_if_disabled) {
  autofill_query_form_ = form;
  autofill_query_field_ = field;
  display_warning_if_disabled_ = display_warning_if_disabled;
  autofill_query_id_ = query_id;
  element_bounds_ = element_bounds;
}

void AutofillExternalDelegate::OnSuggestionsReturned(
    int query_id,
    const std::vector<base::string16>& autofill_values,
    const std::vector<base::string16>& autofill_labels,
    const std::vector<base::string16>& autofill_icons,
    const std::vector<int>& autofill_unique_ids) {
  if (query_id != autofill_query_id_)
    return;

  std::vector<base::string16> values(autofill_values);
  std::vector<base::string16> labels(autofill_labels);
  std::vector<base::string16> icons(autofill_icons);
  std::vector<int> ids(autofill_unique_ids);

  // Add or hide warnings as appropriate.
  ApplyAutofillWarnings(&values, &labels, &icons, &ids);

  // Add a separator to go between the values and menu items.
  values.push_back(base::string16());
  labels.push_back(base::string16());
  icons.push_back(base::string16());
  ids.push_back(WebAutofillClient::MenuItemIDSeparator);

  // Only include "Autofill Options" special menu item if we have Autofill
  // suggestions.
  has_autofill_suggestion_ = false;
  for (size_t i = 0; i < ids.size(); ++i) {
    if (ids[i] > 0) {
      has_autofill_suggestion_ = true;
      break;
    }
  }

  if (has_autofill_suggestion_)
    ApplyAutofillOptions(&values, &labels, &icons, &ids);

  // Remove the separator if it is the last element.
  DCHECK_GT(ids.size(), 0U);
  if (ids.back() == WebAutofillClient::MenuItemIDSeparator) {
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
    autofill_manager_->delegate()->HideAutofillPopup();
    return;
  }

  // Send to display.
  if (autofill_query_field_.is_focusable) {
    autofill_manager_->delegate()->ShowAutofillPopup(
        element_bounds_,
        autofill_query_field_.text_direction,
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
  autofill_query_field_ = field;
  element_bounds_ = element_bounds;

  if (suggestions.empty()) {
    autofill_manager_->delegate()->HideAutofillPopup();
    return;
  }

  std::vector<base::string16> empty(suggestions.size());
  std::vector<int> password_ids(suggestions.size(),
                                WebAutofillClient::MenuItemIDPasswordEntry);
  autofill_manager_->delegate()->ShowAutofillPopup(
      element_bounds_,
      autofill_query_field_.text_direction,
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

  autofill_manager_->delegate()->UpdateAutofillPopupDataListValues(
      data_list_values,
      data_list_labels);
}

void AutofillExternalDelegate::OnPopupShown() {
  autofill_manager_->OnDidShowAutofillSuggestions(
      has_autofill_suggestion_ && !has_shown_autofill_popup_for_current_edit_);
  has_shown_autofill_popup_for_current_edit_ |= has_autofill_suggestion_;
}

void AutofillExternalDelegate::OnPopupHidden() {
}

bool AutofillExternalDelegate::ShouldRepostEvent(const ui::MouseEvent& event) {
  NOTREACHED();
  return true;
}

void AutofillExternalDelegate::DidSelectSuggestion(int identifier) {
  ClearPreviewedForm();

  // Only preview the data if it is a profile.
  if (identifier > 0)
    FillAutofillFormData(identifier, true);
}

void AutofillExternalDelegate::DidAcceptSuggestion(const base::string16& value,
                                                   int identifier) {
  if (identifier == WebAutofillClient::MenuItemIDAutofillOptions) {
    // User selected 'Autofill Options'.
    autofill_manager_->ShowAutofillSettings();
  } else if (identifier == WebAutofillClient::MenuItemIDClearForm) {
    // User selected 'Clear form'.
    autofill_driver_->RendererShouldClearFilledForm();
  } else if (identifier == WebAutofillClient::MenuItemIDPasswordEntry) {
    bool success = password_autofill_manager_.DidAcceptAutofillSuggestion(
        autofill_query_field_, value);
    DCHECK(success);
  } else if (identifier == WebAutofillClient::MenuItemIDDataListEntry) {
    autofill_driver_->RendererShouldAcceptDataListSuggestion(value);
  } else if (identifier == WebAutofillClient::MenuItemIDAutocompleteEntry) {
    // User selected an Autocomplete, so we fill directly.
    autofill_driver_->RendererShouldSetNodeText(value);
  } else {
    FillAutofillFormData(identifier, false);
  }

  autofill_manager_->delegate()->HideAutofillPopup();
}

void AutofillExternalDelegate::RemoveSuggestion(const base::string16& value,
                                                int identifier) {
  if (identifier > 0) {
    autofill_manager_->RemoveAutofillProfileOrCreditCard(identifier);
  } else {
    autofill_manager_->RemoveAutocompleteEntry(autofill_query_field_.name,
                                               value);
  }
}

void AutofillExternalDelegate::DidEndTextFieldEditing() {
  autofill_manager_->delegate()->HideAutofillPopup();

  has_shown_autofill_popup_for_current_edit_ = false;
}

void AutofillExternalDelegate::ClearPreviewedForm() {
  autofill_driver_->RendererShouldClearPreviewedForm();
}

void AutofillExternalDelegate::Reset() {
  autofill_manager_->delegate()->HideAutofillPopup();

  password_autofill_manager_.Reset();
}

void AutofillExternalDelegate::AddPasswordFormMapping(
      const FormFieldData& username_field,
      const PasswordFormFillData& fill_data) {
  password_autofill_manager_.AddPasswordFormMapping(username_field, fill_data);
}

base::WeakPtr<AutofillExternalDelegate> AutofillExternalDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillExternalDelegate::FillAutofillFormData(int unique_id,
                                                    bool is_preview) {
  // If the selected element is a warning we don't want to do anything.
  if (unique_id == WebAutofillClient::MenuItemIDWarningMessage)
    return;

  AutofillDriver::RendererFormDataAction renderer_action = is_preview ?
      AutofillDriver::FORM_DATA_ACTION_PREVIEW :
      AutofillDriver::FORM_DATA_ACTION_FILL;

  DCHECK(autofill_driver_->RendererIsAvailable());
  autofill_driver_->SetRendererActionOnFormDataReception(renderer_action);
  // Fill the values for the whole form.
  autofill_manager_->OnFillAutofillFormData(autofill_query_id_,
                                            autofill_query_form_,
                                            autofill_query_field_,
                                            unique_id);
}

void AutofillExternalDelegate::ApplyAutofillWarnings(
    std::vector<base::string16>* autofill_values,
    std::vector<base::string16>* autofill_labels,
    std::vector<base::string16>* autofill_icons,
    std::vector<int>* autofill_unique_ids) {
  if (!autofill_query_field_.should_autocomplete) {
    // Autofill is disabled.  If there were some profile or credit card
    // suggestions to show, show a warning instead.  Otherwise, clear out the
    // list of suggestions.
    if (!autofill_unique_ids->empty() && (*autofill_unique_ids)[0] > 0) {
      // If autofill is disabled and we had suggestions, show a warning instead.
      autofill_values->assign(
          1, l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_FORM_DISABLED));
      autofill_labels->assign(1, base::string16());
      autofill_icons->assign(1, base::string16());
      autofill_unique_ids->assign(1,
                                  WebAutofillClient::MenuItemIDWarningMessage);
    } else {
      autofill_values->clear();
      autofill_labels->clear();
      autofill_icons->clear();
      autofill_unique_ids->clear();
    }
  } else if (autofill_unique_ids->size() > 1 &&
             (*autofill_unique_ids)[0] ==
                 WebAutofillClient::MenuItemIDWarningMessage) {
    // If we received a warning instead of suggestions from autofill but regular
    // suggestions from autocomplete, don't show the autofill warning.
    autofill_values->erase(autofill_values->begin());
    autofill_labels->erase(autofill_labels->begin());
    autofill_icons->erase(autofill_icons->begin());
    autofill_unique_ids->erase(autofill_unique_ids->begin());
  }

  // If we were about to show a warning and we shouldn't, don't.
  if (!autofill_unique_ids->empty() &&
      (*autofill_unique_ids)[0] ==
          WebAutofillClient::MenuItemIDWarningMessage &&
      !display_warning_if_disabled_) {
    autofill_values->clear();
    autofill_labels->clear();
    autofill_icons->clear();
    autofill_unique_ids->clear();
  }
}

void AutofillExternalDelegate::ApplyAutofillOptions(
    std::vector<base::string16>* autofill_values,
    std::vector<base::string16>* autofill_labels,
    std::vector<base::string16>* autofill_icons,
    std::vector<int>* autofill_unique_ids) {
  // The form has been auto-filled, so give the user the chance to clear the
  // form.  Append the 'Clear form' menu item.
  if (autofill_query_field_.is_autofilled) {
    autofill_values->push_back(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM));
    autofill_labels->push_back(base::string16());
    autofill_icons->push_back(base::string16());
    autofill_unique_ids->push_back(WebAutofillClient::MenuItemIDClearForm);
  }

  // Append the 'Chrome Autofill options' menu item;
  autofill_values->push_back(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_OPTIONS_POPUP));
  autofill_labels->push_back(base::string16());
  autofill_icons->push_back(base::string16());
  autofill_unique_ids->push_back(WebAutofillClient::MenuItemIDAutofillOptions);
}

void AutofillExternalDelegate::InsertDataListValues(
    std::vector<base::string16>* autofill_values,
    std::vector<base::string16>* autofill_labels,
    std::vector<base::string16>* autofill_icons,
    std::vector<int>* autofill_unique_ids) {
  if (data_list_values_.empty())
    return;

  // Insert the separator between the datalist and Autofill values (if there
  // are any).
  if (!autofill_values->empty()) {
    autofill_values->insert(autofill_values->begin(), base::string16());
    autofill_labels->insert(autofill_labels->begin(), base::string16());
    autofill_icons->insert(autofill_icons->begin(), base::string16());
    autofill_unique_ids->insert(autofill_unique_ids->begin(),
                                WebAutofillClient::MenuItemIDSeparator);
  }

  // Insert the datalist elements.
  autofill_values->insert(autofill_values->begin(),
                          data_list_values_.begin(),
                          data_list_values_.end());
  autofill_labels->insert(autofill_labels->begin(),
                          data_list_labels_.begin(),
                          data_list_labels_.end());

  // Set the values that all datalist elements share.
  autofill_icons->insert(autofill_icons->begin(),
                         data_list_values_.size(),
                         base::string16());
  autofill_unique_ids->insert(autofill_unique_ids->begin(),
                              data_list_values_.size(),
                              WebAutofillClient::MenuItemIDDataListEntry);
}

}  // namespace autofill
