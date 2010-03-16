// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_manager.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_infobar_delegate.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/form_field_values.h"

AutoFillManager::AutoFillManager(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      personal_data_(NULL),
      infobar_(NULL) {
  DCHECK(tab_contents);
  personal_data_ = tab_contents_->profile()->GetPersonalDataManager();
}

AutoFillManager::~AutoFillManager() {
  if (personal_data_)
    personal_data_->RemoveObserver(this);
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
}

void AutoFillManager::FormFieldValuesSubmitted(
    const webkit_glue::FormFieldValues& form) {
  if (!IsAutoFillEnabled())
    return;

  // Grab a copy of the form data.
  upload_form_structure_.reset(new FormStructure(form));

  if (!upload_form_structure_->IsAutoFillable())
    return;

  // Determine the possible field types and upload the form structure to the
  // PersonalDataManager.
  DeterminePossibleFieldTypes(upload_form_structure_.get());
  HandleSubmit();

  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  bool infobar_shown = prefs->GetBoolean(prefs::kAutoFillInfoBarShown);
  if (!infobar_shown) {
    // Ask the user for permission to save form information.
    infobar_.reset(new AutoFillInfoBarDelegate(tab_contents_, this));
  }
}

void AutoFillManager::FormsSeen(
    const std::vector<webkit_glue::FormFieldValues>& forms) {
  if (!IsAutoFillEnabled())
    return;

  for (std::vector<webkit_glue::FormFieldValues>::const_iterator iter =
           forms.begin();
       iter != forms.end(); ++iter) {
    FormStructure* form_structure = new FormStructure(*iter);
    DeterminePossibleFieldTypes(form_structure);
    form_structures_.push_back(form_structure);
  }
}

bool AutoFillManager::GetAutoFillSuggestions(
    int query_id, const webkit_glue::FormField& field) {
  if (!IsAutoFillEnabled())
    return false;

  RenderViewHost* host = tab_contents_->render_view_host();
  if (!host)
    return false;

  const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
  if (profiles.empty())
    return false;

  AutoFillFieldType type = UNKNOWN_TYPE;
  for (std::vector<FormStructure*>::iterator form = form_structures_.begin();
       form != form_structures_.end(); ++form) {
    for (std::vector<AutoFillField*>::const_iterator iter = (*form)->begin();
         iter != (*form)->end(); ++iter) {
      // The field list is terminated with a NULL AutoFillField, so don't try to
      // dereference it.
      if (!*iter)
        break;

      AutoFillField* form_field = *iter;
      if (*form_field != field)
        continue;

      if (form_field->possible_types().find(NAME_FIRST) !=
          form_field->possible_types().end() ||
          form_field->heuristic_type() == NAME_FIRST) {
        type = NAME_FIRST;
        break;
      }

      if (form_field->possible_types().find(NAME_FULL) !=
          form_field->possible_types().end() ||
          form_field->heuristic_type() == NAME_FULL) {
        type = NAME_FULL;
        break;
      }
    }
  }

  if (type == UNKNOWN_TYPE)
    return false;

  std::vector<string16> names;
  std::vector<string16> labels;
  for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    string16 name = (*iter)->GetFieldText(AutoFillType(type));
    string16 label = (*iter)->Label();

    // TODO(jhawkins): What if name.length() == 0?
    if (StartsWith(name, field.value(), false)) {
      names.push_back(name);
      labels.push_back(label);
    }
  }

  // No suggestions.
  if (names.empty())
    return false;

  // TODO(jhawkins): If the default profile is in this list, set it as the
  // default suggestion index.
  host->AutoFillSuggestionsReturned(query_id, names, labels, -1);
  return true;
}

bool AutoFillManager::FillAutoFillFormData(int query_id,
                                           const webkit_glue::FormData& form,
                                           const string16& name,
                                           const string16& label) {
  if (!IsAutoFillEnabled())
    return false;

  RenderViewHost* host = tab_contents_->render_view_host();
  if (!host)
    return false;

  const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
  if (profiles.empty())
    return false;

  const AutoFillProfile* profile = NULL;
  for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    if ((*iter)->Label() != label)
      continue;

    if ((*iter)->GetFieldText(AutoFillType(NAME_FIRST)) != name &&
        (*iter)->GetFieldText(AutoFillType(NAME_FULL)) != name)
      continue;

    profile = *iter;
    break;
  }

  if (!profile)
    return false;

  webkit_glue::FormData result = form;
  for (std::vector<FormStructure*>::const_iterator iter =
           form_structures_.begin();
       iter != form_structures_.end(); ++iter) {
    const FormStructure* form_structure = *iter;
    if (*form_structure != form)
      continue;

    for (size_t i = 0; i < form_structure->field_count(); ++i) {
      const AutoFillField* field = form_structure->field(i);

      for (size_t j = 0; j < result.fields.size(); ++j) {
        if (field->name() == result.fields[j].name()) {
          result.fields[j].set_value(
              profile->GetFieldText(AutoFillType(field->heuristic_type())));
          break;
        }
      }
    }
  }

  host->AutoFillFormDataFilled(query_id, result);
  return true;
}

