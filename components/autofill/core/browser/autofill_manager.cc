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
#include "base/message_loop/message_loop.h"
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
#include "components/autofill/core/browser/field_types.h"
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
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace autofill {

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
void RemoveDuplicateSuggestions(std::vector<Suggestion>* suggestions) {
  std::set<std::pair<base::string16, base::string16>> seen_suggestions;

  for (int i = 0; i < static_cast<int>(suggestions->size()); ++i) {
    if (!seen_suggestions.insert(std::make_pair(
            (*suggestions)[i].value, (*suggestions)[i].label)).second) {
      // Duplicate found, delete it.
      suggestions->erase(suggestions->begin() + i);
      i--;
    }
  }
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
  return form.source_url().SchemeIs(url::kHttpsScheme);
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
      real_pan_client_(driver->GetURLRequestContext(), this),
      app_locale_(app_locale),
      personal_data_(client->GetPersonalDataManager()),
      autocomplete_history_manager_(
          new AutocompleteHistoryManager(driver, client)),
      address_form_event_logger_(
          new AutofillMetrics::FormEventLogger(false /* is_for_credit_card */)),
      credit_card_form_event_logger_(
          new AutofillMetrics::FormEventLogger(true /* is_for_credit_card */)),
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
  registry->RegisterBooleanPref(
      prefs::kAutofillWalletSyncExperimentEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  // TODO(estade): Should this be syncable?
  registry->RegisterBooleanPref(
      prefs::kAutofillWalletImportEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  // This choice is made on a per-device basis, so it's not syncable.
  registry->RegisterBooleanPref(
      prefs::kAutofillWalletImportStorageCheckboxState,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
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
  registry->RegisterIntegerPref(
      prefs::kAutofillMacAddressBookShowedCount,
      0,
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
  AutofillField* autofill_field = GetAutofillField(form, field);
  return autofill_field &&
         personal_data_->ShouldShowAccessAddressBookSuggestion(
             autofill_field->Type());
}

bool AutofillManager::AccessAddressBook() {
  if (!personal_data_)
    return false;
  return personal_data_->AccessAddressBook();
}

void AutofillManager::ShowedAccessAddressBookPrompt() {
  if (!personal_data_)
    return;
  return personal_data_->ShowedAccessAddressBookPrompt();
}

int AutofillManager::AccessAddressBookPromptCount() {
  if (!personal_data_)
    return 0;
  return personal_data_->AccessAddressBookPromptCount();
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

bool AutofillManager::ShouldShowScanCreditCard(const FormData& form,
                                               const FormFieldData& field) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          autofill::switches::kEnableCreditCardScan)) {
    return false;
  }

  if (!client_->HasCreditCardScanFeature())
    return false;

  AutofillField* autofill_field = GetAutofillField(form, field);
  if (!autofill_field ||
      autofill_field->Type().GetStorableType() != CREDIT_CARD_NUMBER) {
    return false;
  }

  static const int kShowScanCreditCardMaxValueLength = 6;
  return field.value.size() <= kShowScanCreditCardMaxValueLength &&
         base::ContainsOnlyChars(CreditCard::StripSeparators(field.value),
                                 base::ASCIIToUTF16("0123456789"));
}

bool AutofillManager::OnWillSubmitForm(const FormData& form,
                                       const TimeTicks& timestamp) {
  if (!IsValidFormData(form))
    return false;

  scoped_ptr<FormStructure> submitted_form = ValidateSubmittedForm(form);
  if (!submitted_form)
    return false;

  address_form_event_logger_->OnDidSubmitForm();
  credit_card_form_event_logger_->OnDidSubmitForm();

  // Only upload server statistics and UMA metrics if at least some local data
  // is available to use as a baseline.
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  if (submitted_form->IsAutofillable()) {
    AutofillMetrics::LogNumberOfProfilesAtAutofillableFormSubmission(
        personal_data_->GetProfiles().size());
  }
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

bool AutofillManager::OnFormSubmitted(const FormData& form) {
  if (!IsValidFormData(form))
    return false;

  // Let Autocomplete know as well.
  autocomplete_history_manager_->OnFormSubmitted(form);

  scoped_ptr<FormStructure> submitted_form = ValidateSubmittedForm(form);
  if (!submitted_form)
    return false;

  // Update Personal Data with the form's submitted data.
  if (submitted_form->IsAutofillable())
    ImportFormData(*submitted_form);

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
    AutofillMetrics::LogIsAutofillEnabledAtPageLoad(enabled);
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
    AutofillMetrics::LogUserHappinessMetric(AutofillMetrics::USER_DID_TYPE);
  }

  if (autofill_field->is_autofilled) {
    autofill_field->is_autofilled = false;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD);

    if (!user_did_edit_autofilled_field_) {
      user_did_edit_autofilled_field_ = true;
      AutofillMetrics::LogUserHappinessMetric(
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

  std::vector<Suggestion> suggestions;

  external_delegate_->OnQuery(query_id,
                              form,
                              field,
                              bounding_box,
                              display_warning);

  // Need to refresh models before using the form_event_loggers.
  bool is_autofill_possible = RefreshDataModels();

  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  bool got_autofillable_form =
      GetCachedFormAndField(form, field, &form_structure, &autofill_field) &&
      // Don't send suggestions or track forms that aren't auto-fillable.
      form_structure->IsAutofillable();

  // Logging interactions of forms that are autofillable.
  if (got_autofillable_form) {
    if (autofill_field->Type().group() == CREDIT_CARD)
      credit_card_form_event_logger_->OnDidInteractWithAutofillableForm();
    else
      address_form_event_logger_->OnDidInteractWithAutofillableForm();
  }

  if (is_autofill_possible &&
      driver_->RendererIsAvailable() &&
      got_autofillable_form) {
    AutofillType type = autofill_field->Type();
    bool is_filling_credit_card = (type.group() == CREDIT_CARD);
    if (is_filling_credit_card) {
      suggestions = GetCreditCardSuggestions(field, type);
    } else {
      suggestions =
          GetProfileSuggestions(*form_structure, field, *autofill_field);
    }
    if (!suggestions.empty()) {
      // Don't provide Autofill suggestions when Autofill is disabled, and don't
      // provide credit card suggestions for non-HTTPS pages. However, provide a
      // warning to the user in these cases.
      int warning = 0;
      if (is_filling_credit_card && !FormIsHTTPS(*form_structure)) {
        warning = IDS_AUTOFILL_WARNING_INSECURE_CONNECTION;
      }
      if (warning) {
        Suggestion warning_suggestion(l10n_util::GetStringUTF16(warning));
        warning_suggestion.frontend_id = POPUP_ITEM_ID_WARNING_MESSAGE;
        suggestions.assign(1, warning_suggestion);
      } else {
        bool section_is_autofilled =
            SectionIsAutofilled(*form_structure, form,
                                autofill_field->section());
        if (section_is_autofilled) {
          // If the relevant section is auto-filled and the renderer is querying
          // for suggestions, then the user is editing the value of a field.
          // In this case, mimic autocomplete: don't display labels or icons,
          // as that information is redundant.
          for (size_t i = 0; i < suggestions.size(); i++) {
            suggestions[i].label = base::string16();
            suggestions[i].icon = base::string16();
          }
        }

        // When filling credit card suggestions, the values and labels are
        // typically obfuscated, which makes detecting duplicates hard.  Since
        // duplicates only tend to be a problem when filling address forms
        // anyway, only don't de-dup credit card suggestions.
        if (!is_filling_credit_card)
          RemoveDuplicateSuggestions(&suggestions);

        // The first time we show suggestions on this page, log the number of
        // suggestions available.
        // TODO(mathp): Differentiate between number of suggestions available
        // (current metric) and number shown to the user.
        if (!has_logged_address_suggestions_count_ && !section_is_autofilled) {
          AutofillMetrics::LogAddressSuggestionsCount(suggestions.size());
          has_logged_address_suggestions_count_ = true;
        }
      }
    }
  }

  if (field.should_autocomplete) {
    // Add the results from AutoComplete.  They come back asynchronously, so we
    // hand off what we generated and they will send the results back to the
    // renderer.
    autocomplete_history_manager_->OnGetAutocompleteSuggestions(
        query_id, field.name, field.value, field.form_control_type,
        suggestions);
  } else {
    // Autocomplete is disabled for this field; only pass back Autofill
    // suggestions.
    autocomplete_history_manager_->CancelPendingQuery();
    external_delegate_->OnSuggestionsReturned(
        query_id, suggestions);
  }
}

void AutofillManager::FillOrPreviewCreditCardForm(
    AutofillDriver::RendererFormDataAction action,
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const CreditCard& credit_card,
    size_t variant) {
  if (action == AutofillDriver::FORM_DATA_ACTION_FILL) {
    if (credit_card.record_type() == CreditCard::MASKED_SERVER_CARD) {
      unmasking_card_ = credit_card;
      unmasking_query_id_ = query_id;
      unmasking_form_ = form;
      unmasking_field_ = field;
      real_pan_client_.Prepare();
      client()->ShowUnmaskPrompt(unmasking_card_,
                                 weak_ptr_factory_.GetWeakPtr());
      credit_card_form_event_logger_->OnDidSelectMaskedServerCardSuggestion();
      return;
    }
    credit_card_form_event_logger_->OnDidFillSuggestion(credit_card);
  }

  FillOrPreviewDataModelForm(action, query_id, form, field, credit_card,
                             variant, true /* is_credit_card */);
}

void AutofillManager::FillOrPreviewProfileForm(
    AutofillDriver::RendererFormDataAction action,
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const AutofillProfile& profile,
    size_t variant) {
  if (action == AutofillDriver::FORM_DATA_ACTION_FILL)
    address_form_event_logger_->OnDidFillSuggestion(profile);

  FillOrPreviewDataModelForm(action, query_id, form, field, profile, variant,
                             false /* is_credit_card */);
}

void AutofillManager::FillOrPreviewForm(
    AutofillDriver::RendererFormDataAction action,
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    int unique_id) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  // NOTE: RefreshDataModels may invalidate |data_model| because it causes the
  // PersonalDataManager to reload Mac address book entries. Thus it must come
  // before GetProfile or GetCreditCard.
  if (!RefreshDataModels() || !driver_->RendererIsAvailable())
    return;

  size_t variant = 0;
  const CreditCard* credit_card = nullptr;
  const AutofillProfile* profile = nullptr;
  if (GetCreditCard(unique_id, &credit_card))
    FillOrPreviewCreditCardForm(action, query_id, form, field, *credit_card, 0);
  else if (GetProfile(unique_id, &profile, &variant))
    FillOrPreviewProfileForm(action, query_id, form, field, *profile, variant);
}

void AutofillManager::FillCreditCardForm(int query_id,
                                         const FormData& form,
                                         const FormFieldData& field,
                                         const CreditCard& credit_card) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field) ||
      !driver_->RendererIsAvailable()) {
    return;
  }

  FillOrPreviewDataModelForm(AutofillDriver::FORM_DATA_ACTION_FILL, query_id,
                             form, field, credit_card, 0, true);
}

void AutofillManager::OnDidPreviewAutofillFormData() {
  if (test_delegate_)
    test_delegate_->DidPreviewFormData();
}

void AutofillManager::OnDidFillAutofillFormData(const TimeTicks& timestamp) {
  if (test_delegate_)
    test_delegate_->DidFillFormData();

  AutofillMetrics::LogUserHappinessMetric(AutofillMetrics::USER_DID_AUTOFILL);
  if (!user_did_autofill_) {
    user_did_autofill_ = true;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_AUTOFILL_ONCE);
  }

  UpdateInitialInteractionTimestamp(timestamp);
}

