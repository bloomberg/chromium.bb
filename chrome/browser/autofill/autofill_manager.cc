// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_manager.h"

#include <limits>
#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/select_control_handler.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;
using webkit_glue::FormField;

namespace {

// We only send a fraction of the forms to upload server.
// The rate for positive/negative matches potentially could be different.
const double kAutoFillPositiveUploadRateDefaultValue = 0.01;
const double kAutoFillNegativeUploadRateDefaultValue = 0.01;

// Size and offset of the prefix and suffix portions of phone numbers.
const int kAutoFillPhoneNumberPrefixOffset = 0;
const int kAutoFillPhoneNumberPrefixCount = 3;
const int kAutoFillPhoneNumberSuffixOffset = 3;
const int kAutoFillPhoneNumberSuffixCount = 4;

const string16::value_type kCreditCardPrefix[] = {'*',0};
const string16::value_type kLabelSeparator[] = {';',' ','*',0};

// Combines the |label| string with the last four digits of the credit card
// |cc|.  If one, the other, or both are empty strings we omit the separator.
string16 CombineLabelAndCreditCard(const string16& label,
                                   const CreditCard* cc) {
  if (label.empty())
    return kCreditCardPrefix + cc->LastFourDigits();
  else if (cc->LastFourDigits().empty())
    return label;
  else
    return label + kLabelSeparator + cc->LastFourDigits();
}

// Removes duplicate elements whilst preserving original order of |elements| and
// |unique_ids|.
void RemoveDuplicateElements(
    std::vector<string16>* elements, std::vector<int>* unique_ids) {
  DCHECK_EQ(elements->size(), unique_ids->size());

  std::vector<string16> elements_copy;
  std::vector<int> unique_ids_copy;
  for (size_t i = 0; i < elements->size(); ++i) {
    const string16& element = (*elements)[i];

    bool unique = true;
    for (std::vector<string16>::const_iterator copy_iter
             = elements_copy.begin();
         copy_iter != elements_copy.end(); ++copy_iter) {
      if (element == *copy_iter) {
        unique = false;
        break;
      }
    }

    if (unique) {
      elements_copy.push_back(element);
      unique_ids_copy.push_back((*unique_ids)[i]);
    }
  }

  elements->assign(elements_copy.begin(), elements_copy.end());
  unique_ids->assign(unique_ids_copy.begin(), unique_ids_copy.end());
}

bool FormIsHTTPS(FormStructure* form) {
  return form->source_url().SchemeIs(chrome::kHttpsScheme);
}

}  // namespace

// TODO(jhawkins): Maybe this should be in a grd file?
const char* kAutoFillLearnMoreUrl =
    "http://www.google.com/support/chrome/bin/answer.py?answer=142893";

AutoFillManager::AutoFillManager(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      personal_data_(NULL),
      download_manager_(tab_contents_->profile()),
      disable_download_manager_requests_(false),
      cc_infobar_(NULL) {
  DCHECK(tab_contents);

  // |personal_data_| is NULL when using TestTabContents.
  personal_data_ =
      tab_contents_->profile()->GetOriginalProfile()->GetPersonalDataManager();
  download_manager_.SetObserver(this);
}

AutoFillManager::~AutoFillManager() {
  download_manager_.SetObserver(NULL);
}

// static
void AutoFillManager::RegisterBrowserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kAutoFillDialogPlacement);
}

// static
void AutoFillManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kAutoFillEnabled, true);
#if defined(OS_MACOSX)
  prefs->RegisterBooleanPref(prefs::kAutoFillAuxiliaryProfilesEnabled, true);
#else
  prefs->RegisterBooleanPref(prefs::kAutoFillAuxiliaryProfilesEnabled, false);
#endif
  prefs->RegisterRealPref(prefs::kAutoFillPositiveUploadRate,
                          kAutoFillPositiveUploadRateDefaultValue);
  prefs->RegisterRealPref(prefs::kAutoFillNegativeUploadRate,
                          kAutoFillNegativeUploadRateDefaultValue);
}

void AutoFillManager::FormSubmitted(const FormData& form) {
  if (!IsAutoFillEnabled())
    return;

  if (tab_contents_->profile()->IsOffTheRecord())
    return;

  // Don't save data that was submitted through JavaScript.
  if (!form.user_submitted)
    return;

  // Grab a copy of the form data.
  upload_form_structure_.reset(new FormStructure(form));

  if (!upload_form_structure_->IsAutoFillable())
    return;

  // Determine the possible field types and upload the form structure to the
  // PersonalDataManager.
  DeterminePossibleFieldTypes(upload_form_structure_.get());
  HandleSubmit();
}

