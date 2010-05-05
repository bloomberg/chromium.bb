// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_manager.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_infobar_delegate.h"
#include "chrome/browser/autofill/autofill_xml_parser.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;
using webkit_glue::FormField;

namespace {
// We only send a fraction of the forms to upload server.
// The rate for positive/negative matches potentially could be different.
const double kAutoFillPositiveUploadRateDefaultValue = 0.01;
const double kAutoFillNegativeUploadRateDefaultValue = 0.01;
}  // namespace

// TODO(jhawkins): Maybe this should be in a grd file?
const char* kAutoFillLearnMoreUrl =
    "http://www.google.com/support/chrome/bin/answer.py?answer=142893";

AutoFillManager::AutoFillManager(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      personal_data_(NULL),
      download_manager_(tab_contents_->profile()),
      infobar_(NULL) {
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
  prefs->RegisterBooleanPref(prefs::kAutoFillInfoBarShown, false);
  prefs->RegisterBooleanPref(prefs::kAutoFillEnabled, true);
  prefs->RegisterBooleanPref(prefs::kAutoFillAuxiliaryProfilesEnabled, false);
  prefs->RegisterStringPref(prefs::kAutoFillDefaultProfile, std::wstring());
  prefs->RegisterStringPref(prefs::kAutoFillDefaultCreditCard, std::wstring());

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

  if (upload_form_structure_->HasAutoFillableValues()) {
    PrefService* prefs = tab_contents_->profile()->GetPrefs();
    bool infobar_shown = prefs->GetBoolean(prefs::kAutoFillInfoBarShown);
    if (!infobar_shown) {
      // Ask the user for permission to save form information.
      infobar_.reset(new AutoFillInfoBarDelegate(tab_contents_, this));
    }
  }
}

void AutoFillManager::FormsSeen(const std::vector<FormData>& forms) {
  if (!IsAutoFillEnabled())
    return;

  for (std::vector<FormData>::const_iterator iter =
           forms.begin();
       iter != forms.end(); ++iter) {
    FormStructure* form_structure = new FormStructure(*iter);
    DeterminePossibleFieldTypes(form_structure);
    form_structures_.push_back(form_structure);
  }
  download_manager_.StartQueryRequest(form_structures_);
}

bool AutoFillManager::GetAutoFillSuggestions(int query_id,
                                             const FormField& field) {
  if (!IsAutoFillEnabled())
    return false;

  RenderViewHost* host = tab_contents_->render_view_host();
  if (!host)
    return false;

  if (personal_data_->profiles().empty() &&
      personal_data_->credit_cards().empty())
    return false;

  AutoFillField* form_field = NULL;
  for (std::vector<FormStructure*>::iterator form = form_structures_.begin();
       form != form_structures_.end(); ++form) {
    // Don't send suggestions for forms that aren't auto-fillable.
    if (!(*form)->IsAutoFillable())
      continue;

    for (std::vector<AutoFillField*>::const_iterator iter = (*form)->begin();
         iter != (*form)->end(); ++iter) {
      // The field list is terminated with a NULL AutoFillField, so don't try to
      // dereference it.
      if (!*iter)
        break;

      if ((**iter) == field) {
        form_field = *iter;
        break;
      }
    }
  }

  if (form_field == NULL)
    return false;

  std::vector<string16> values;
  std::vector<string16> labels;

  if (AutoFillType(form_field->type()).group() != AutoFillType::CREDIT_CARD)
    GetProfileSuggestions(field, form_field->type(), &values, &labels);
  else
    GetCreditCardSuggestions(field, form_field->type(), &values, &labels);

  // No suggestions.
  if (values.empty())
    return false;

  // TODO(jhawkins): If the default profile is in this list, set it as the
  // default suggestion index.
  host->AutoFillSuggestionsReturned(query_id, values, labels, -1);
  return true;
}

bool AutoFillManager::FillAutoFillFormData(int query_id,
                                           const FormData& form,
                                           const string16& value,
                                           const string16& label) {
  if (!IsAutoFillEnabled())
    return false;

  RenderViewHost* host = tab_contents_->render_view_host();
  if (!host)
    return false;

  const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
  const std::vector<CreditCard*>& credit_cards = personal_data_->credit_cards();
  if (profiles.empty() && credit_cards.empty())
    return false;

  // Find profile that matches the |value| and |label| in question.
  const AutoFillProfile* profile = NULL;
  for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    if ((*iter)->Label() != label)
      continue;

    FieldTypeSet field_types;
    (*iter)->GetPossibleFieldTypes(value, &field_types);
    for (FieldTypeSet::const_iterator type = field_types.begin();
         type != field_types.end(); ++type) {
      if ((*iter)->GetFieldText(AutoFillType(*type)) == value) {
        profile = *iter;
        break;
      }
    }
  }

  // Only look for credit card info if we're not filling profile.
  const CreditCard* credit_card = NULL;
  if (!profile) {
    for (std::vector<CreditCard*>::const_iterator iter = credit_cards.begin();
         iter != credit_cards.end(); ++iter) {
      if ((*iter)->Label() != label)
        continue;

      FieldTypeSet field_types;
      (*iter)->GetPossibleFieldTypes(value, &field_types);
      for (FieldTypeSet::const_iterator type = field_types.begin();
           type != field_types.end(); ++type) {
        if ((*iter)->GetFieldText(AutoFillType(*type)) == value) {
          credit_card = *iter;
          break;
        }
      }
    }
  }

  if (!profile && !credit_card)
    return false;

  // We fill either the profile or the credit card, not both.
  DCHECK((profile && !credit_card) || (!profile && credit_card));

  FormData result = form;
  for (std::vector<FormStructure*>::const_iterator iter =
           form_structures_.begin();
       iter != form_structures_.end(); ++iter) {
    const FormStructure* form_structure = *iter;
    if (*form_structure != form)
      continue;

    for (size_t i = 0; i < form_structure->field_count(); ++i) {
      const AutoFillField* field = form_structure->field(i);

      // TODO(jhawkins): We should have the same number of fields in both
      // |form_structure| and |result|, so this loop should be unnecessary.
      for (size_t j = 0; j < result.fields.size(); ++j) {
        if (field->name() == result.fields[j].name() &&
            field->label() == result.fields[j].label()) {
          AutoFillType autofill_type(field->type());
          if (credit_card &&
              autofill_type.group() == AutoFillType::CREDIT_CARD) {
            result.fields[j].set_value(
                credit_card->GetFieldText(autofill_type));
          } else if (profile) {
            result.fields[j].set_value(profile->GetFieldText(autofill_type));
          }
          break;
        }
      }
    }
  }

  host->AutoFillFormDataFilled(query_id, result);
  return true;
}