void AutofillManager::DidShowSuggestions(bool is_new_popup,
                                         const FormData& form,
                                         const FormFieldData& field) {
  if (test_delegate_)
    test_delegate_->DidShowSuggestions();
  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  if (is_new_popup) {
    AutofillMetrics::LogUserHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);

    if (!did_show_suggestions_) {
      did_show_suggestions_ = true;
      AutofillMetrics::LogUserHappinessMetric(
          AutofillMetrics::SUGGESTIONS_SHOWN_ONCE);
    }

    if (autofill_field->Type().group() == CREDIT_CARD)
      credit_card_form_event_logger_->OnDidShowSuggestions();
    else
      address_form_event_logger_->OnDidShowSuggestions();
  }
}

void AutofillManager::OnHidePopup() {
  if (!IsAutofillEnabled())
    return;

  autocomplete_history_manager_->CancelPendingQuery();
  client_->HideAutofillPopup();
}

void AutofillManager::RemoveAutofillProfileOrCreditCard(int unique_id) {
  std::string guid;
  size_t variant = 0;
  const CreditCard* credit_card = nullptr;
  const AutofillProfile* profile = nullptr;
  if (GetCreditCard(unique_id, &credit_card)) {
    guid = credit_card->guid();
  } else if (GetProfile(unique_id, &profile, &variant)) {
    guid = profile->guid();
  } else {
    NOTREACHED();
    return;
  }

  // TODO(csharp): If we are dealing with a variant only the variant should
  // be deleted, instead of doing nothing.
  // http://crbug.com/124211
  if (variant != 0)
    return;

  personal_data_->RemoveByGUID(guid);
}