void AutoFillManager::FormsSeen(const std::vector<FormData>& forms) {
  if (!IsAutoFillEnabled())
    return;

  ParseForms(forms);
}

bool AutoFillManager::GetAutoFillSuggestions(int query_id,
                                             bool form_autofilled,
                                             const FormField& field) {
  if (!IsAutoFillEnabled())
    return false;

  RenderViewHost* host = tab_contents_->render_view_host();
  if (!host)
    return false;

  if (personal_data_->profiles().empty() &&
      personal_data_->credit_cards().empty())
    return false;

  // Loops through the cached FormStructures looking for the FormStructure that
  // contains |field| and the associated AutoFillFieldType.
  FormStructure* form = NULL;
  AutoFillField* autofill_field = NULL;
  for (std::vector<FormStructure*>::iterator form_iter =
           form_structures_.begin();
       form_iter != form_structures_.end() && !autofill_field; ++form_iter) {
    form = *form_iter;

    // Don't send suggestions for forms that aren't auto-fillable.
    if (!form->IsAutoFillable())
      continue;

    for (std::vector<AutoFillField*>::const_iterator iter = form->begin();
         iter != form->end(); ++iter) {
      // The field list is terminated with a NULL AutoFillField, so don't try to
      // dereference it.
      if (!*iter)
        break;

      if ((**iter) == field) {
        autofill_field = *iter;
        break;
      }
    }
  }

  if (!autofill_field)
    return false;

  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  AutoFillType type(autofill_field->type());

  // If this form is non-HTTPS, treat billing address fields as regular address
  // fields.
  bool handle_billing = FormIsHTTPS(form);

  if (type.group() == AutoFillType::CREDIT_CARD)
    GetCreditCardSuggestions(
        form, field, type, &values, &labels, &icons, &unique_ids);
  else if (type.group() == AutoFillType::ADDRESS_BILLING)
    GetBillingProfileSuggestions(
        form, field, type, &values, &labels, &icons, &unique_ids);
  else
    GetProfileSuggestions(form, field, type, handle_billing,
                          &values, &labels, &icons, &unique_ids);

  DCHECK_EQ(values.size(), labels.size());
  DCHECK_EQ(values.size(), icons.size());
  DCHECK_EQ(values.size(), unique_ids.size());

  // No suggestions.
  if (values.empty())
    return false;

  // If the form is auto-filled and the renderer is querying for suggestions,
  // then the user is editing the value of a field.  In this case, don't display
  // labels, as that information is redundant. In addition, remove duplicate
  // values.
  if (form_autofilled) {
    RemoveDuplicateElements(&values, &unique_ids);
    labels.resize(values.size());
    icons.resize(values.size());

    for (size_t i = 0; i < labels.size(); ++i) {
      labels[i] = string16();
      icons[i] = string16();
    }
  }

  host->AutoFillSuggestionsReturned(
      query_id, values, labels, icons, unique_ids);
  return true;
}

