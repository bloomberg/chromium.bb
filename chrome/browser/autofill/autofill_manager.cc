// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_manager.h"

#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/pref_service.h"
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

const string16::value_type kLabelSeparator[] = {';',' ',0};

// Removes duplicate elements whilst preserving original order of |elements| and
// |unique_ids|.
void RemoveDuplicateElements(
    std::vector<string16>* elements, std::vector<int>* unique_ids) {
  std::vector<string16> copy;
  for (size_t i = 0; i < elements->size(); ++i) {
    const string16& element = (*elements)[i];

    bool unique = true;
    for (std::vector<string16>::const_iterator copy_iter = copy.begin();
         copy_iter != copy.end(); ++copy_iter) {
      if (element == *copy_iter) {
        unique = false;
        break;
      }
    }

    if (unique)
      copy.push_back(element);
    else
      unique_ids->erase(unique_ids->begin() + i);
  }

  elements->assign(copy.begin(), copy.end());
}

bool FormIsHTTPS(FormStructure* form) {
  return form->ConvertToFormData().origin.SchemeIs(chrome::kHttpsScheme);
}

}  // namespace

// TODO(jhawkins): Maybe this should be in a grd file?
const char* kAutoFillLearnMoreUrl =
    "http://www.google.com/support/chrome/bin/answer.py?answer=142893";

AutoFillManager::AutoFillManager(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      personal_data_(NULL),
      download_manager_(tab_contents_->profile()) {
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

  // No profiles or credit cards, no need to parse the forms.
  if (personal_data_->profiles().empty() &&
      personal_data_->credit_cards().empty())
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
       form_iter != form_structures_.end(); ++form_iter) {
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

  if (autofill_field == NULL)
    return false;

  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<int> unique_ids;
  AutoFillType type(autofill_field->type());

  // If this form is non-HTTPS, treat billing address fields as regular address
  // fields.
  bool handle_billing = FormIsHTTPS(form);

  if (type.group() == AutoFillType::CREDIT_CARD)
    GetCreditCardSuggestions(form, field, type, &values, &labels, &unique_ids);
  else if (type.group() == AutoFillType::ADDRESS_BILLING)
    GetBillingProfileSuggestions(
        form, field, type, &values, &labels, &unique_ids);
  else
    GetProfileSuggestions(
        form, field, type, handle_billing, &values, &labels, &unique_ids);

  DCHECK_EQ(values.size(), labels.size());
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

    for (size_t i = 0; i < labels.size(); ++i)
      labels[i] = string16();
  }

  host->AutoFillSuggestionsReturned(query_id, values, labels, unique_ids);
  return true;
}