void AutofillManager::RemoveAutocompleteEntry(const base::string16& name,
                                              const base::string16& value) {
  autocomplete_history_manager_->OnRemoveAutocompleteEntry(name, value);
}

bool AutofillManager::IsShowingUnmaskPrompt() {
  return unmasking_card_.Compare(CreditCard()) != 0;
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
  FormStructure::ParseQueryResponse(response_xml, form_structures_.get());

  // Forward form structures to the password generation manager to detect
  // account creation forms.
  driver_->DetectAccountCreationForms(form_structures_.get());

  // If the corresponding flag is set, annotate forms with the predicted types.
  driver_->SendAutofillTypePredictionsToRenderer(form_structures_.get());
}

void AutofillManager::OnUnmaskResponse(const UnmaskResponse& response) {
  unmask_response_ = response;
  real_pan_client_.UnmaskCard(unmasking_card_, response);
}

void AutofillManager::OnUnmaskPromptClosed() {
  real_pan_client_.CancelRequest();
  driver_->RendererShouldClearPreviewedForm();
  unmasking_card_ = CreditCard();
  unmask_response_ = UnmaskResponse();
}

IdentityProvider* AutofillManager::GetIdentityProvider() {
  return client()->GetIdentityProvider();
}

void AutofillManager::OnDidGetRealPan(AutofillClient::GetRealPanResult result,
                                      const std::string& real_pan) {
  if (!real_pan.empty()) {
    DCHECK_EQ(AutofillClient::SUCCESS, result);
    credit_card_form_event_logger_->OnDidFillSuggestion(unmasking_card_);
    unmasking_card_.set_record_type(CreditCard::FULL_SERVER_CARD);
    unmasking_card_.SetNumber(base::UTF8ToUTF16(real_pan));
    if (unmask_response_.should_store_pan)
      personal_data_->UpdateServerCreditCard(unmasking_card_);

    FillCreditCardForm(unmasking_query_id_, unmasking_form_, unmasking_field_,
                       unmasking_card_);
  }

  client()->OnUnmaskVerificationResult(result);
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
  submitted_form->LogQualityMetrics(load_time, interaction_time,
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

  download_manager_->StartUploadRequest(submitted_form, was_autofilled,
                                        non_empty_types);
}

bool AutofillManager::UploadPasswordForm(
    const FormData& form,
    const ServerFieldType& password_type) {
  FormStructure form_structure(form);

  if (!ShouldUploadForm(form_structure))
    return false;

  if (!form_structure.ShouldBeCrowdsourced())
    return false;

  // Find the first password field to label. We don't try to label anything
  // else.
  bool found_password_field = false;
  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    AutofillField* field = form_structure.field(i);

    ServerFieldTypeSet types;
    if (!found_password_field && field->form_control_type == "password") {
      types.insert(password_type);
      found_password_field = true;
    } else {
      types.insert(UNKNOWN_TYPE);
    }
    field->set_possible_types(types);
  }
  DCHECK(found_password_field);

  // Only one field type should be present.
  ServerFieldTypeSet available_field_types;
  available_field_types.insert(password_type);

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
  address_form_event_logger_.reset(
      new AutofillMetrics::FormEventLogger(false /* is_for_credit_card */));
  credit_card_form_event_logger_.reset(
      new AutofillMetrics::FormEventLogger(true /* is_for_credit_card */));
  has_logged_autofill_enabled_ = false;
  has_logged_address_suggestions_count_ = false;
  did_show_suggestions_ = false;
  user_did_type_ = false;
  user_did_autofill_ = false;
  user_did_edit_autofilled_field_ = false;
  unmasking_card_ = CreditCard();
  unmasking_query_id_ = -1;
  unmasking_form_ = FormData();
  unmasking_field_ = FormFieldData();
  forms_loaded_timestamps_.clear();
  initial_interaction_timestamp_ = TimeTicks();
  external_delegate_->Reset();
}