bool AutoFillManager::FillAutoFillFormData(int query_id,
                                           const FormData& form,
                                           int unique_id) {
  if (!IsAutoFillEnabled())
    return false;

  RenderViewHost* host = tab_contents_->render_view_host();
  if (!host)
    return false;

  const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
  const std::vector<CreditCard*>& credit_cards = personal_data_->credit_cards();

  // No data to return if the profiles are empty.
  if (profiles.empty() && credit_cards.empty())
    return false;

  // Find the FormStructure that corresponds to |form|.
  FormData result = form;
  FormStructure* form_structure = NULL;
  for (std::vector<FormStructure*>::const_iterator iter =
           form_structures_.begin();
       iter != form_structures_.end(); ++iter) {
    if (**iter == form) {
      form_structure = *iter;
      break;
    }
  }

  if (!form_structure)
    return false;

  // No data to return if there are no auto-fillable fields.
  if (!form_structure->autofill_count())
    return false;

  // Unpack the |unique_id| into component parts.
  int cc_id = 0;
  int profile_id = 0;
  UnpackIDs(unique_id, &cc_id, &profile_id);

  // Find the profile that matches the |profile_id|, if one is specified.
  const AutoFillProfile* profile = NULL;
  if (profile_id != 0) {
    for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
         iter != profiles.end(); ++iter) {
      if ((*iter)->unique_id() == profile_id) {
        profile = *iter;
        break;
      }
    }
  }

  // Find the credit card that matches the |cc_id|, if one is specified.
  const CreditCard* credit_card = NULL;
  if (cc_id != 0) {
    for (std::vector<CreditCard*>::const_iterator iter = credit_cards.begin();
         iter != credit_cards.end(); ++iter) {
      if ((*iter)->unique_id() == cc_id) {
        credit_card = *iter;
        break;
      }
    }
  }

  if (!profile && !credit_card)
    return false;

  // The list of fields in |form_structure| and |result.fields| often match
  // directly and we can fill these corresponding fields; however, when the
  // |form_structure| and |result.fields| do not match directly we search
  // ahead in the |form_structure| for the matching field.
  // See unit tests: AutoFillManagerTest.FormChangesRemoveField and
  // AutoFillManagerTest.FormChangesAddField for usage.
  for (size_t i = 0, j = 0;
       i < form_structure->field_count() && j < result.fields.size();
       j++) {
    size_t k = i;

    // Search forward in the |form_structure| for a corresponding field.
    while (k < form_structure->field_count() &&
           *form_structure->field(k) != result.fields[j]) {
      k++;
    }

    // If we've found a match then fill the |result| field with the found
    // field in the |form_structure|.
    if (k >= form_structure->field_count())
      continue;

    const AutoFillField* field = form_structure->field(k);
    AutoFillType autofill_type(field->type());
    if (credit_card &&
        autofill_type.group() == AutoFillType::CREDIT_CARD) {
      FillCreditCardFormField(credit_card, autofill_type, &result.fields[j]);
    } else if (credit_card &&
               autofill_type.group() == AutoFillType::ADDRESS_BILLING) {
      FillBillingFormField(credit_card, autofill_type, &result.fields[j]);
    } else if (profile) {
      FillFormField(profile, autofill_type, &result.fields[j]);
    }

    // We found a matching field in the |form_structure| so we
    // proceed to the next |result| field, and the next |form_structure|.
    ++i;
  }
  autofilled_forms_signatures_.push_front(form_structure->FormSignature());

  host->AutoFillFormDataFilled(query_id, result);
  return true;
}

void AutoFillManager::ShowAutoFillDialog() {
  ::ShowAutoFillDialog(tab_contents_->GetContentNativeView(),
                       personal_data_,
                       tab_contents_->profile()->GetOriginalProfile());
}

void AutoFillManager::Reset() {
  upload_form_structure_.reset();
  form_structures_.reset();
}

void AutoFillManager::OnLoadedAutoFillHeuristics(
    const std::string& heuristic_xml) {
  // TODO(jhawkins): Store |upload_required| in the AutoFillManager.
  UploadRequired upload_required;
  FormStructure::ParseQueryResponse(heuristic_xml,
                                    form_structures_.get(),
                                    &upload_required);
}

void AutoFillManager::OnUploadedAutoFillHeuristics(
    const std::string& form_signature) {
}

void AutoFillManager::OnHeuristicsRequestError(
    const std::string& form_signature,
    AutoFillDownloadManager::AutoFillRequestType request_type,
    int http_error) {
}

bool AutoFillManager::IsAutoFillEnabled() const {
  PrefService* prefs = tab_contents_->profile()->GetPrefs();

  // Migrate obsolete AutoFill pref.
  if (prefs->FindPreference(prefs::kFormAutofillEnabled)) {
    bool enabled = prefs->GetBoolean(prefs::kFormAutofillEnabled);
    prefs->ClearPref(prefs::kFormAutofillEnabled);
    prefs->SetBoolean(prefs::kAutoFillEnabled, enabled);
    return enabled;
  }

  return prefs->GetBoolean(prefs::kAutoFillEnabled);
}

void AutoFillManager::DeterminePossibleFieldTypes(
    FormStructure* form_structure) {
  for (size_t i = 0; i < form_structure->field_count(); i++) {
    const AutoFillField* field = form_structure->field(i);
    FieldTypeSet field_types;
    personal_data_->GetPossibleFieldTypes(field->value(), &field_types);
    form_structure->set_possible_types(i, field_types);
  }
}

