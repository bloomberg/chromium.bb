// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_manager.h"

#include <stddef.h>

#include <limits>
#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_manager_test_delegate.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

namespace autofill {

typedef PersonalDataManager::GUIDPair GUIDPair;

using base::TimeTicks;

namespace {

// We only send a fraction of the forms to upload server.
// The rate for positive/negative matches potentially could be different.
const double kAutofillPositiveUploadRateDefaultValue = 0.20;
const double kAutofillNegativeUploadRateDefaultValue = 0.20;

const size_t kMaxRecentFormSignaturesToRemember = 3;

// Set a conservative upper bound on the number of forms we are willing to
// cache, simply to prevent unbounded memory consumption.
const size_t kMaxFormCacheSize = 100;

// Removes duplicate suggestions whilst preserving their original order.
void RemoveDuplicateSuggestions(std::vector<base::string16>* values,
                                std::vector<base::string16>* labels,
                                std::vector<base::string16>* icons,
                                std::vector<int>* unique_ids) {
  DCHECK_EQ(values->size(), labels->size());
  DCHECK_EQ(values->size(), icons->size());
  DCHECK_EQ(values->size(), unique_ids->size());

  std::set<std::pair<base::string16, base::string16> > seen_suggestions;
  std::vector<base::string16> values_copy;
  std::vector<base::string16> labels_copy;
  std::vector<base::string16> icons_copy;
  std::vector<int> unique_ids_copy;

  for (size_t i = 0; i < values->size(); ++i) {
    const std::pair<base::string16, base::string16> suggestion(
        (*values)[i], (*labels)[i]);
    if (seen_suggestions.insert(suggestion).second) {
      values_copy.push_back((*values)[i]);
      labels_copy.push_back((*labels)[i]);
      icons_copy.push_back((*icons)[i]);
      unique_ids_copy.push_back((*unique_ids)[i]);
    }
  }

  values->swap(values_copy);
  labels->swap(labels_copy);
  icons->swap(icons_copy);
  unique_ids->swap(unique_ids_copy);
}

// Precondition: |form_structure| and |form| should correspond to the same
// logical form.  Returns true if any field in the given |section| within |form|
// is auto-filled.
bool SectionIsAutofilled(const FormStructure& form_structure,
                         const FormData& form,
                         const std::string& section) {
  DCHECK_EQ(form_structure.field_count(), form.fields.size());
  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    if (form_structure.field(i)->section() == section &&
        form.fields[i].is_autofilled) {
      return true;
    }
  }

  return false;
}

bool FormIsHTTPS(const FormStructure& form) {
  // TODO(blundell): Change this to use a constant once crbug.com/306258 is
  // fixed.
  return form.source_url().SchemeIs("https");
}

// Uses the existing personal data in |profiles| and |credit_cards| to determine
// possible field types for the |submitted_form|.  This is potentially
// expensive -- on the order of 50ms even for a small set of |stored_data|.
// Hence, it should not run on the UI thread -- to avoid locking up the UI --
// nor on the IO thread -- to avoid blocking IPC calls.
void DeterminePossibleFieldTypesForUpload(
    const std::vector<AutofillProfile>& profiles,
    const std::vector<CreditCard>& credit_cards,
    const std::string& app_locale,
    FormStructure* submitted_form) {
  // For each field in the |submitted_form|, extract the value.  Then for each
  // profile or credit card, identify any stored types that match the value.
  for (size_t i = 0; i < submitted_form->field_count(); ++i) {
    AutofillField* field = submitted_form->field(i);
    ServerFieldTypeSet matching_types;

    // If it's a password field, set the type directly.
    if (field->form_control_type == "password") {
      matching_types.insert(PASSWORD);
    } else {
      base::string16 value;
      base::TrimWhitespace(field->value, base::TRIM_ALL, &value);
      for (std::vector<AutofillProfile>::const_iterator it = profiles.begin();
           it != profiles.end(); ++it) {
        it->GetMatchingTypes(value, app_locale, &matching_types);
      }
      for (std::vector<CreditCard>::const_iterator it = credit_cards.begin();
            it != credit_cards.end(); ++it) {
        it->GetMatchingTypes(value, app_locale, &matching_types);
      }
    }

    if (matching_types.empty())
      matching_types.insert(UNKNOWN_TYPE);

    field->set_possible_types(matching_types);
  }
}

}  // namespace

AutofillManager::AutofillManager(
    AutofillDriver* driver,
    AutofillClient* client,
    const std::string& app_locale,
    AutofillDownloadManagerState enable_download_manager)
    : driver_(driver),
      client_(client),
      app_locale_(app_locale),
      personal_data_(client->GetPersonalDataManager()),
      autocomplete_history_manager_(
          new AutocompleteHistoryManager(driver, client)),
      metric_logger_(new AutofillMetrics),
      has_logged_autofill_enabled_(false),
      has_logged_address_suggestions_count_(false),
      did_show_suggestions_(false),
      user_did_type_(false),
      user_did_autofill_(false),
      user_did_edit_autofilled_field_(false),
      external_delegate_(NULL),
      test_delegate_(NULL),
      weak_ptr_factory_(this) {
  if (enable_download_manager == ENABLE_AUTOFILL_DOWNLOAD_MANAGER) {
    download_manager_.reset(
        new AutofillDownloadManager(driver, client_->GetPrefs(), this));
  }
}