void AutoFillManager::OnInfoBarClosed() {
  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAutoFillEnabled, true);

  // Save the imported form data as a profile.
  personal_data_->SaveImportedFormData();
}

void AutoFillManager::OnInfoBarAccepted() {
  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAutoFillEnabled, true);

  // This is the first time the user is interacting with AutoFill, so set the
  // uploaded form structure as the initial profile and credit card in the
  // AutoFillDialog.
  AutoFillProfile* profile = NULL;
  CreditCard* credit_card = NULL;
  // TODO(dhollowa) Now that we aren't immediately saving the imported form
  // data, we should store the profile and CC in the AFM instead of the PDM.
  personal_data_->GetImportedFormData(&profile, &credit_card);
  ShowAutoFillDialog(tab_contents_->GetContentNativeView(),
                     personal_data_,
                     tab_contents_->profile()->GetOriginalProfile(),
                     profile,
                     credit_card);
}

void AutoFillManager::OnInfoBarCancelled() {
  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAutoFillEnabled, false);
}

void AutoFillManager::Reset() {
  upload_form_structure_.reset();
  form_structures_.reset();
}

void AutoFillManager::OnLoadedAutoFillHeuristics(
    const std::vector<std::string>& form_signatures,
    const std::string& heuristic_xml) {
  // Create a vector of AutoFillFieldTypes,
  // to assign the parsed field types to.
  std::vector<AutoFillFieldType> field_types;
  UploadRequired upload_required = USE_UPLOAD_RATES;

  // Create a parser.
  AutoFillQueryXmlParser parse_handler(&field_types, &upload_required);
  buzz::XmlParser parser(&parse_handler);
  parser.Parse(heuristic_xml.c_str(), heuristic_xml.length(), true);
  if (!parse_handler.succeeded()) {
    return;
  }

  // For multiple forms requested, returned field types are in one array.
  // |field_shift| indicates start of the fields for current form.
  size_t field_shift = 0;
  // form_signatures should mirror form_structures_ unless new request is
  // initiated. So if there is a discrepancy we just ignore data and return.
  ScopedVector<FormStructure>::iterator it_forms;
  std::vector<std::string>::const_iterator it_signatures;
  for (it_forms = form_structures_.begin(),
       it_signatures = form_signatures.begin();
       it_forms != form_structures_.end() &&
       it_signatures != form_signatures.end() &&
       (*it_forms)->FormSignature() == *it_signatures;
       ++it_forms, ++it_signatures) {
    // In some cases *successful* response does not return all the fields.
    // Quit the update of the types then.
    if (field_types.size() - field_shift < (*it_forms)->field_count()) {
      break;
    }
    for (size_t i = 0; i < (*it_forms)->field_count(); ++i) {
      if (field_types[i + field_shift] != NO_SERVER_DATA &&
          field_types[i + field_shift] != UNKNOWN_TYPE) {
        FieldTypeSet types = (*it_forms)->field(i)->possible_types();
        types.insert(field_types[i + field_shift]);
        (*it_forms)->set_possible_types(i, types);
      }
    }
    field_shift += (*it_forms)->field_count();
  }
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
  if (prefs->HasPrefPath(prefs::kFormAutofillEnabled)) {
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

  UploadFormData();
}

void AutoFillManager::UploadFormData() {
  // TODO(georgey): enable upload request when we make sure that our data is in
  // line with toolbar data:
  // download_manager_.StartUploadRequest(upload_form_structure_,
  //                                      form_is_autofilled);
}

void AutoFillManager::FillDefaultProfile() {
  if (!IsAutoFillEnabled())
    return;

  RenderViewHost* host = tab_contents_->render_view_host();
  if (!host)
    return;

  // TODO(jhawkins): Do we need to wait for the profiles to be loaded?
  const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
  if (profiles.empty())
    return;

  AutoFillProfile* profile = NULL;
  int default_profile = personal_data_->DefaultProfile();
  if (default_profile != -1)
    profile = profiles[default_profile];

  // If we have any profiles, at least one of them must be the default.
  DCHECK(profile);

  std::vector<FormData> forms;
  for (std::vector<FormStructure*>::const_iterator iter =
           form_structures_.begin();
       iter != form_structures_.end(); ++iter) {
    const FormStructure* form_structure = *iter;

    // Don't fill the form if it's not auto-fillable.
    if (!form_structure->IsAutoFillable())
      continue;

    FormData form = form_structure->ConvertToFormData();
    DCHECK_EQ(form_structure->field_count(), form.fields.size());

    for (size_t i = 0; i < form_structure->field_count(); ++i) {
      const AutoFillField* field = form_structure->field(i);
      AutoFillType type(field->type());

      // Don't AutoFill credit card information.
      if (type.group() == AutoFillType::CREDIT_CARD)
        continue;

      form.fields[i].set_value(profile->GetFieldText(type));
    }

    forms.push_back(form);
  }

  if (!forms.empty())
    host->AutoFillForms(forms);
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
      download_manager_(NULL),  // No download manager in tests.
      infobar_(NULL) {
  DCHECK(tab_contents);
}

void AutoFillManager::GetProfileSuggestions(const FormField& field,
                                            AutoFillFieldType type,
                                            std::vector<string16>* values,
                                            std::vector<string16>* labels) {
  const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
  for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    AutoFillProfile* profile = *iter;

    // The value of the stored data for this field type in the |profile|.
    string16 profile_field_value = profile->GetFieldText(AutoFillType(type));

    if (!profile_field_value.empty() &&
        StartsWith(profile_field_value, field.value(), false)) {
      values->push_back(profile_field_value);
      labels->push_back(profile->Label());
    }
  }
}

void AutoFillManager::GetCreditCardSuggestions(const FormField& field,
                                               AutoFillFieldType type,
                                               std::vector<string16>* values,
                                               std::vector<string16>* labels) {
  // TODO(jhawkins): Only return suggestions for the credit card number until
  // the AutoFill dropdown is redesigned to show a credit card icon.
  if (type != CREDIT_CARD_NUMBER)
    return;

  const std::vector<CreditCard*>& credit_cards = personal_data_->credit_cards();
  for (std::vector<CreditCard*>::const_iterator iter = credit_cards.begin();
       iter != credit_cards.end(); ++iter) {
    CreditCard* credit_card = *iter;

    // The value of the stored data for this field type in the |credit_card|.
    string16 creditcard_field_value =
        credit_card->GetFieldText(AutoFillType(type));
    if (!creditcard_field_value.empty() &&
        StartsWith(creditcard_field_value, field.value(), false)) {
      values->push_back(credit_card->ObfuscatedNumber());
      labels->push_back(credit_card->Label());
    }
  }
}