// TODO(jhawkins): Remove the |value| parameter.
bool AutoFillManager::FillAutoFillFormData(int query_id,
                                           const FormData& form,
                                           const string16& value,
                                           const string16& label,
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

  // |cc_digits| will contain the last four digits of a credit card number only
  // if the form has billing fields.
  string16 cc_digits;

  // If the form has billing fields, |label| will contain at least one "; "
  // followed by the last four digits of a credit card number.
  if (form_structure->HasBillingFields()) {
    // We must search for the last "; " as it's possible the profile label
    // proper can contain this sequence of characters.
    size_t index = label.find_last_of(kLabelSeparator);
    if (index != string16::npos) {
      size_t cc_index = index + 1;
      cc_digits = label.substr(cc_index);
    }
  }

  // Find the profile that matches the |unique_id|.
  const AutoFillProfile* profile = NULL;
  for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    if ((*iter)->unique_id() == unique_id) {
      profile = *iter;
    }
  }

  // Don't look for a matching credit card if we fully-matched the profile using
  // the entire label.
  const CreditCard* credit_card = NULL;
  if (!cc_digits.empty()) {
    // Find the credit card that matches the |cc_label|.
    for (std::vector<CreditCard*>::const_iterator iter = credit_cards.begin();
         iter != credit_cards.end(); ++iter) {
      if ((*iter)->LastFourDigits() == cc_digits) {
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
      result.fields[i].set_value(
          credit_card->GetFieldText(autofill_type));
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

  if (credit_card) {
    cc_infobar_.reset(new AutoFillCCInfoBarDelegate(tab_contents_, this));
  } else {
    UploadFormData();
  }
}

void AutoFillManager::UploadFormData() {
  // TODO(georgey): enable upload request when we make sure that our data is in
  // line with toolbar data:
  // download_manager_.StartUploadRequest(upload_form_structure_,
  //                                      form_is_autofilled);
}

void AutoFillManager::OnInfoBarClosed(bool should_save) {
  if (should_save)
    personal_data_->SaveImportedCreditCard();
  UploadFormData();
}

AutoFillManager::AutoFillManager()
    : tab_contents_(NULL),
      personal_data_(NULL),
      download_manager_(NULL) {
}

AutoFillManager::AutoFillManager(TabContents* tab_contents,
                                 PersonalDataManager* personal_data)
    : tab_contents_(tab_contents),
      personal_data_(personal_data),
      download_manager_(NULL) {
  DCHECK(tab_contents);
}

void AutoFillManager::GetProfileSuggestions(FormStructure* form,
                                            const FormField& field,
                                            AutoFillType type,
                                            bool include_cc_labels,
                                            std::vector<string16>* values,
                                            std::vector<string16>* labels,
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
      unique_ids->push_back(profile->unique_id());
    }
  }

  AutoFillProfile::CreateInferredLabels(&matched_profiles, labels, 0,
                                        type.field_type());

  if (!include_cc_labels || !form->HasBillingFields() || !FormIsHTTPS(form))
    return;

  size_t i = 0;
  std::vector<string16> expanded_values;
  std::vector<string16> expanded_labels;
  for (std::vector<AutoFillProfile*>::const_iterator iter =
       matched_profiles.begin(); iter != matched_profiles.end();
       ++iter, ++i) {
    AutoFillProfile* profile = *iter;
    for (std::vector<CreditCard*>::const_iterator cc =
         personal_data_->credit_cards().begin();
         cc != personal_data_->credit_cards().end(); ++cc) {
      expanded_values.push_back((*values)[i]);
      string16 label = (*labels)[i] + kLabelSeparator +
          (*cc)->LastFourDigits();
      expanded_labels.push_back(label);
      unique_ids->push_back(profile->unique_id());
    }
  }
  expanded_labels.swap(*labels);
  expanded_values.swap(*values);
}

void AutoFillManager::GetBillingProfileSuggestions(
    FormStructure* form,
    const FormField& field,
    AutoFillType type,
    std::vector<string16>* values,
    std::vector<string16>* labels,
    std::vector<int>* unique_ids) {
  std::vector<CreditCard*> matching_creditcards;
  std::vector<AutoFillProfile*> matching_profiles;
  std::vector<string16> cc_values;
  std::vector<string16> cc_labels;

  // If the form is non-HTTPS, no CC suggestions are provided; however, give the
  // user the option of filling the billing address fields with regular address
  // data.
  if (!FormIsHTTPS(form)) {
    GetProfileSuggestions(form, field, type, false, values, labels, unique_ids);
    return;
  }

  for (std::vector<CreditCard*>::const_iterator cc =
           personal_data_->credit_cards().begin();
       cc != personal_data_->credit_cards().end(); ++cc) {
    string16 label = (*cc)->billing_address();
    AutoFillProfile* billing_profile = NULL;

    // The value of the stored data for this field type in the |profile|.
    string16 profile_field_value;

    for (std::vector<AutoFillProfile*>::const_iterator iter =
             personal_data_->profiles().begin();
         iter != personal_data_->profiles().end(); ++iter) {
      AutoFillProfile* profile = *iter;

      // This assumes that labels are unique.
      if (profile->Label() == label &&
          !profile->GetFieldText(type).empty() &&
          StartsWith(profile->GetFieldText(type), field.value(), false)) {
        billing_profile = profile;
        break;
      }
    }

    if (!billing_profile)
      continue;

    for (std::vector<AutoFillProfile*>::const_iterator iter =
             personal_data_->profiles().begin();
         iter != personal_data_->profiles().end(); ++iter) {
      values->push_back(billing_profile->GetFieldText(type));

      string16 label = (*iter)->Label() +
                       ASCIIToUTF16("; ") +
                       (*cc)->LastFourDigits();
      labels->push_back(label);
      unique_ids->push_back((*iter)->unique_id());
    }
  }
}