void AutoFillManager::HandleSubmit() {
  // If there wasn't enough data to import then we don't want to send an upload
  // to the server.
  // TODO(jhawkins): Import form data from |form_structures_|.  That will
  // require querying the FormManager for updated field values.
  std::vector<FormStructure*> import;
  import.push_back(upload_form_structure_.get());
  if (!personal_data_->ImportFormData(import, this))
    return;

  // Did we get credit card info?
  AutoFillProfile* profile;
  CreditCard* credit_card;
  personal_data_->GetImportedFormData(&profile, &credit_card);

  if (!credit_card) {
    UploadFormData();
    return;
  }

  // Show an infobar to offer to save the credit card info.
  if (tab_contents_) {
    tab_contents_->AddInfoBar(new AutoFillCCInfoBarDelegate(tab_contents_,
                                                            this));
  }
}

void AutoFillManager::UploadFormData() {
  if (!disable_download_manager_requests_ && upload_form_structure_.get()) {
    bool was_autofilled = false;
    // Check if the form among last 3 forms that were auto-filled.
    // Clear older signatures.
    std::list<std::string>::iterator it;
    int total_form_checked = 0;
    for (it = autofilled_forms_signatures_.begin();
         it != autofilled_forms_signatures_.end() && total_form_checked < 3;
         ++it, ++total_form_checked) {
      if (*it == upload_form_structure_->FormSignature())
        was_autofilled = true;
    }
    // Remove outdated form signatures.
    if (total_form_checked == 3 && it != autofilled_forms_signatures_.end()) {
      autofilled_forms_signatures_.erase(it,
                                         autofilled_forms_signatures_.end());
    }
    download_manager_.StartUploadRequest(*(upload_form_structure_.get()),
                                         was_autofilled);
  }
}

void AutoFillManager::OnInfoBarClosed(bool should_save) {
  if (should_save)
    personal_data_->SaveImportedCreditCard();
  UploadFormData();
}

AutoFillManager::AutoFillManager()
    : tab_contents_(NULL),
      personal_data_(NULL),
      download_manager_(NULL),
      disable_download_manager_requests_(false),
      cc_infobar_(NULL) {
}

AutoFillManager::AutoFillManager(TabContents* tab_contents,
                                 PersonalDataManager* personal_data)
    : tab_contents_(tab_contents),
      personal_data_(personal_data),
      download_manager_(NULL),
      disable_download_manager_requests_(false),
      cc_infobar_(NULL) {
  DCHECK(tab_contents);
}

void AutoFillManager::GetProfileSuggestions(FormStructure* form,
                                            const FormField& field,
                                            AutoFillType type,
                                            bool include_cc_labels,
                                            std::vector<string16>* values,
                                            std::vector<string16>* labels,
                                            std::vector<string16>* icons,
                                            std::vector<int>* unique_ids) {
  const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
  std::vector<AutoFillProfile*> matched_profiles;
  for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    AutoFillProfile* profile = *iter;

    // The value of the stored data for this field type in the |profile|.
    string16 profile_field_value = profile->GetFieldText(type);

    if (!profile_field_value.empty() &&
        StartsWith(profile_field_value, field.value(), false)) {
      matched_profiles.push_back(profile);
      values->push_back(profile_field_value);
      unique_ids->push_back(PackIDs(0, profile->unique_id()));
    }
  }

  AutoFillProfile::CreateInferredLabels(&matched_profiles, labels, 0,
                                        type.field_type());

  if (!include_cc_labels || !form->HasBillingFields() || !FormIsHTTPS(form)) {
    icons->resize(values->size());  // No CC, so no icons.
    return;
  }

  size_t i = 0;
  std::vector<string16> expanded_values;
  std::vector<string16> expanded_labels;
  std::vector<int> expanded_ids;
  for (std::vector<AutoFillProfile*>::const_iterator iter =
       matched_profiles.begin(); iter != matched_profiles.end();
       ++iter, ++i) {
    AutoFillProfile* profile = *iter;
    for (std::vector<CreditCard*>::const_iterator cc =
         personal_data_->credit_cards().begin();
         cc != personal_data_->credit_cards().end(); ++cc) {
      expanded_values.push_back((*values)[i]);
      string16 label = CombineLabelAndCreditCard((*labels)[i], *cc);
      expanded_labels.push_back(label);
      icons->push_back((*cc)->type());
      expanded_ids.push_back(PackIDs((*cc)->unique_id(), profile->unique_id()));
    }
  }

  expanded_labels.swap(*labels);
  expanded_values.swap(*values);
  expanded_ids.swap(*unique_ids);
}