AutofillManager::AutofillManager(AutofillDriver* driver,
                                 AutofillClient* client,
                                 PersonalDataManager* personal_data)
    : driver_(driver),
      client_(client),
      real_pan_client_(driver->GetURLRequestContext(), this),
      app_locale_("en-US"),
      personal_data_(personal_data),
      autocomplete_history_manager_(
          new AutocompleteHistoryManager(driver, client)),
      address_form_event_logger_(
          new AutofillMetrics::FormEventLogger(false /* is_for_credit_card */)),
      credit_card_form_event_logger_(
          new AutofillMetrics::FormEventLogger(true /* is_for_credit_card */)),
      has_logged_autofill_enabled_(false),
      has_logged_address_suggestions_count_(false),
      did_show_suggestions_(false),
      user_did_type_(false),
      user_did_autofill_(false),
      user_did_edit_autofilled_field_(false),
      unmasking_query_id_(-1),
      external_delegate_(NULL),
      test_delegate_(NULL),
      weak_ptr_factory_(this) {
  DCHECK(driver_);
  DCHECK(client_);
}

bool AutofillManager::RefreshDataModels() {
  if (!IsAutofillEnabled())
    return false;

  // No autofill data to return if the profiles are empty.
  const std::vector<AutofillProfile*>& profiles =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& credit_cards =
      personal_data_->GetCreditCards();

  // Updating the FormEventLoggers for addresses and credit cards.
  {
    bool is_server_data_available = false;
    bool is_local_data_available = false;
    for (CreditCard* credit_card : credit_cards) {
      if (credit_card->record_type() == CreditCard::LOCAL_CARD)
        is_local_data_available = true;
      else
        is_server_data_available = true;
    }
    credit_card_form_event_logger_->set_is_server_data_available(
        is_server_data_available);
    credit_card_form_event_logger_->set_is_local_data_available(
        is_local_data_available);
  }
  {
    bool is_server_data_available = false;
    bool is_local_data_available = false;
    for (AutofillProfile* profile : profiles) {
      if (profile->record_type() == AutofillProfile::LOCAL_PROFILE)
        is_local_data_available = true;
      else
        is_server_data_available = true;
    }
    address_form_event_logger_->set_is_server_data_available(
        is_server_data_available);
    address_form_event_logger_->set_is_local_data_available(
        is_local_data_available);
  }

  if (profiles.empty() && credit_cards.empty())
    return false;

  return true;
}