AutofillManager::~AutofillManager() {}

// static
void AutofillManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kAutofillEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if defined(OS_MACOSX)
  registry->RegisterBooleanPref(
      prefs::kAutofillAuxiliaryProfilesEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#else  // defined(OS_MACOSX)
  registry->RegisterBooleanPref(
      prefs::kAutofillAuxiliaryProfilesEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif  // defined(OS_MACOSX)
#if defined(OS_MACOSX)
  registry->RegisterBooleanPref(
      prefs::kAutofillMacAddressBookQueried,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif  // defined(OS_MACOSX)
  registry->RegisterDoublePref(
      prefs::kAutofillPositiveUploadRate,
      kAutofillPositiveUploadRateDefaultValue,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kAutofillNegativeUploadRate,
      kAutofillNegativeUploadRateDefaultValue,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  registry->RegisterBooleanPref(
      prefs::kAutofillUseMacAddressBook,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
void AutofillManager::MigrateUserPrefs(PrefService* prefs) {
  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kAutofillUseMacAddressBook);

  // If the pref is not its default value, then the migration has already been
  // performed.
  if (!pref->IsDefaultValue())
    return;

  // Whether Chrome has already tried to access the user's Address Book.
  const PrefService::Preference* pref_accessed =
      prefs->FindPreference(prefs::kAutofillMacAddressBookQueried);
  // Whether the user wants to use the Address Book to populate Autofill.
  const PrefService::Preference* pref_enabled =
      prefs->FindPreference(prefs::kAutofillAuxiliaryProfilesEnabled);

  if (pref_accessed->IsDefaultValue() && pref_enabled->IsDefaultValue()) {
    // This is likely a new user. Reset the default value to prevent the
    // migration from happening again.
    prefs->SetBoolean(prefs::kAutofillUseMacAddressBook,
                      prefs->GetBoolean(prefs::kAutofillUseMacAddressBook));
    return;
  }

  bool accessed;
  bool enabled;
  bool success = pref_accessed->GetValue()->GetAsBoolean(&accessed);
  DCHECK(success);
  success = pref_enabled->GetValue()->GetAsBoolean(&enabled);
  DCHECK(success);

  prefs->SetBoolean(prefs::kAutofillUseMacAddressBook, accessed && enabled);
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

void AutofillManager::SetExternalDelegate(AutofillExternalDelegate* delegate) {
  // TODO(jrg): consider passing delegate into the ctor.  That won't
  // work if the delegate has a pointer to the AutofillManager, but
  // future directions may not need such a pointer.
  external_delegate_ = delegate;
  autocomplete_history_manager_->SetExternalDelegate(delegate);
}

void AutofillManager::ShowAutofillSettings() {
  client_->ShowAutofillSettings();
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
bool AutofillManager::ShouldShowAccessAddressBookSuggestion(
    const FormData& form,
    const FormFieldData& field) {
  if (!personal_data_)
    return false;
  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return false;

  return personal_data_->ShouldShowAccessAddressBookSuggestion(
      autofill_field->Type());
}

bool AutofillManager::AccessAddressBook() {
  if (!personal_data_)
    return false;
  return personal_data_->AccessAddressBook();
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

bool AutofillManager::OnFormSubmitted(const FormData& form,
                                      const TimeTicks& timestamp) {
  if (!IsValidFormData(form))
    return false;

  // Let Autocomplete know as well.
  autocomplete_history_manager_->OnFormSubmitted(form);

  // Grab a copy of the form data.
  scoped_ptr<FormStructure> submitted_form(new FormStructure(form));

  if (!ShouldUploadForm(*submitted_form))
    return false;

  // Don't save data that was submitted through JavaScript.
  if (!form.user_submitted)
    return false;

  // Ignore forms not present in our cache.  These are typically forms with
  // wonky JavaScript that also makes them not auto-fillable.
  FormStructure* cached_submitted_form;
  if (!FindCachedForm(form, &cached_submitted_form))
    return false;

  submitted_form->UpdateFromCache(*cached_submitted_form);
  if (submitted_form->IsAutofillable())
    ImportFormData(*submitted_form);

  // Only upload server statistics and UMA metrics if at least some local data
  // is available to use as a baseline.
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  const std::vector<CreditCard*>& credit_cards =
      personal_data_->GetCreditCards();
  if (!profiles.empty() || !credit_cards.empty()) {
    // Copy the profile and credit card data, so that it can be accessed on a
    // separate thread.
    std::vector<AutofillProfile> copied_profiles;
    copied_profiles.reserve(profiles.size());
    for (std::vector<AutofillProfile*>::const_iterator it = profiles.begin();
         it != profiles.end(); ++it) {
      copied_profiles.push_back(**it);
    }

    std::vector<CreditCard> copied_credit_cards;
    copied_credit_cards.reserve(credit_cards.size());
    for (std::vector<CreditCard*>::const_iterator it = credit_cards.begin();
         it != credit_cards.end(); ++it) {
      copied_credit_cards.push_back(**it);
    }

    // Note that ownership of |submitted_form| is passed to the second task,
    // using |base::Owned|.
    FormStructure* raw_submitted_form = submitted_form.get();
    driver_->GetBlockingPool()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&DeterminePossibleFieldTypesForUpload,
                   copied_profiles,
                   copied_credit_cards,
                   app_locale_,
                   raw_submitted_form),
        base::Bind(&AutofillManager::UploadFormDataAsyncCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Owned(submitted_form.release()),
                   forms_loaded_timestamps_[form],
                   initial_interaction_timestamp_,
                   timestamp));
  }

  return true;
}

void AutofillManager::OnFormsSeen(const std::vector<FormData>& forms,
                                  const TimeTicks& timestamp) {
  if (!IsValidFormDataVector(forms))
    return;

  if (!driver_->RendererIsAvailable())
    return;

  bool enabled = IsAutofillEnabled();
  if (!has_logged_autofill_enabled_) {
    metric_logger_->LogIsAutofillEnabledAtPageLoad(enabled);
    has_logged_autofill_enabled_ = true;
  }

  if (!enabled)
    return;

  for (size_t i = 0; i < forms.size(); ++i) {
    forms_loaded_timestamps_[forms[i]] = timestamp;
  }

  ParseForms(forms);
}

void AutofillManager::OnTextFieldDidChange(const FormData& form,
                                           const FormFieldData& field,
                                           const TimeTicks& timestamp) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  if (!user_did_type_) {
    user_did_type_ = true;
    metric_logger_->LogUserHappinessMetric(AutofillMetrics::USER_DID_TYPE);
  }

  if (autofill_field->is_autofilled) {
    autofill_field->is_autofilled = false;
    metric_logger_->LogUserHappinessMetric(
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD);

    if (!user_did_edit_autofilled_field_) {
      user_did_edit_autofilled_field_ = true;
      metric_logger_->LogUserHappinessMetric(
          AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE);
    }
  }

  UpdateInitialInteractionTimestamp(timestamp);
}

void AutofillManager::OnQueryFormFieldAutofill(int query_id,
                                               const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box,
                                               bool display_warning) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  std::vector<base::string16> values;
  std::vector<base::string16> labels;
  std::vector<base::string16> icons;
  std::vector<int> unique_ids;

  external_delegate_->OnQuery(query_id,
                              form,
                              field,
                              bounding_box,
                              display_warning);
  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (RefreshDataModels() &&
      driver_->RendererIsAvailable() &&
      GetCachedFormAndField(form, field, &form_structure, &autofill_field) &&
      // Don't send suggestions for forms that aren't auto-fillable.
      form_structure->IsAutofillable()) {
    AutofillType type = autofill_field->Type();
    bool is_filling_credit_card = (type.group() == CREDIT_CARD);
    if (is_filling_credit_card) {
      GetCreditCardSuggestions(
          field, type, &values, &labels, &icons, &unique_ids);
    } else {
      GetProfileSuggestions(
          form_structure, field, type, &values, &labels, &icons, &unique_ids);
    }

    DCHECK_EQ(values.size(), labels.size());
    DCHECK_EQ(values.size(), icons.size());
    DCHECK_EQ(values.size(), unique_ids.size());

    if (!values.empty()) {
      // Don't provide Autofill suggestions when Autofill is disabled, and don't
      // provide credit card suggestions for non-HTTPS pages. However, provide a
      // warning to the user in these cases.
      int warning = 0;
      if (!form_structure->IsAutofillable())
        warning = IDS_AUTOFILL_WARNING_FORM_DISABLED;
      else if (is_filling_credit_card && !FormIsHTTPS(*form_structure))
        warning = IDS_AUTOFILL_WARNING_INSECURE_CONNECTION;
      if (warning) {
        values.assign(1, l10n_util::GetStringUTF16(warning));
        labels.assign(1, base::string16());
        icons.assign(1, base::string16());
        unique_ids.assign(1, POPUP_ITEM_ID_WARNING_MESSAGE);
      } else {
        bool section_is_autofilled =
            SectionIsAutofilled(*form_structure, form,
                                autofill_field->section());
        if (section_is_autofilled) {
          // If the relevant section is auto-filled and the renderer is querying
          // for suggestions, then the user is editing the value of a field.
          // In this case, mimic autocomplete: don't display labels or icons,
          // as that information is redundant.
          labels.assign(labels.size(), base::string16());
          icons.assign(icons.size(), base::string16());
        }

        // When filling credit card suggestions, the values and labels are
        // typically obfuscated, which makes detecting duplicates hard.  Since
        // duplicates only tend to be a problem when filling address forms
        // anyway, only don't de-dup credit card suggestions.
        if (!is_filling_credit_card)
          RemoveDuplicateSuggestions(&values, &labels, &icons, &unique_ids);

        // The first time we show suggestions on this page, log the number of
        // suggestions shown.
        if (!has_logged_address_suggestions_count_ && !section_is_autofilled) {
          metric_logger_->LogAddressSuggestionsCount(values.size());
          has_logged_address_suggestions_count_ = true;
        }
      }
    }
  }

  // Add the results from AutoComplete.  They come back asynchronously, so we
  // hand off what we generated and they will send the results back to the
  // renderer.
  autocomplete_history_manager_->OnGetAutocompleteSuggestions(
      query_id, field.name, field.value, field.form_control_type, values,
      labels, icons, unique_ids);
}

void AutofillManager::FillOrPreviewForm(
    AutofillDriver::RendererFormDataAction action,
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    int unique_id) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  const AutofillDataModel* data_model = NULL;
  size_t variant = 0;
  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  bool is_credit_card = false;
  // NOTE: RefreshDataModels may invalidate |data_model| because it causes the
  // PersonalDataManager to reload Mac address book entries. Thus it must come
  // before GetProfileOrCreditCard.
  if (!RefreshDataModels() ||
      !driver_->RendererIsAvailable() ||
      !GetProfileOrCreditCard(
          unique_id, &data_model, &variant, &is_credit_card) ||
      !GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  DCHECK(form_structure);
  DCHECK(autofill_field);

  FormData result = form;

  base::string16 profile_full_name;
  std::string profile_language_code;
  if (!is_credit_card) {
    profile_full_name = data_model->GetInfo(
        AutofillType(NAME_FULL), app_locale_);
    profile_language_code =
        static_cast<const AutofillProfile*>(data_model)->language_code();
  }

  // If the relevant section is auto-filled, we should fill |field| but not the
  // rest of the form.
  if (SectionIsAutofilled(*form_structure, form, autofill_field->section())) {
    for (std::vector<FormFieldData>::iterator iter = result.fields.begin();
         iter != result.fields.end(); ++iter) {
      if ((*iter) == field) {
        base::string16 value = data_model->GetInfoForVariant(
            autofill_field->Type(), variant, app_locale_);
        if (AutofillField::FillFormField(*autofill_field,
                                         value,
                                         profile_language_code,
                                         app_locale_,
                                         &(*iter))) {
          // Mark the cached field as autofilled, so that we can detect when a
          // user edits an autofilled field (for metrics).
          autofill_field->is_autofilled = true;

          // Mark the field as autofilled when a non-empty value is assigned to
          // it. This allows the renderer to distinguish autofilled fields from
          // fields with non-empty values, such as select-one fields.
          iter->is_autofilled = true;

          if (!is_credit_card && !value.empty())
            client_->DidFillOrPreviewField(value, profile_full_name);
        }
        break;
      }
    }

    driver_->SendFormDataToRenderer(query_id, action, result);
    return;
  }

  // Cache the field type for the field from which the user initiated autofill.
  FieldTypeGroup initiating_group_type = autofill_field->Type().group();
  DCHECK_EQ(form_structure->field_count(), form.fields.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    if (form_structure->field(i)->section() != autofill_field->section())
      continue;

    DCHECK_EQ(*form_structure->field(i), result.fields[i]);

    const AutofillField* cached_field = form_structure->field(i);
    FieldTypeGroup field_group_type = cached_field->Type().group();
    if (field_group_type != NO_GROUP) {
      // If the field being filled is either
      //   (a) the field that the user initiated the fill from, or
      //   (b) part of the same logical unit, e.g. name or phone number,
      // then take the multi-profile "variant" into account.
      // Otherwise fill with the default (zeroth) variant.
      size_t use_variant = 0;
      if (result.fields[i] == field ||
          field_group_type == initiating_group_type) {
        use_variant = variant;
      }
      base::string16 value = data_model->GetInfoForVariant(
          cached_field->Type(), use_variant, app_locale_);

      // Must match ForEachMatchingFormField() in form_autofill_util.cc.
      // Only notify autofilling of empty fields and the field that initiated
      // the filling (note that "select-one" controls may not be empty but will
      // still be autofilled).
      bool should_notify =
          !is_credit_card &&
          !value.empty() &&
          (result.fields[i] == field ||
           result.fields[i].form_control_type == "select-one" ||
           result.fields[i].value.empty());
      if (AutofillField::FillFormField(*cached_field,
                                       value,
                                       profile_language_code,
                                       app_locale_,
                                       &result.fields[i])) {
        // Mark the cached field as autofilled, so that we can detect when a
        // user edits an autofilled field (for metrics).
        form_structure->field(i)->is_autofilled = true;

        // Mark the field as autofilled when a non-empty value is assigned to
        // it. This allows the renderer to distinguish autofilled fields from
        // fields with non-empty values, such as select-one fields.
        result.fields[i].is_autofilled = true;

        if (should_notify)
          client_->DidFillOrPreviewField(value, profile_full_name);
      }
    }
  }

  autofilled_form_signatures_.push_front(form_structure->FormSignature());
  // Only remember the last few forms that we've seen, both to avoid false
  // positives and to avoid wasting memory.
  if (autofilled_form_signatures_.size() > kMaxRecentFormSignaturesToRemember)
    autofilled_form_signatures_.pop_back();

  driver_->SendFormDataToRenderer(query_id, action, result);
}

void AutofillManager::OnDidPreviewAutofillFormData() {
  if (test_delegate_)
    test_delegate_->DidPreviewFormData();
}

void AutofillManager::OnDidFillAutofillFormData(const TimeTicks& timestamp) {
  if (test_delegate_)
    test_delegate_->DidFillFormData();

  metric_logger_->LogUserHappinessMetric(AutofillMetrics::USER_DID_AUTOFILL);
  if (!user_did_autofill_) {
    user_did_autofill_ = true;
    metric_logger_->LogUserHappinessMetric(
        AutofillMetrics::USER_DID_AUTOFILL_ONCE);
  }

  UpdateInitialInteractionTimestamp(timestamp);
}

void AutofillManager::DidShowSuggestions(bool is_new_popup) {
  if (test_delegate_)
    test_delegate_->DidShowSuggestions();

  if (is_new_popup) {
    metric_logger_->LogUserHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);

    if (!did_show_suggestions_) {
      did_show_suggestions_ = true;
      metric_logger_->LogUserHappinessMetric(
          AutofillMetrics::SUGGESTIONS_SHOWN_ONCE);
    }
  }
}

void AutofillManager::OnHidePopup() {
  if (!IsAutofillEnabled())
    return;

  client_->HideAutofillPopup();
}

void AutofillManager::RemoveAutofillProfileOrCreditCard(int unique_id) {
  const AutofillDataModel* data_model = NULL;
  size_t variant = 0;
  bool unused_is_credit_card = false;
  if (!GetProfileOrCreditCard(
          unique_id, &data_model, &variant, &unused_is_credit_card)) {
    NOTREACHED();
    return;
  }

  // TODO(csharp): If we are dealing with a variant only the variant should
  // be deleted, instead of doing nothing.
  // http://crbug.com/124211
  if (variant != 0)
    return;

  personal_data_->RemoveByGUID(data_model->guid());
}

void AutofillManager::RemoveAutocompleteEntry(const base::string16& name,
                                              const base::string16& value) {
  autocomplete_history_manager_->OnRemoveAutocompleteEntry(name, value);
}

const std::vector<FormStructure*>& AutofillManager::GetFormStructures() {
  return form_structures_.get();
}

void AutofillManager::SetTestDelegate(AutofillManagerTestDelegate* delegate) {
  test_delegate_ = delegate;
}

void AutofillManager::OnSetDataList(const std::vector<base::string16>& values,
                                    const std::vector<base::string16>& labels) {
  if (!IsValidString16Vector(values) ||
      !IsValidString16Vector(labels) ||
      values.size() != labels.size())
    return;

  external_delegate_->SetCurrentDataListValues(values, labels);
}

void AutofillManager::OnLoadedServerPredictions(
    const std::string& response_xml) {
  // Parse and store the server predictions.
  FormStructure::ParseQueryResponse(response_xml,
                                    form_structures_.get(),
                                    *metric_logger_);

  // Forward form structures to the password generation manager to detect
  // account creation forms.
  client_->DetectAccountCreationForms(form_structures_.get());

  // If the corresponding flag is set, annotate forms with the predicted types.
  driver_->SendAutofillTypePredictionsToRenderer(form_structures_.get());
}

void AutofillManager::OnDidEndTextFieldEditing() {
  external_delegate_->DidEndTextFieldEditing();
}

bool AutofillManager::IsAutofillEnabled() const {
  return client_->GetPrefs()->GetBoolean(prefs::kAutofillEnabled);
}

void AutofillManager::ImportFormData(const FormStructure& submitted_form) {
  scoped_ptr<CreditCard> imported_credit_card;
  if (!personal_data_->ImportFormData(submitted_form, &imported_credit_card))
    return;

  // If credit card information was submitted, we need to confirm whether to
  // save it.
  if (imported_credit_card) {
    client_->ConfirmSaveCreditCard(
        *metric_logger_,
        base::Bind(
            base::IgnoreResult(&PersonalDataManager::SaveImportedCreditCard),
            base::Unretained(personal_data_),
            *imported_credit_card));
  }
}

// Note that |submitted_form| is passed as a pointer rather than as a reference
// so that we can get memory management right across threads.  Note also that we
// explicitly pass in all the time stamps of interest, as the cached ones might
// get reset before this method executes.
void AutofillManager::UploadFormDataAsyncCallback(
    const FormStructure* submitted_form,
    const TimeTicks& load_time,
    const TimeTicks& interaction_time,
    const TimeTicks& submission_time) {
  submitted_form->LogQualityMetrics(*metric_logger_,
                                    load_time,
                                    interaction_time,
                                    submission_time);

  if (submitted_form->ShouldBeCrowdsourced())
    UploadFormData(*submitted_form);
}

void AutofillManager::UploadFormData(const FormStructure& submitted_form) {
  if (!download_manager_)
    return;

  // Check if the form is among the forms that were recently auto-filled.
  bool was_autofilled = false;
  std::string form_signature = submitted_form.FormSignature();
  for (std::list<std::string>::const_iterator it =
           autofilled_form_signatures_.begin();
       it != autofilled_form_signatures_.end() && !was_autofilled;
       ++it) {
    if (*it == form_signature)
      was_autofilled = true;
  }

  ServerFieldTypeSet non_empty_types;
  personal_data_->GetNonEmptyTypes(&non_empty_types);
  // Always add PASSWORD to |non_empty_types| so that if |submitted_form|
  // contains a password field it will be uploaded to the server. If
  // |submitted_form| doesn't contain a password field, there is no side
  // effect from adding PASSWORD to |non_empty_types|.
  non_empty_types.insert(PASSWORD);

  download_manager_->StartUploadRequest(submitted_form, was_autofilled,
                                        non_empty_types);
}

bool AutofillManager::UploadPasswordGenerationForm(const FormData& form) {
  FormStructure form_structure(form);

  if (!ShouldUploadForm(form_structure))
    return false;

  if (!form_structure.ShouldBeCrowdsourced())
    return false;

  // TODO(gcasto): Check that PasswordGeneration is enabled?

  // Find the first password field to label. We don't try to label anything
  // else.
  bool found_password_field = false;
  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    AutofillField* field = form_structure.field(i);

    ServerFieldTypeSet types;
    if (!found_password_field && field->form_control_type == "password") {
      types.insert(ACCOUNT_CREATION_PASSWORD);
      found_password_field = true;
    } else {
      types.insert(UNKNOWN_TYPE);
    }
    field->set_possible_types(types);
  }
  DCHECK(found_password_field);

  // Only one field type should be present.
  ServerFieldTypeSet available_field_types;
  available_field_types.insert(ACCOUNT_CREATION_PASSWORD);

  // Force uploading as these events are relatively rare and we want to make
  // sure to receive them. It also makes testing easier if these requests
  // always pass.
  form_structure.set_upload_required(UPLOAD_REQUIRED);

  if (!download_manager_)
    return false;

  return download_manager_->StartUploadRequest(form_structure,
                                               false /* was_autofilled */,
                                               available_field_types);
}

void AutofillManager::Reset() {
  form_structures_.clear();
  has_logged_autofill_enabled_ = false;
  has_logged_address_suggestions_count_ = false;
  did_show_suggestions_ = false;
  user_did_type_ = false;
  user_did_autofill_ = false;
  user_did_edit_autofilled_field_ = false;
  forms_loaded_timestamps_.clear();
  initial_interaction_timestamp_ = TimeTicks();
  external_delegate_->Reset();
}

AutofillManager::AutofillManager(AutofillDriver* driver,
                                 AutofillClient* client,
                                 PersonalDataManager* personal_data)
    : driver_(driver),
      client_(client),
      app_locale_("en-US"),
      personal_data_(personal_data),
      autocomplete_history_manager_(
          new AutocompleteHistoryManager(driver, client)),
      metric_logger_(new AutofillMetrics),
      has_logged_autofill_enabled_(false),
      has_logged_address_suggestions_count_(false),
      did_show_suggestions_(false),
      user_did_type_(false),
      user_did_autofill_(false),
      user_did_edit_autofilled_field_(false),
      external_delegate_(NULL),
      test_delegate_(NULL),
      weak_ptr_factory_(this) {
  DCHECK(driver_);
  DCHECK(client_);
}

void AutofillManager::set_metric_logger(const AutofillMetrics* metric_logger) {
  metric_logger_.reset(metric_logger);
}

bool AutofillManager::RefreshDataModels() const {
  if (!IsAutofillEnabled())
    return false;

  // No autofill data to return if the profiles are empty.
  if (personal_data_->GetProfiles().empty() &&
      personal_data_->GetCreditCards().empty()) {
    return false;
  }

  return true;
}

bool AutofillManager::GetProfileOrCreditCard(
    int unique_id,
    const AutofillDataModel** data_model,
    size_t* variant,
    bool* is_credit_card) const {
  // Unpack the |unique_id| into component parts.
  GUIDPair credit_card_guid;
  GUIDPair profile_guid;
  UnpackGUIDs(unique_id, &credit_card_guid, &profile_guid);
  DCHECK(!base::IsValidGUID(credit_card_guid.first) ||
         !base::IsValidGUID(profile_guid.first));
  *is_credit_card = false;

  // Find the profile that matches the |profile_guid|, if one is specified.
  // Otherwise find the credit card that matches the |credit_card_guid|,
  // if specified.
  if (base::IsValidGUID(profile_guid.first)) {
    *data_model = personal_data_->GetProfileByGUID(profile_guid.first);
    *variant = profile_guid.second;
  } else if (base::IsValidGUID(credit_card_guid.first)) {
    *data_model = personal_data_->GetCreditCardByGUID(credit_card_guid.first);
    *variant = credit_card_guid.second;
    *is_credit_card = true;
  }

  return !!*data_model;
}

bool AutofillManager::FindCachedForm(const FormData& form,
                                     FormStructure** form_structure) const {
  // Find the FormStructure that corresponds to |form|.
  // Scan backward through the cached |form_structures_|, as updated versions of
  // forms are added to the back of the list, whereas original versions of these
  // forms might appear toward the beginning of the list.  The communication
  // protocol with the crowdsourcing server does not permit us to discard the
  // original versions of the forms.
  *form_structure = NULL;
  for (std::vector<FormStructure*>::const_reverse_iterator iter =
           form_structures_.rbegin();
       iter != form_structures_.rend(); ++iter) {
    if (**iter == form) {
      *form_structure = *iter;

      // The same form might be cached with multiple field counts: in some
      // cases, non-autofillable fields are filtered out, whereas in other cases
      // they are not.  To avoid thrashing the cache, keep scanning until we
      // find a cached version with the same number of fields, if there is one.
      if ((*iter)->field_count() == form.fields.size())
        break;
    }
  }

  if (!(*form_structure))
    return false;

  return true;
}

bool AutofillManager::GetCachedFormAndField(const FormData& form,
                                            const FormFieldData& field,
                                            FormStructure** form_structure,
                                            AutofillField** autofill_field) {
  // Find the FormStructure that corresponds to |form|.
  // If we do not have this form in our cache but it is parseable, we'll add it
  // in the call to |UpdateCachedForm()|.
  if (!FindCachedForm(form, form_structure) &&
      !FormStructure(form).ShouldBeParsed()) {
    return false;
  }

  // Update the cached form to reflect any dynamic changes to the form data, if
  // necessary.
  if (!UpdateCachedForm(form, *form_structure, form_structure))
    return false;

  // No data to return if there are no auto-fillable fields.
  if (!(*form_structure)->autofill_count())
    return false;

  // Find the AutofillField that corresponds to |field|.
  *autofill_field = NULL;
  for (std::vector<AutofillField*>::const_iterator iter =
           (*form_structure)->begin();
       iter != (*form_structure)->end(); ++iter) {
    if ((**iter) == field) {
      *autofill_field = *iter;
      break;
    }
  }

  // Even though we always update the cache, the field might not exist if the
  // website disables autocomplete while the user is interacting with the form.
  // See http://crbug.com/160476
  return *autofill_field != NULL;
}

bool AutofillManager::UpdateCachedForm(const FormData& live_form,
                                       const FormStructure* cached_form,
                                       FormStructure** updated_form) {
  bool needs_update =
      (!cached_form ||
       live_form.fields.size() != cached_form->field_count());
  for (size_t i = 0; !needs_update && i < cached_form->field_count(); ++i) {
    needs_update = *cached_form->field(i) != live_form.fields[i];
  }

  if (!needs_update)
    return true;

  if (form_structures_.size() >= kMaxFormCacheSize)
    return false;

  // Add the new or updated form to our cache.
  form_structures_.push_back(new FormStructure(live_form));
  *updated_form = *form_structures_.rbegin();
  (*updated_form)->DetermineHeuristicTypes(*metric_logger_);

  // If we have cached data, propagate it to the updated form.
  if (cached_form) {
    std::map<base::string16, const AutofillField*> cached_fields;
    for (size_t i = 0; i < cached_form->field_count(); ++i) {
      const AutofillField* field = cached_form->field(i);
      cached_fields[field->unique_name()] = field;
    }

    for (size_t i = 0; i < (*updated_form)->field_count(); ++i) {
      AutofillField* field = (*updated_form)->field(i);
      std::map<base::string16, const AutofillField*>::iterator cached_field =
          cached_fields.find(field->unique_name());
      if (cached_field != cached_fields.end()) {
        field->set_server_type(cached_field->second->server_type());
        field->is_autofilled = cached_field->second->is_autofilled;
      }
    }

    // Note: We _must not_ remove the original version of the cached form from
    // the list of |form_structures_|.  Otherwise, we break parsing of the
    // crowdsourcing server's response to our query.
  }

  // Annotate the updated form with its predicted types.
  std::vector<FormStructure*> forms(1, *updated_form);
  driver_->SendAutofillTypePredictionsToRenderer(forms);

  return true;
}

void AutofillManager::GetProfileSuggestions(
    FormStructure* form,
    const FormFieldData& field,
    const AutofillType& type,
    std::vector<base::string16>* values,
    std::vector<base::string16>* labels,
    std::vector<base::string16>* icons,
    std::vector<int>* unique_ids) const {
  std::vector<ServerFieldType> field_types(form->field_count());
  for (size_t i = 0; i < form->field_count(); ++i) {
    field_types.push_back(form->field(i)->Type().GetStorableType());
  }
  std::vector<GUIDPair> guid_pairs;

  personal_data_->GetProfileSuggestions(
      type, field.value, field.is_autofilled, field_types,
      base::Callback<bool(const AutofillProfile&)>(),
      values, labels, icons, &guid_pairs);

  for (size_t i = 0; i < guid_pairs.size(); ++i) {
    unique_ids->push_back(PackGUIDs(GUIDPair(std::string(), 0),
                                    guid_pairs[i]));
  }
}

void AutofillManager::GetCreditCardSuggestions(
    const FormFieldData& field,
    const AutofillType& type,
    std::vector<base::string16>* values,
    std::vector<base::string16>* labels,
    std::vector<base::string16>* icons,
    std::vector<int>* unique_ids) const {
  std::vector<GUIDPair> guid_pairs;
  personal_data_->GetCreditCardSuggestions(
      type, field.value, values, labels, icons, &guid_pairs);

  for (size_t i = 0; i < guid_pairs.size(); ++i) {
    unique_ids->push_back(PackGUIDs(guid_pairs[i], GUIDPair(std::string(), 0)));
  }
}

void AutofillManager::ParseForms(const std::vector<FormData>& forms) {
  std::vector<FormStructure*> non_queryable_forms;
  for (std::vector<FormData>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    scoped_ptr<FormStructure> form_structure(new FormStructure(*iter));
    if (!form_structure->ShouldBeParsed())
      continue;

    form_structure->DetermineHeuristicTypes(*metric_logger_);

    // Set aside forms with method GET or author-specified types, so that they
    // are not included in the query to the server.
    if (form_structure->ShouldBeCrowdsourced())
      form_structures_.push_back(form_structure.release());
    else
      non_queryable_forms.push_back(form_structure.release());
  }

  if (!form_structures_.empty() && download_manager_) {
    // Query the server if we have at least one of the forms were parsed.
    download_manager_->StartQueryRequest(form_structures_.get(),
                                        *metric_logger_);
  }

  for (std::vector<FormStructure*>::const_iterator iter =
           non_queryable_forms.begin();
       iter != non_queryable_forms.end(); ++iter) {
    form_structures_.push_back(*iter);
  }

  if (!form_structures_.empty())
    metric_logger_->LogUserHappinessMetric(AutofillMetrics::FORMS_LOADED);

  // For the |non_queryable_forms|, we have all the field type info we're ever
  // going to get about them.  For the other forms, we'll wait until we get a
  // response from the server.
  driver_->SendAutofillTypePredictionsToRenderer(non_queryable_forms);
}

int AutofillManager::GUIDToID(const GUIDPair& guid) const {
  if (!base::IsValidGUID(guid.first))
    return 0;

  std::map<GUIDPair, int>::const_iterator iter = guid_id_map_.find(guid);
  if (iter == guid_id_map_.end()) {
    int id = guid_id_map_.size() + 1;
    guid_id_map_[guid] = id;
    id_guid_map_[id] = guid;
    return id;
  } else {
    return iter->second;
  }
}

const GUIDPair AutofillManager::IDToGUID(int id) const {
  if (id == 0)
    return GUIDPair(std::string(), 0);

  std::map<int, GUIDPair>::const_iterator iter = id_guid_map_.find(id);
  if (iter == id_guid_map_.end()) {
    NOTREACHED();
    return GUIDPair(std::string(), 0);
  }

  return iter->second;
}

// When sending IDs (across processes) to the renderer we pack credit card and
// profile IDs into a single integer.  Credit card IDs are sent in the high
// word and profile IDs are sent in the low word.
int AutofillManager::PackGUIDs(const GUIDPair& cc_guid,
                               const GUIDPair& profile_guid) const {
  int cc_id = GUIDToID(cc_guid);
  int profile_id = GUIDToID(profile_guid);

  DCHECK(cc_id <= std::numeric_limits<unsigned short>::max());
  DCHECK(profile_id <= std::numeric_limits<unsigned short>::max());

  return cc_id << std::numeric_limits<unsigned short>::digits | profile_id;
}

// When receiving IDs (across processes) from the renderer we unpack credit card
// and profile IDs from a single integer.  Credit card IDs are stored in the
// high word and profile IDs are stored in the low word.
void AutofillManager::UnpackGUIDs(int id,
                                  GUIDPair* cc_guid,
                                  GUIDPair* profile_guid) const {
  int cc_id = id >> std::numeric_limits<unsigned short>::digits &
      std::numeric_limits<unsigned short>::max();
  int profile_id = id & std::numeric_limits<unsigned short>::max();

  *cc_guid = IDToGUID(cc_id);
  *profile_guid = IDToGUID(profile_id);
}

void AutofillManager::UpdateInitialInteractionTimestamp(
    const TimeTicks& interaction_timestamp) {
  if (initial_interaction_timestamp_.is_null() ||
      interaction_timestamp < initial_interaction_timestamp_) {
    initial_interaction_timestamp_ = interaction_timestamp;
  }
}

bool AutofillManager::ShouldUploadForm(const FormStructure& form) {
  if (!IsAutofillEnabled())
    return false;

  if (driver_->IsOffTheRecord())
    return false;

  // Disregard forms that we wouldn't ever autofill in the first place.
  if (!form.ShouldBeParsed())
    return false;

  return true;
}

}  // namespace autofill