void AutoFillManager::GetCreditCardSuggestions(FormStructure* form,
                                               const FormField& field,
                                               AutoFillType type,
                                               std::vector<string16>* values,
                                               std::vector<string16>* labels,
                                               std::vector<int>* unique_ids) {
  // Don't return CC suggestions for non-HTTPS pages.
  if (!FormIsHTTPS(form))
    return;

  for (std::vector<CreditCard*>::const_iterator iter =
           personal_data_->credit_cards().begin();
       iter != personal_data_->credit_cards().end(); ++iter) {
    CreditCard* credit_card = *iter;

    // The value of the stored data for this field type in the |credit_card|.
    string16 creditcard_field_value =
        credit_card->GetFieldText(type);
    if (!creditcard_field_value.empty() &&
        StartsWith(creditcard_field_value, field.value(), false)) {
      if (type.field_type() == CREDIT_CARD_NUMBER)
        creditcard_field_value = credit_card->ObfuscatedNumber();

      if (!form->HasNonBillingFields()) {
        values->push_back(creditcard_field_value);
        labels->push_back(credit_card->Label());
        unique_ids->push_back(credit_card->unique_id());
      } else {
        for (std::vector<AutoFillProfile*>::const_iterator iter =
                 personal_data_->profiles().begin();
             iter != personal_data_->profiles().end(); ++iter) {
          values->push_back(creditcard_field_value);

          string16 label = (*iter)->Label() +
                           ASCIIToUTF16("; ") +
                           credit_card->LastFourDigits();
          labels->push_back(label);
          unique_ids->push_back((*iter)->unique_id());
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

  string16 billing_address = credit_card->billing_address();
  if (!billing_address.empty()) {
    AutoFillProfile* profile = NULL;
    const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
    for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
         iter != profiles.end(); ++iter) {
      if ((*iter)->Label() == billing_address) {
        profile = *iter;
        break;
      }
    }

    if (profile) {
      FillFormField(profile, type, field);
    }
  }
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
      FillSelectOneField(profile, type, field);
    else
      field->set_value(profile->GetFieldText(type));
  }
}

void AutoFillManager::FillSelectOneField(const AutoFillProfile* profile,
                                         AutoFillType type,
                                         webkit_glue::FormField* field) {
  DCHECK(profile);
  DCHECK(field);
  DCHECK(field->form_control_type() == ASCIIToUTF16("select-one"));
  string16 selected_string = profile->GetFieldText(type);
  std::string ascii_value = UTF16ToASCII(selected_string);
  for (size_t i = 0; i < field->option_strings().size(); ++i) {
    if (profile->GetFieldText(type) == field->option_strings()[i]) {
      // An exact match - use it.
      selected_string = profile->GetFieldText(type);
      break;
    }
    if (!base::strcasecmp(UTF16ToASCII(field->option_strings()[i]).c_str(),
                          ascii_value.c_str())) {
      // A match, but not in the same case - save it for the case we won't
      // find an exact match.
      selected_string = field->option_strings()[i];
    }
  }
  field->set_value(selected_string);
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
    FormStructure* form_structure = new FormStructure(*iter);
    if (!form_structure->ShouldBeParsed())
      continue;

    DeterminePossibleFieldTypes(form_structure);
    form_structures_.push_back(form_structure);
  }

  // If none of the forms were parsed, no use querying the server.
  if (!form_structures_.empty())
    download_manager_.StartQueryRequest(form_structures_);
}