bool AutofillManager::IsCreditCard(int unique_id) {
  // Unpack the |unique_id| into component parts.
  SuggestionBackendID credit_card_id;
  SuggestionBackendID profile_id;
  SplitFrontendID(unique_id, &credit_card_id, &profile_id);
  DCHECK(!base::IsValidGUID(credit_card_id.guid) ||
         !base::IsValidGUID(profile_id.guid));
  return base::IsValidGUID(credit_card_id.guid);
}

bool AutofillManager::GetProfile(int unique_id,
                                 const AutofillProfile** profile,
                                 size_t* variant) {
  // Unpack the |unique_id| into component parts.
  SuggestionBackendID credit_card_id;
  SuggestionBackendID profile_id;
  SplitFrontendID(unique_id, &credit_card_id, &profile_id);
  *profile = NULL;
  if (base::IsValidGUID(profile_id.guid)) {
    *profile = personal_data_->GetProfileByGUID(profile_id.guid);
    *variant = profile_id.variant;
  }
  return !!*profile;
}

bool AutofillManager::GetCreditCard(int unique_id,
                                    const CreditCard** credit_card) {
  // Unpack the |unique_id| into component parts.
  SuggestionBackendID credit_card_id;
  SuggestionBackendID profile_id;
  SplitFrontendID(unique_id, &credit_card_id, &profile_id);
  *credit_card = NULL;
  if (base::IsValidGUID(credit_card_id.guid))
    *credit_card = personal_data_->GetCreditCardByGUID(credit_card_id.guid);
  return !!*credit_card;
}