void AutoFillManager::OnAutoFillDialogApply(
    std::vector<AutoFillProfile>* profiles,
    std::vector<CreditCard>* credit_cards) {
  DCHECK(personal_data_);

  // Save the personal data.
  personal_data_->SetProfiles(profiles);
  personal_data_->SetCreditCards(credit_cards);
}

void AutoFillManager::OnPersonalDataLoaded() {
  DCHECK(personal_data_);

  // We might have been alerted that the PersonalDataManager has loaded, so
  // remove ourselves as observer.
  personal_data_->RemoveObserver(this);

#if !defined(OS_WIN)
#if defined(OS_MACOSX)
  ShowAutoFillDialog(this,
                     personal_data_->web_profiles(),
                     personal_data_->credit_cards(),
                     tab_contents_->profile()->GetOriginalProfile());
#else  // defined(OS_MACOSX)
  ShowAutoFillDialog(NULL, this,
                     tab_contents_->profile()->GetOriginalProfile());
#endif  // defined(OS_MACOSX)
#endif  // !defined(OS_WIN)
}

void AutoFillManager::OnInfoBarClosed() {
  DCHECK(personal_data_);

  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAutoFillEnabled, true);

  // Save the imported form data as a profile.
  personal_data_->SaveImportedFormData();
}

void AutoFillManager::OnInfoBarAccepted() {
  DCHECK(personal_data_);

  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAutoFillEnabled, true);

  // This is the first time the user is interacting with AutoFill, so set the
  // uploaded form structure as the initial profile in the AutoFillDialog.
  personal_data_->SaveImportedFormData();

#if defined(OS_WIN)
  ShowAutoFillDialog(tab_contents_->GetContentNativeView(), this,
                     tab_contents_->profile()->GetOriginalProfile());
#else
  // If the personal data manager has not loaded the data yet, set ourselves as
  // its observer so that we can listen for the OnPersonalDataLoaded signal.
  if (!personal_data_->IsDataLoaded())
    personal_data_->SetObserver(this);
  else
    OnPersonalDataLoaded();
#endif
}

void AutoFillManager::OnInfoBarCancelled() {
  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAutoFillEnabled, false);
}

void AutoFillManager::Reset() {
  upload_form_structure_.reset();
  form_structures_.reset();
}

void AutoFillManager::DeterminePossibleFieldTypes(
    FormStructure* form_structure) {
  DCHECK(personal_data_);

  // TODO(jhawkins): Update field text.

  form_structure->GetHeuristicAutoFillTypes();

  for (size_t i = 0; i < form_structure->field_count(); i++) {
    const AutoFillField* field = form_structure->field(i);
    FieldTypeSet field_types;
    personal_data_->GetPossibleFieldTypes(field->value(), &field_types);
    form_structure->set_possible_types(i, field_types);
  }
}

void AutoFillManager::HandleSubmit() {
  DCHECK(personal_data_);

  // If there wasn't enough data to import then we don't want to send an upload
  // to the server.
  if (!personal_data_->ImportFormData(form_structures_.get(), this))
    return;

  UploadFormData();
}

void AutoFillManager::UploadFormData() {
  std::string xml;
  bool ok = upload_form_structure_->EncodeUploadRequest(false, &xml);
  DCHECK(ok);

  // TODO(jhawkins): Initiate the upload request thread.
}

bool AutoFillManager::IsAutoFillEnabled() {
  // The PersonalDataManager is NULL in OTR.
  if (!personal_data_)
    return false;

  PrefService* prefs = tab_contents_->profile()->GetPrefs();

  // Migrate obsolete autofill pref.
  if (prefs->HasPrefPath(prefs::kFormAutofillEnabled)) {
    bool enabled = prefs->GetBoolean(prefs::kFormAutofillEnabled);
    prefs->ClearPref(prefs::kFormAutofillEnabled);
    prefs->SetBoolean(prefs::kAutoFillEnabled, enabled);
    return enabled;
  }

  return prefs->GetBoolean(prefs::kAutoFillEnabled);
}