void AutoFillManager::GetBillingProfileSuggestions(
    FormStructure* form,
    const FormField& field,
    AutoFillType type,
    std::vector<string16>* values,
    std::vector<string16>* labels,
    std::vector<string16>* icons,
    std::vector<int>* unique_ids) {

  // If the form is non-HTTPS, no CC suggestions are provided; however, give the
  // user the option of filling the billing address fields with regular address
  // data.
  if (!FormIsHTTPS(form)) {
    GetProfileSuggestions(
        form, field, type, false, values, icons, labels, unique_ids);
    return;
  }

  std::vector<CreditCard*> matching_creditcards;
  std::vector<AutoFillProfile*> matching_profiles;

  // Collect matching pairs of credit cards and related profiles, where profile
  // field value matches form field value.
  for (std::vector<CreditCard*>::const_iterator cc =
           personal_data_->credit_cards().begin();
       cc != personal_data_->credit_cards().end(); ++cc) {
    int billing_address_id = (*cc)->billing_address_id();
    AutoFillProfile* billing_profile = NULL;

    // The value of the stored data for this field type in the |profile|.
    string16 profile_field_value;

    for (std::vector<AutoFillProfile*>::const_iterator iter =
             personal_data_->profiles().begin();
         iter != personal_data_->profiles().end(); ++iter) {
      AutoFillProfile* profile = *iter;

      // This assumes that labels are unique.
      if (profile->unique_id() == billing_address_id &&
          !profile->GetFieldText(type).empty() &&
          StartsWith(profile->GetFieldText(type), field.value(), false)) {
        billing_profile = profile;
        break;
      }
    }

    if (!billing_profile)
      continue;

    matching_creditcards.push_back(*cc);
    matching_profiles.push_back(billing_profile);
  }

  std::vector<string16> inferred_labels;
  AutoFillProfile::CreateInferredLabels(&matching_profiles, &inferred_labels, 0,
                                        type.field_type());

  DCHECK_EQ(matching_profiles.size(), matching_creditcards.size());
  DCHECK_EQ(matching_profiles.size(), inferred_labels.size());

  // Process the matching pairs into suggested |values|, |labels|, and
  // |unique_ids|.
  for (size_t i = 0; i < matching_profiles.size(); ++i) {
    values->push_back(matching_profiles[i]->GetFieldText(type));
    string16 label = CombineLabelAndCreditCard(inferred_labels[i],
                                               matching_creditcards[i]);
    labels->push_back(label);
    icons->push_back(matching_creditcards[i]->type());
    unique_ids->push_back(PackIDs(matching_creditcards[i]->unique_id(),
                                  matching_profiles[i]->unique_id()));
  }
}

void AutoFillManager::GetCreditCardSuggestions(FormStructure* form,
                                               const FormField& field,
                                               AutoFillType type,
                                               std::vector<string16>* values,
                                               std::vector<string16>* labels,
                                               std::vector<string16>* icons,
                                               std::vector<int>* unique_ids) {
  // Don't return CC suggestions for non-HTTPS pages.
  if (!FormIsHTTPS(form))
    return;

  for (std::vector<CreditCard*>::const_iterator iter =
           personal_data_->credit_cards().begin();
       iter != personal_data_->credit_cards().end(); ++iter) {
    CreditCard* credit_card = *iter;

    // The value of the stored data for this field type in the |credit_card|.
    string16 creditcard_field_value = credit_card->GetFieldText(type);
    if (!creditcard_field_value.empty() &&
        StartsWith(creditcard_field_value, field.value(), false)) {
      if (type.field_type() == CREDIT_CARD_NUMBER)
        creditcard_field_value = credit_card->ObfuscatedNumber();

      if (!form->HasNonBillingFields()) {
        values->push_back(creditcard_field_value);
        labels->push_back(CombineLabelAndCreditCard(string16(), credit_card));
        icons->push_back(credit_card->type());
        unique_ids->push_back(PackIDs(credit_card->unique_id(), 0));
      } else {
        const std::vector<AutoFillProfile*>& profiles
            = personal_data_->profiles();
        std::vector<string16> inferred_labels;
        AutoFillProfile::CreateInferredLabels(&profiles,
                                              &inferred_labels,
                                              0,
                                              type.field_type());
        DCHECK_EQ(profiles.size(), inferred_labels.size());

        for (size_t i = 0; i < profiles.size(); ++i) {
          values->push_back(creditcard_field_value);

          string16 label = CombineLabelAndCreditCard(inferred_labels[i],
                                                     credit_card);
          labels->push_back(label);
          icons->push_back(credit_card->type());
          unique_ids->push_back(
              PackIDs(credit_card->unique_id(), profiles[i]->unique_id()));
        }
      }
    }
  }
}