void AutofillManager::FillOrPreviewDataModelForm(
    AutofillDriver::RendererFormDataAction action,
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const AutofillDataModel& data_model,
    size_t variant,
    bool is_credit_card) {
  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  DCHECK(form_structure);
  DCHECK(autofill_field);

  FormData result = form;

  base::string16 profile_full_name;
  std::string profile_language_code;
  if (!is_credit_card) {
    profile_full_name = data_model.GetInfo(
        AutofillType(NAME_FULL), app_locale_);
    profile_language_code =
        static_cast<const AutofillProfile*>(&data_model)->language_code();
  }

  // If the relevant section is auto-filled, we should fill |field| but not the
  // rest of the form.
  if (SectionIsAutofilled(*form_structure, form, autofill_field->section())) {
    for (std::vector<FormFieldData>::iterator iter = result.fields.begin();
         iter != result.fields.end(); ++iter) {
      if (iter->SameFieldAs(field)) {
        base::string16 value = data_model.GetInfoForVariant(
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

    // Note that this may invalidate |data_model|, particularly if it is a Mac
    // address book entry.
    if (action == AutofillDriver::FORM_DATA_ACTION_FILL)
      personal_data_->RecordUseOf(data_model);

    driver_->SendFormDataToRenderer(query_id, action, result);
    return;
  }

  // Cache the field type for the field from which the user initiated autofill.
  FieldTypeGroup initiating_group_type = autofill_field->Type().group();
  DCHECK_EQ(form_structure->field_count(), form.fields.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    if (form_structure->field(i)->section() != autofill_field->section())
      continue;

    DCHECK(form_structure->field(i)->SameFieldAs(result.fields[i]));

    const AutofillField* cached_field = form_structure->field(i);
    FieldTypeGroup field_group_type = cached_field->Type().group();

    if (field_group_type == NO_GROUP)
      continue;

    // If the field being filled is either
    //   (a) the field that the user initiated the fill from, or
    //   (b) part of the same logical unit, e.g. name or phone number,
    // then take the multi-profile "variant" into account.
    // Otherwise fill with the default (zeroth) variant.
    size_t use_variant = 0;
    if (result.fields[i].SameFieldAs(field) ||
        field_group_type == initiating_group_type) {
      use_variant = variant;
    }
    base::string16 value = data_model.GetInfoForVariant(
        cached_field->Type(), use_variant, app_locale_);
    if (is_credit_card &&
        cached_field->Type().GetStorableType() ==
            CREDIT_CARD_VERIFICATION_CODE) {
      // If this is |unmasking_card_|, |unmask_response_.cvc| should be
      // non-empty and vice versa.
      value = unmask_response_.cvc;
      DCHECK_EQ(&unmasking_card_ == &data_model, !value.empty());
    }

    // Must match ForEachMatchingFormField() in form_autofill_util.cc.
    // Only notify autofilling of empty fields and the field that initiated
    // the filling (note that "select-one" controls may not be empty but will
    // still be autofilled).
    bool should_notify =
        !is_credit_card &&
        !value.empty() &&
        (result.fields[i].SameFieldAs(field) ||
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

  autofilled_form_signatures_.push_front(form_structure->FormSignature());
  // Only remember the last few forms that we've seen, both to avoid false
  // positives and to avoid wasting memory.
  if (autofilled_form_signatures_.size() > kMaxRecentFormSignaturesToRemember)
    autofilled_form_signatures_.pop_back();

  // Note that this may invalidate |data_model|, particularly if it is a Mac
  // address book entry.
  if (action == AutofillDriver::FORM_DATA_ACTION_FILL)
    personal_data_->RecordUseOf(data_model);

  driver_->SendFormDataToRenderer(query_id, action, result);
}

scoped_ptr<FormStructure> AutofillManager::ValidateSubmittedForm(
    const FormData& form) {
  scoped_ptr<FormStructure> submitted_form(new FormStructure(form));
  if (!ShouldUploadForm(*submitted_form))
    return scoped_ptr<FormStructure>();

  // Ignore forms not present in our cache.  These are typically forms with
  // wonky JavaScript that also makes them not auto-fillable.
  FormStructure* cached_submitted_form;
  if (!FindCachedForm(form, &cached_submitted_form))
    return scoped_ptr<FormStructure>();

  submitted_form->UpdateFromCache(*cached_submitted_form);
  return submitted_form.Pass();
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
  for (AutofillField* current : **form_structure) {
    if (current->SameFieldAs(field)) {
      *autofill_field = current;
      break;
    }
  }

  // Even though we always update the cache, the field might not exist if the
  // website disables autocomplete while the user is interacting with the form.
  // See http://crbug.com/160476
  return *autofill_field != NULL;
}

AutofillField* AutofillManager::GetAutofillField(const FormData& form,
                                                 const FormFieldData& field) {
  if (!personal_data_)
    return NULL;

  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return NULL;

  if (!form_structure->IsAutofillable())
    return NULL;

  return autofill_field;
}

bool AutofillManager::UpdateCachedForm(const FormData& live_form,
                                       const FormStructure* cached_form,
                                       FormStructure** updated_form) {
  bool needs_update =
      (!cached_form ||
       live_form.fields.size() != cached_form->field_count());
  for (size_t i = 0; !needs_update && i < cached_form->field_count(); ++i)
    needs_update = !cached_form->field(i)->SameFieldAs(live_form.fields[i]);

  if (!needs_update)
    return true;

  if (form_structures_.size() >= kMaxFormCacheSize)
    return false;

  // Add the new or updated form to our cache.
  form_structures_.push_back(new FormStructure(live_form));
  *updated_form = *form_structures_.rbegin();
  (*updated_form)->DetermineHeuristicTypes();

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

std::vector<Suggestion> AutofillManager::GetProfileSuggestions(
    const FormStructure& form,
    const FormFieldData& field,
    const AutofillField& autofill_field) const {
  std::vector<ServerFieldType> field_types(form.field_count());
  for (size_t i = 0; i < form.field_count(); ++i) {
    field_types.push_back(form.field(i)->Type().GetStorableType());
  }

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      autofill_field.Type(), field.value, field.is_autofilled, field_types);

  // Adjust phone number to display in prefix/suffix case.
  if (autofill_field.Type().GetStorableType() == PHONE_HOME_NUMBER) {
    for (size_t i = 0; i < suggestions.size(); ++i) {
      suggestions[i].value = AutofillField::GetPhoneNumberValue(
          autofill_field, suggestions[i].value, field);
    }
  }

  for (size_t i = 0; i < suggestions.size(); ++i) {
    suggestions[i].frontend_id =
        MakeFrontendID(SuggestionBackendID(), suggestions[i].backend_id);
  }
  return suggestions;
}

std::vector<Suggestion> AutofillManager::GetCreditCardSuggestions(
    const FormFieldData& field,
    const AutofillType& type) const {
  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(type, field.value);
  for (size_t i = 0; i < suggestions.size(); i++) {
    suggestions[i].frontend_id =
        MakeFrontendID(suggestions[i].backend_id, SuggestionBackendID());
  }
  return suggestions;
}

void AutofillManager::ParseForms(const std::vector<FormData>& forms) {
  std::vector<FormStructure*> non_queryable_forms;
  for (std::vector<FormData>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    scoped_ptr<FormStructure> form_structure(new FormStructure(*iter));
    if (!form_structure->ShouldBeParsed())
      continue;

    form_structure->DetermineHeuristicTypes();

    if (form_structure->ShouldBeCrowdsourced())
      form_structures_.push_back(form_structure.release());
    else
      non_queryable_forms.push_back(form_structure.release());
  }

  if (!form_structures_.empty() && download_manager_) {
    // Query the server if at least one of the forms was parsed.
    download_manager_->StartQueryRequest(form_structures_.get());
  }

  for (std::vector<FormStructure*>::const_iterator iter =
           non_queryable_forms.begin();
       iter != non_queryable_forms.end(); ++iter) {
    form_structures_.push_back(*iter);
  }

  if (!form_structures_.empty())
    AutofillMetrics::LogUserHappinessMetric(AutofillMetrics::FORMS_LOADED);

  // For the |non_queryable_forms|, we have all the field type info we're ever
  // going to get about them.  For the other forms, we'll wait until we get a
  // response from the server.
  driver_->SendAutofillTypePredictionsToRenderer(non_queryable_forms);
}

int AutofillManager::BackendIDToInt(
    const SuggestionBackendID& backend_id) const {
  if (!base::IsValidGUID(backend_id.guid))
    return 0;

  const auto found = backend_to_int_map_.find(backend_id);
  if (found == backend_to_int_map_.end()) {
    // Unknown one, make a new entry.
    int int_id = backend_to_int_map_.size() + 1;
    backend_to_int_map_[backend_id] = int_id;
    int_to_backend_map_[int_id] = backend_id;
    return int_id;
  }
  return found->second;
}

SuggestionBackendID AutofillManager::IntToBackendID(int int_id) const {
  if (int_id == 0)
    return SuggestionBackendID();

  const auto found = int_to_backend_map_.find(int_id);
  if (found == int_to_backend_map_.end()) {
    NOTREACHED();
    return SuggestionBackendID();
  }
  return found->second;
}

// When sending IDs (across processes) to the renderer we pack credit card and
// profile IDs into a single integer.  Credit card IDs are sent in the high
// word and profile IDs are sent in the low word.
int AutofillManager::MakeFrontendID(
    const SuggestionBackendID& cc_backend_id,
    const SuggestionBackendID& profile_backend_id) const {
  int cc_int_id = BackendIDToInt(cc_backend_id);
  int profile_int_id = BackendIDToInt(profile_backend_id);

  // Should fit in signed 16-bit integers. We use 16-bits each when combining
  // below, and negative frontend IDs have special meaning so we can never use
  // the high bit.
  DCHECK(cc_int_id <= std::numeric_limits<int16_t>::max());
  DCHECK(profile_int_id <= std::numeric_limits<int16_t>::max());

  // Put CC in the high half of the bits.
  return (cc_int_id << std::numeric_limits<uint16_t>::digits) | profile_int_id;
}

// When receiving IDs (across processes) from the renderer we unpack credit card
// and profile IDs from a single integer.  Credit card IDs are stored in the
// high word and profile IDs are stored in the low word.
void AutofillManager::SplitFrontendID(
    int frontend_id,
    SuggestionBackendID* cc_backend_id,
    SuggestionBackendID* profile_backend_id) const {
  int cc_int_id = (frontend_id >> std::numeric_limits<uint16_t>::digits) &
      std::numeric_limits<uint16_t>::max();
  int profile_int_id = frontend_id & std::numeric_limits<uint16_t>::max();

  *cc_backend_id = IntToBackendID(cc_int_id);
  *profile_backend_id = IntToBackendID(profile_int_id);
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