void AutoFillManager::FillBillingFormField(const CreditCard* credit_card,
                                           AutoFillType type,
                                           webkit_glue::FormField* field) {
  DCHECK(credit_card);
  DCHECK(type.group() == AutoFillType::ADDRESS_BILLING);
  DCHECK(field);

  int billing_address_id = credit_card->billing_address_id();
  if (billing_address_id != 0) {
    AutoFillProfile* profile = NULL;
    const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
    for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
         iter != profiles.end(); ++iter) {
      if ((*iter)->unique_id() == billing_address_id) {
        profile = *iter;
        break;
      }
    }

    if (profile) {
      FillFormField(profile, type, field);
    }
  }
}

void AutoFillManager::FillCreditCardFormField(const CreditCard* credit_card,
                                              AutoFillType type,
                                              webkit_glue::FormField* field) {
  DCHECK(credit_card);
  DCHECK(field);

  if (field->form_control_type() == ASCIIToUTF16("select-one"))
    autofill::FillSelectControl(credit_card, type, field);
  else
    field->set_value(credit_card->GetFieldText(type));
}

void AutoFillManager::FillFormField(const AutoFillProfile* profile,
                                    AutoFillType type,
                                    webkit_glue::FormField* field) {
  DCHECK(profile);
  DCHECK(field);

  if (type.subgroup() == AutoFillType::PHONE_NUMBER) {
    FillPhoneNumberField(profile, field);
  } else {
    if (field->form_control_type() == ASCIIToUTF16("select-one"))
      autofill::FillSelectControl(profile, type, field);
    else
      field->set_value(profile->GetFieldText(type));
  }
}

void AutoFillManager::FillPhoneNumberField(const AutoFillProfile* profile,
                                           webkit_glue::FormField* field) {
  // If we are filling a phone number, check to see if the size field
  // matches the "prefix" or "suffix" sizes and fill accordingly.
  string16 number = profile->GetFieldText(AutoFillType(PHONE_HOME_NUMBER));
  bool has_valid_suffix_and_prefix = (number.length() ==
      (kAutoFillPhoneNumberPrefixCount + kAutoFillPhoneNumberSuffixCount));
  if (has_valid_suffix_and_prefix &&
      field->size() == kAutoFillPhoneNumberPrefixCount) {
    number = number.substr(kAutoFillPhoneNumberPrefixOffset,
                           kAutoFillPhoneNumberPrefixCount);
    field->set_value(number);
  } else if (has_valid_suffix_and_prefix &&
             field->size() == kAutoFillPhoneNumberSuffixCount) {
    number = number.substr(kAutoFillPhoneNumberSuffixOffset,
                           kAutoFillPhoneNumberSuffixCount);
    field->set_value(number);
  } else {
    field->set_value(number);
  }
}

void AutoFillManager::ParseForms(
    const std::vector<webkit_glue::FormData>& forms) {
  for (std::vector<FormData>::const_iterator iter =
           forms.begin();
       iter != forms.end(); ++iter) {
    scoped_ptr<FormStructure> form_structure(new FormStructure(*iter));
    if (!form_structure->ShouldBeParsed())
      continue;

    DeterminePossibleFieldTypes(form_structure.get());
    form_structures_.push_back(form_structure.release());
  }

  // If none of the forms were parsed, no use querying the server.
  if (!form_structures_.empty() && !disable_download_manager_requests_)
    download_manager_.StartQueryRequest(form_structures_);
}

// When sending IDs (across processes) to the renderer we pack credit card and
// profile IDs into a single integer.  Credit card IDs are sent in the high
// word and profile IDs are sent in the low word.
// static
int AutoFillManager::PackIDs(int cc_id, int profile_id) {
  DCHECK(cc_id <= std::numeric_limits<unsigned short>::max());
  DCHECK(profile_id <= std::numeric_limits<unsigned short>::max());

  return cc_id << std::numeric_limits<unsigned short>::digits | profile_id;
}

// When receiving IDs (across processes) from the renderer we unpack credit card
// and profile IDs from a single integer.  Credit card IDs are stored in the
// high word and profile IDs are stored in the low word.
// static
void AutoFillManager::UnpackIDs(int id, int* cc_id, int* profile_id) {
  *cc_id = id >> std::numeric_limits<unsigned short>::digits &
      std::numeric_limits<unsigned short>::max();
  *profile_id = id & std::numeric_limits<unsigned short>::max();
}
