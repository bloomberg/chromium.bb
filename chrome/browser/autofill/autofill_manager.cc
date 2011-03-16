// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_manager.h"

#include <limits>
#include <map>
#include <set>
#include <utility>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete_history_manager.h"
#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/phone_number.h"
#include "chrome/browser/autofill/select_control_handler.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/guid.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;
using webkit_glue::FormField;

namespace {

// We only send a fraction of the forms to upload server.
// The rate for positive/negative matches potentially could be different.
const double kAutoFillPositiveUploadRateDefaultValue = 0.01;
const double kAutoFillNegativeUploadRateDefaultValue = 0.01;

const string16::value_type kCreditCardPrefix[] = {'*', 0};
const string16::value_type kLabelSeparator[] = {';', ' ', '*', 0};

// Removes duplicate suggestions whilst preserving their original order.
void RemoveDuplicateSuggestions(std::vector<string16>* values,
                                std::vector<string16>* labels,
                                std::vector<string16>* icons,
                                std::vector<int>* unique_ids) {
  DCHECK_EQ(values->size(), labels->size());
  DCHECK_EQ(values->size(), icons->size());
  DCHECK_EQ(values->size(), unique_ids->size());

  std::set<std::pair<string16, string16> > seen_suggestions;
  std::vector<string16> values_copy;
  std::vector<string16> labels_copy;
  std::vector<string16> icons_copy;
  std::vector<int> unique_ids_copy;

  for (size_t i = 0; i < values->size(); ++i) {
    const std::pair<string16, string16> suggestion((*values)[i], (*labels)[i]);
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

// Precondition: |form| should be the cached version of the form that is to be
// autofilled, and |field| should be the field in the |form| that corresponds to
// the initiating field. |is_filling_credit_card| should be true if filling
// credit card data, false otherwise.
// Fills |section_start| and |section_end| so that [section_start, section_end)
// gives the bounds of logical section within |form| that includes |field|.
// Logical sections are identified by two heuristics:
//  1. The fields in the section must all be profile or credit card fields,
//     depending on whether |is_filling_credit_card| is true.
//  2. A logical section should not include multiple fields of the same autofill
//     type (except for phone/fax numbers, as described below).
void FindSectionBounds(const FormStructure& form,
                       const AutofillField& field,
                       bool is_filling_credit_card,
                       size_t* section_start,
                       size_t* section_end) {
  DCHECK(section_start);
  DCHECK(section_end);

  // By default, the relevant section is the entire form.
  *section_start = 0;
  *section_end = form.field_count();

  std::set<AutofillFieldType> seen_types;
  bool initiating_field_is_in_current_section = false;
  for (size_t i = 0; i < form.field_count(); ++i) {
    const AutofillField* current_field = form.field(i);
    const AutofillFieldType current_type =
        AutofillType::GetEquivalentFieldType(current_field->type());

    // Fields of unknown type don't help us to distinguish sections.
    if (current_type == UNKNOWN_TYPE)
      continue;

    bool already_saw_current_type = seen_types.count(current_type) > 0;
    // Forms often ask for multiple phone numbers -- e.g. both a daytime and
    // evening phone number.  Our phone and fax number detection is also
    // generally a little off.  Hence, ignore both field types as a signal here.
    // Likewise, forms often ask for multiple email addresses, so ignore this
    // field type as well.
    AutofillType::FieldTypeGroup current_type_group =
        AutofillType(current_type).group();
    if (current_type_group == AutofillType::PHONE_HOME ||
        current_type_group == AutofillType::PHONE_FAX ||
        current_type_group == AutofillType::EMAIL)
      already_saw_current_type = false;

    // If we are filling credit card data, the relevant section should include
    // only credit card fields; and similarly for profile data.
    bool is_credit_card_field = current_type_group == AutofillType::CREDIT_CARD;
    bool is_appropriate_type = is_credit_card_field == is_filling_credit_card;

    if (already_saw_current_type || !is_appropriate_type) {
      if (initiating_field_is_in_current_section) {
        // We reached the end of the section containing the initiating field.
        *section_end = i;
        break;
      }

      // We reached the end of a section, so start a new section.
      seen_types.clear();

      // Only include the current field in the new section if it matches the
      // type of data we are filling.
      if (is_appropriate_type) {
        *section_start = i;
      } else {
        *section_start = i + 1;
        continue;
      }
    }

    seen_types.insert(current_type);

    if (current_field == &field)
      initiating_field_is_in_current_section = true;
  }

  // We should have found the initiating field.
  DCHECK(initiating_field_is_in_current_section);
}

// Precondition: |form_structure| and |form| should correspond to the same
// logical form. Returns true if the relevant portion of |form| is auto-filled.
// The "relevant" fields in |form| are ones corresponding to fields in
// |form_structure| with indices in the range [section_start, section_end).
bool SectionIsAutoFilled(const FormStructure* form_structure,
                         const webkit_glue::FormData& form,
                         size_t section_start,
                         size_t section_end) {
  // TODO(isherman): It would be nice to share most of this code with the loop
  // in |FillAutoFillFormData()|, but I don't see a particularly clean way to do
  // that.

  // The list of fields in |form_structure| and |form.fields| often match
  // directly and we can fill these corresponding fields; however, when the
  // |form_structure| and |form.fields| do not match directly we search
  // ahead in the |form_structure| for the matching field.
  for (size_t i = section_start, j = 0;
       i < section_end && j < form.fields.size();
       j++) {
    size_t k = i;

    // Search forward in the |form_structure| for a corresponding field.
    while (k < form_structure->field_count() &&
           *form_structure->field(k) != form.fields[j]) {
      k++;
    }

    // If we didn't find a match, continue on to the next |form| field.
    if (k >= form_structure->field_count())
      continue;

    AutofillType autofill_type(form_structure->field(k)->type());
    if (form.fields[j].is_autofilled)
      return true;

    // We found a matching field in the |form_structure| so we
    // proceed to the next |form| field, and the next |form_structure|.
    ++i;
  }

  return false;
}

bool FormIsHTTPS(FormStructure* form) {
  return form->source_url().SchemeIs(chrome::kHttpsScheme);
}

}  // namespace

AutofillManager::AutofillManager(TabContents* tab_contents)
    : TabContentsObserver(tab_contents),
      personal_data_(NULL),
      download_manager_(tab_contents->profile()),
      disable_download_manager_requests_(false),
      metric_logger_(new AutofillMetrics),
      cc_infobar_(NULL) {
  DCHECK(tab_contents);

  // |personal_data_| is NULL when using TestTabContents.
  personal_data_ =
      tab_contents->profile()->GetOriginalProfile()->GetPersonalDataManager();
  download_manager_.SetObserver(this);
}

AutofillManager::~AutofillManager() {
  download_manager_.SetObserver(NULL);
}

// static
void AutofillManager::RegisterBrowserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kAutoFillDialogPlacement);
}

// static
void AutofillManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kAutoFillEnabled, true);
#if defined(OS_MACOSX)
  prefs->RegisterBooleanPref(prefs::kAutoFillAuxiliaryProfilesEnabled, true);
#else
  prefs->RegisterBooleanPref(prefs::kAutoFillAuxiliaryProfilesEnabled, false);
#endif
  prefs->RegisterDoublePref(prefs::kAutoFillPositiveUploadRate,
                            kAutoFillPositiveUploadRateDefaultValue);
  prefs->RegisterDoublePref(prefs::kAutoFillNegativeUploadRate,
                            kAutoFillNegativeUploadRateDefaultValue);
}

void AutofillManager::DidNavigateMainFramePostCommit(
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  Reset();
}

bool AutofillManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutofillManager, message)
    IPC_MESSAGE_HANDLER(AutoFillHostMsg_FormsSeen, OnFormsSeen)
    IPC_MESSAGE_HANDLER(AutoFillHostMsg_FormSubmitted, OnFormSubmitted)
    IPC_MESSAGE_HANDLER(AutoFillHostMsg_QueryFormFieldAutoFill,
                        OnQueryFormFieldAutoFill)
    IPC_MESSAGE_HANDLER(AutoFillHostMsg_ShowAutoFillDialog,
                        OnShowAutoFillDialog)
    IPC_MESSAGE_HANDLER(AutoFillHostMsg_FillAutoFillFormData,
                        OnFillAutoFillFormData)
    IPC_MESSAGE_HANDLER(AutoFillHostMsg_DidFillAutoFillFormData,
                        OnDidFillAutoFillFormData)
    IPC_MESSAGE_HANDLER(AutoFillHostMsg_DidShowAutoFillSuggestions,
                        OnDidShowAutoFillSuggestions)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void AutofillManager::OnFormSubmitted(const FormData& form) {
  // Let AutoComplete know as well.
  tab_contents()->autocomplete_history_manager()->OnFormSubmitted(form);

  if (!IsAutoFillEnabled())
    return;

  if (tab_contents()->profile()->IsOffTheRecord())
    return;

  // Don't save data that was submitted through JavaScript.
  if (!form.user_submitted)
    return;

  // Grab a copy of the form data.
  FormStructure submitted_form(form);

  // Disregard forms that we wouldn't ever autofill in the first place.
  if (!submitted_form.ShouldBeParsed(true))
    return;

  DeterminePossibleFieldTypesForUpload(&submitted_form);
  LogMetricsAboutSubmittedForm(form, submitted_form);

  UploadFormData(submitted_form);

  if (!submitted_form.IsAutoFillable(true))
    return;

  ImportFormData(submitted_form);
}

void AutofillManager::OnFormsSeen(const std::vector<FormData>& forms) {
  if (!IsAutoFillEnabled())
    return;

  ParseForms(forms);
}

void AutofillManager::OnQueryFormFieldAutoFill(
    int query_id,
    const webkit_glue::FormData& form,
    const webkit_glue::FormField& field) {
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;

  RenderViewHost* host = NULL;
  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (GetHost(
          personal_data_->profiles(), personal_data_->credit_cards(), &host) &&
      FindCachedFormAndField(form, field, &form_structure, &autofill_field) &&
      // Don't send suggestions for forms that aren't auto-fillable.
      form_structure->IsAutoFillable(false)) {
    AutofillType type(autofill_field->type());
    bool is_filling_credit_card = (type.group() == AutofillType::CREDIT_CARD);
    if (is_filling_credit_card) {
      GetCreditCardSuggestions(
          form_structure, field, type, &values, &labels, &icons, &unique_ids);
    } else {
      GetProfileSuggestions(
          form_structure, field, type, &values, &labels, &icons, &unique_ids);
    }

    DCHECK_EQ(values.size(), labels.size());
    DCHECK_EQ(values.size(), icons.size());
    DCHECK_EQ(values.size(), unique_ids.size());

    if (!values.empty()) {
      // Don't provide AutoFill suggestions when AutoFill is disabled, and don't
      // provide credit card suggestions for non-HTTPS pages. However, provide a
      // warning to the user in these cases.
      int warning = 0;
      if (!form_structure->IsAutoFillable(true))
        warning = IDS_AUTOFILL_WARNING_FORM_DISABLED;
      else if (is_filling_credit_card && !FormIsHTTPS(form_structure))
        warning = IDS_AUTOFILL_WARNING_INSECURE_CONNECTION;
      if (warning) {
        values.assign(1, l10n_util::GetStringUTF16(warning));
        labels.assign(1, string16());
        icons.assign(1, string16());
        unique_ids.assign(1, -1);
      } else {
        size_t section_start, section_end;
        FindSectionBounds(*form_structure, *autofill_field,
                          is_filling_credit_card, &section_start, &section_end);
        if (SectionIsAutoFilled(form_structure, form, section_start,
                                section_end)) {
          // If the relevant section is auto-filled and the renderer is querying
          // for suggestions, then the user is editing the value of a field.
          // In this case, mimic autocomplete: don't display labels or icons,
          // as that information is redundant.
          labels.assign(labels.size(), string16());
          icons.assign(icons.size(), string16());
        }

        RemoveDuplicateSuggestions(&values, &labels, &icons, &unique_ids);
      }
    }
  }

  // Add the results from AutoComplete.  They come back asynchronously, so we
  // hand off what we generated and they will send the results back to the
  // renderer.
  tab_contents()->autocomplete_history_manager()->OnGetAutocompleteSuggestions(
      query_id, field.name, field.value, values, labels, icons, unique_ids);
}

void AutofillManager::OnFillAutoFillFormData(int query_id,
                                             const FormData& form,
                                             const FormField& field,
                                             int unique_id) {
  const std::vector<AutofillProfile*>& profiles = personal_data_->profiles();
  const std::vector<CreditCard*>& credit_cards = personal_data_->credit_cards();
  RenderViewHost* host = NULL;
  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (!GetHost(profiles, credit_cards, &host) ||
      !FindCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  DCHECK(host);
  DCHECK(form_structure);
  DCHECK(autofill_field);

  // Unpack the |unique_id| into component parts.
  std::string cc_guid;
  std::string profile_guid;
  UnpackGUIDs(unique_id, &cc_guid, &profile_guid);
  DCHECK(!guid::IsValidGUID(cc_guid) || !guid::IsValidGUID(profile_guid));

  // Find the profile that matches the |profile_id|, if one is specified.
  const AutofillProfile* profile = NULL;
  if (guid::IsValidGUID(profile_guid)) {
    for (std::vector<AutofillProfile*>::const_iterator iter = profiles.begin();
         iter != profiles.end(); ++iter) {
      if ((*iter)->guid() == profile_guid) {
        profile = *iter;
        break;
      }
    }
    DCHECK(profile);
  }

  // Find the credit card that matches the |cc_id|, if one is specified.
  const CreditCard* credit_card = NULL;
  if (guid::IsValidGUID(cc_guid)) {
    for (std::vector<CreditCard*>::const_iterator iter = credit_cards.begin();
         iter != credit_cards.end(); ++iter) {
      if ((*iter)->guid() == cc_guid) {
        credit_card = *iter;
        break;
      }
    }
    DCHECK(credit_card);
  }

  if (!profile && !credit_card)
    return;

  // Find the section of the form that we are autofilling.
  size_t section_start, section_end;
  FindSectionBounds(*form_structure, *autofill_field, (credit_card != NULL),
                    &section_start, &section_end);

  FormData result = form;

  // If the relevant section is auto-filled, we should fill |field| but not the
  // rest of the form.
  if (SectionIsAutoFilled(form_structure, form, section_start, section_end)) {
    for (std::vector<FormField>::iterator iter = result.fields.begin();
         iter != result.fields.end(); ++iter) {
      if ((*iter) == field) {
        AutofillType autofill_type(autofill_field->type());
        if (profile) {
          DCHECK_NE(AutofillType::CREDIT_CARD, autofill_type.group());
          FillFormField(profile, autofill_type, &(*iter));
        } else {
          DCHECK_EQ(AutofillType::CREDIT_CARD, autofill_type.group());
          FillCreditCardFormField(credit_card, autofill_type, &(*iter));
        }
        break;
      }
    }

    host->Send(new AutoFillMsg_FormDataFilled(host->routing_id(), query_id,
                                              result));
    return;
  }

  // The list of fields in |form_structure| and |result.fields| often match
  // directly and we can fill these corresponding fields; however, when the
  // |form_structure| and |result.fields| do not match directly we search
  // ahead in the |form_structure| for the matching field.
  // See unit tests: AutofillManagerTest.FormChangesRemoveField and
  // AutofillManagerTest.FormChangesAddField for usage.
  for (size_t i = section_start, j = 0;
       i < section_end && j < result.fields.size();
       j++) {
    size_t k = i;

    // Search forward in the |form_structure| for a corresponding field.
    while (k < section_end && *form_structure->field(k) != result.fields[j]) {
      k++;
    }

    // If we've found a match then fill the |result| field with the found
    // field in the |form_structure|.
    if (k >= section_end)
      continue;

    AutofillType autofill_type(form_structure->field(k)->type());
    if (autofill_type.group() != AutofillType::NO_GROUP) {
      if (profile) {
        DCHECK_NE(AutofillType::CREDIT_CARD, autofill_type.group());
        FillFormField(profile, autofill_type, &result.fields[j]);
      } else {
        DCHECK_EQ(AutofillType::CREDIT_CARD, autofill_type.group());
        FillCreditCardFormField(credit_card, autofill_type, &result.fields[j]);
      }
    }

    // We found a matching field in the |form_structure| so we
    // proceed to the next |result| field, and the next |form_structure|.
    ++i;
  }
  autofilled_forms_signatures_.push_front(form_structure->FormSignature());

  host->Send(new AutoFillMsg_FormDataFilled(
      host->routing_id(), query_id, result));
}

void AutofillManager::OnShowAutoFillDialog() {
  Browser* browser = BrowserList::GetLastActive();
  if (browser)
    browser->ShowOptionsTab(chrome::kAutoFillSubPage);
}

void AutofillManager::OnDidFillAutoFillFormData() {
  NotificationService::current()->Notify(
      NotificationType::AUTOFILL_DID_FILL_FORM_DATA,
      Source<RenderViewHost>(tab_contents()->render_view_host()),
      NotificationService::NoDetails());
}

void AutofillManager::OnDidShowAutoFillSuggestions() {
  NotificationService::current()->Notify(
      NotificationType::AUTOFILL_DID_SHOW_SUGGESTIONS,
      Source<RenderViewHost>(tab_contents()->render_view_host()),
      NotificationService::NoDetails());
}

void AutofillManager::OnLoadedAutofillHeuristics(
    const std::string& heuristic_xml) {
  // TODO(jhawkins): Store |upload_required| in the AutofillManager.
  UploadRequired upload_required;
  FormStructure::ParseQueryResponse(heuristic_xml,
                                    form_structures_.get(),
                                    &upload_required,
                                    *metric_logger_);
}

void AutofillManager::OnUploadedAutofillHeuristics(
    const std::string& form_signature) {
}

void AutofillManager::OnHeuristicsRequestError(
    const std::string& form_signature,
    AutofillDownloadManager::AutofillRequestType request_type,
    int http_error) {
}

bool AutofillManager::IsAutoFillEnabled() const {
  PrefService* prefs =
      const_cast<AutofillManager*>(this)->tab_contents()->profile()->GetPrefs();

  // Migrate obsolete AutoFill pref.
  if (prefs->FindPreference(prefs::kFormAutofillEnabled)) {
    bool enabled = prefs->GetBoolean(prefs::kFormAutofillEnabled);
    prefs->ClearPref(prefs::kFormAutofillEnabled);
    prefs->SetBoolean(prefs::kAutoFillEnabled, enabled);
    return enabled;
  }

  return prefs->GetBoolean(prefs::kAutoFillEnabled);
}

void AutofillManager::DeterminePossibleFieldTypesForUpload(
    FormStructure* submitted_form) {
  for (size_t i = 0; i < submitted_form->field_count(); i++) {
    const AutofillField* field = submitted_form->field(i);
    FieldTypeSet field_types;
    personal_data_->GetPossibleFieldTypes(field->value, &field_types);

    DCHECK(!field_types.empty());
    submitted_form->set_possible_types(i, field_types);
  }
}

void AutofillManager::LogMetricsAboutSubmittedForm(
    const FormData& form,
    const FormStructure& submitted_form) const {
  FormStructure* cached_submitted_form;
  if (!FindCachedForm(form, &cached_submitted_form)) {
    NOTREACHED();
    return;
  }

  submitted_form.LogQualityMetrics(*cached_submitted_form, *metric_logger_);
}

void AutofillManager::ImportFormData(const FormStructure& submitted_form) {
  std::vector<const FormStructure*> import;
  import.push_back(&submitted_form);

  const CreditCard* imported_credit_card;
  if (!personal_data_->ImportFormData(import, &imported_credit_card))
    return;

  // If credit card information was submitted, show an infobar to offer to save
  // it.
  if (imported_credit_card && tab_contents()) {
    imported_credit_card_.reset(imported_credit_card);
    tab_contents()->AddInfoBar(new AutoFillCCInfoBarDelegate(tab_contents(),
                                                             this));
  }
}

void AutofillManager::UploadFormData(const FormStructure& submitted_form) {
  if (!disable_download_manager_requests_) {
    bool was_autofilled = false;
    // Check if the form among last 3 forms that were auto-filled.
    // Clear older signatures.
    std::list<std::string>::iterator it;
    int total_form_checked = 0;
    for (it = autofilled_forms_signatures_.begin();
         it != autofilled_forms_signatures_.end() && total_form_checked < 3;
         ++it, ++total_form_checked) {
      if (*it == submitted_form.FormSignature())
        was_autofilled = true;
    }
    // Remove outdated form signatures.
    if (total_form_checked == 3 && it != autofilled_forms_signatures_.end()) {
      autofilled_forms_signatures_.erase(it,
                                         autofilled_forms_signatures_.end());
    }
    download_manager_.StartUploadRequest(submitted_form, was_autofilled);
  }
}

void AutofillManager::Reset() {
  form_structures_.reset();
}

void AutofillManager::OnInfoBarClosed(bool should_save) {
  if (should_save)
    personal_data_->SaveImportedCreditCard(*imported_credit_card_);
}

AutofillManager::AutofillManager(TabContents* tab_contents,
                                 PersonalDataManager* personal_data)
    : TabContentsObserver(tab_contents),
      personal_data_(personal_data),
      download_manager_(NULL),
      disable_download_manager_requests_(true),
      metric_logger_(new AutofillMetrics),
      cc_infobar_(NULL) {
  DCHECK(tab_contents);
}

void AutofillManager::set_metric_logger(
    const AutofillMetrics* metric_logger) {
  metric_logger_.reset(metric_logger);
}

bool AutofillManager::GetHost(const std::vector<AutofillProfile*>& profiles,
                              const std::vector<CreditCard*>& credit_cards,
                              RenderViewHost** host) const {
  if (!IsAutoFillEnabled())
    return false;

  // No autofill data to return if the profiles are empty.
  if (profiles.empty() && credit_cards.empty())
    return false;

  *host = tab_contents()->render_view_host();
  if (!*host)
    return false;

  return true;
}

bool AutofillManager::FindCachedForm(const FormData& form,
                                     FormStructure** form_structure) const {
  // Find the FormStructure that corresponds to |form|.
  *form_structure = NULL;
  for (std::vector<FormStructure*>::const_iterator iter =
       form_structures_.begin();
       iter != form_structures_.end(); ++iter) {
    if (**iter == form) {
      *form_structure = *iter;
      break;
    }
  }

  if (!(*form_structure))
    return false;

  return true;
}

bool AutofillManager::FindCachedFormAndField(const FormData& form,
                                             const FormField& field,
                                             FormStructure** form_structure,
                                             AutofillField** autofill_field) {
  // Find the FormStructure that corresponds to |form|.
  if (!FindCachedForm(form, form_structure))
    return false;

  // No data to return if there are no auto-fillable fields.
  if (!(*form_structure)->autofill_count())
    return false;

  // Find the AutofillField that corresponds to |field|.
  *autofill_field = NULL;
  for (std::vector<AutofillField*>::const_iterator iter =
           (*form_structure)->begin();
       iter != (*form_structure)->end(); ++iter) {
    // The field list is terminated with a NULL AutofillField, so don't try to
    // dereference it.
    if (!*iter)
      break;

    if ((**iter) == field) {
      *autofill_field = *iter;
      break;
    }
  }

  if (!(*autofill_field))
    return false;

  return true;
}

void AutofillManager::GetProfileSuggestions(FormStructure* form,
                                            const FormField& field,
                                            AutofillType type,
                                            std::vector<string16>* values,
                                            std::vector<string16>* labels,
                                            std::vector<string16>* icons,
                                            std::vector<int>* unique_ids) {
  const std::vector<AutofillProfile*>& profiles = personal_data_->profiles();
  std::vector<AutofillProfile*> matched_profiles;
  for (std::vector<AutofillProfile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    AutofillProfile* profile = *iter;

    // The value of the stored data for this field type in the |profile|.
    string16 profile_field_value = profile->GetFieldText(type);

    if (!profile_field_value.empty() &&
        StartsWith(profile_field_value, field.value, false)) {
      matched_profiles.push_back(profile);
      values->push_back(profile_field_value);
      unique_ids->push_back(PackGUIDs(std::string(), profile->guid()));
    }
  }

  std::vector<AutofillFieldType> form_fields;
  form_fields.reserve(form->field_count());
  for (std::vector<AutofillField*>::const_iterator iter = form->begin();
       iter != form->end(); ++iter) {
    // The field list is terminated with a NULL AutofillField, so don't try to
    // dereference it.
    if (!*iter)
      break;
    form_fields.push_back((*iter)->type());
  }

  AutofillProfile::CreateInferredLabels(&matched_profiles, &form_fields,
                                        type.field_type(), 1, labels);

  // No icons for profile suggestions.
  icons->resize(values->size());
}

void AutofillManager::GetCreditCardSuggestions(FormStructure* form,
                                               const FormField& field,
                                               AutofillType type,
                                               std::vector<string16>* values,
                                               std::vector<string16>* labels,
                                               std::vector<string16>* icons,
                                               std::vector<int>* unique_ids) {
  for (std::vector<CreditCard*>::const_iterator iter =
           personal_data_->credit_cards().begin();
       iter != personal_data_->credit_cards().end(); ++iter) {
    CreditCard* credit_card = *iter;

    // The value of the stored data for this field type in the |credit_card|.
    string16 creditcard_field_value = credit_card->GetFieldText(type);
    if (!creditcard_field_value.empty() &&
        StartsWith(creditcard_field_value, field.value, false)) {
      if (type.field_type() == CREDIT_CARD_NUMBER)
        creditcard_field_value = credit_card->ObfuscatedNumber();

      string16 label;
      if (credit_card->number().empty()) {
        // If there is no CC number, return name to show something.
        label = credit_card->GetFieldText(AutofillType(CREDIT_CARD_NAME));
      } else {
        label = kCreditCardPrefix;
        label.append(credit_card->LastFourDigits());
      }

      values->push_back(creditcard_field_value);
      labels->push_back(label);
      icons->push_back(credit_card->type());
      unique_ids->push_back(PackGUIDs(credit_card->guid(), std::string()));
    }
  }
}

void AutofillManager::FillCreditCardFormField(const CreditCard* credit_card,
                                              AutofillType type,
                                              webkit_glue::FormField* field) {
  DCHECK(credit_card);
  DCHECK_EQ(AutofillType::CREDIT_CARD, type.group());
  DCHECK(field);

  if (field->form_control_type == ASCIIToUTF16("select-one")) {
    autofill::FillSelectControl(*credit_card, type, field);
  } else if (field->form_control_type == ASCIIToUTF16("month")) {
    // HTML5 input="month" consists of year-month.
    string16 year = credit_card->GetFieldText(
        AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR));
    string16 month = credit_card->GetFieldText(
        AutofillType(CREDIT_CARD_EXP_MONTH));
    if (!year.empty() && !month.empty()) {
      // Fill the value only if |credit_card| includes both year and month
      // information.
      field->value = year + ASCIIToUTF16("-") + month;
    }
  } else {
    field->value = credit_card->GetFieldText(type);
  }
}

void AutofillManager::FillFormField(const AutofillProfile* profile,
                                    AutofillType type,
                                    webkit_glue::FormField* field) {
  DCHECK(profile);
  DCHECK_NE(AutofillType::CREDIT_CARD, type.group());
  DCHECK(field);

  if (type.subgroup() == AutofillType::PHONE_NUMBER) {
    FillPhoneNumberField(profile, type, field);
  } else {
    if (field->form_control_type == ASCIIToUTF16("select-one"))
      autofill::FillSelectControl(*profile, type, field);
    else
      field->value = profile->GetFieldText(type);
  }
}

void AutofillManager::FillPhoneNumberField(const AutofillProfile* profile,
                                           AutofillType type,
                                           webkit_glue::FormField* field) {
  // If we are filling a phone number, check to see if the size field
  // matches the "prefix" or "suffix" sizes and fill accordingly.
  string16 number = profile->GetFieldText(AutofillType(type));
  bool has_valid_suffix_and_prefix = (number.length() ==
      static_cast<size_t>(PhoneNumber::kPrefixLength +
                          PhoneNumber::kSuffixLength));
  if (has_valid_suffix_and_prefix &&
      field->max_length == PhoneNumber::kPrefixLength) {
    number = number.substr(PhoneNumber::kPrefixOffset,
                           PhoneNumber::kPrefixLength);
    field->value = number;
  } else if (has_valid_suffix_and_prefix &&
             field->max_length == PhoneNumber::kSuffixLength) {
    number = number.substr(PhoneNumber::kSuffixOffset,
                           PhoneNumber::kSuffixLength);
    field->value = number;
  } else {
    field->value = number;
  }
}

void AutofillManager::ParseForms(const std::vector<FormData>& forms) {
  std::vector<FormStructure*> non_queryable_forms;
  for (std::vector<FormData>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    scoped_ptr<FormStructure> form_structure(new FormStructure(*iter));
    if (!form_structure->ShouldBeParsed(false))
      continue;

    form_structure->DetermineHeuristicTypes();

    // Set aside forms with method GET so that they are not included in the
    // query to the server.
    if (form_structure->ShouldBeParsed(true))
      form_structures_.push_back(form_structure.release());
    else
      non_queryable_forms.push_back(form_structure.release());
  }

  // If none of the forms were parsed, no use querying the server.
  if (!form_structures_.empty() && !disable_download_manager_requests_)
    download_manager_.StartQueryRequest(form_structures_, *metric_logger_);

  for (std::vector<FormStructure*>::const_iterator iter =
           non_queryable_forms.begin();
       iter != non_queryable_forms.end(); ++iter) {
    form_structures_.push_back(*iter);
  }
}

// When sending IDs (across processes) to the renderer we pack credit card and
// profile IDs into a single integer.  Credit card IDs are sent in the high
// word and profile IDs are sent in the low word.
int AutofillManager::PackGUIDs(const std::string& cc_guid,
                               const std::string& profile_guid) {
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
                                  std::string* cc_guid,
                                  std::string* profile_guid) {
  int cc_id = id >> std::numeric_limits<unsigned short>::digits &
      std::numeric_limits<unsigned short>::max();
  int profile_id = id & std::numeric_limits<unsigned short>::max();

  *cc_guid = IDToGUID(cc_id);
  *profile_guid = IDToGUID(profile_id);
}

int AutofillManager::GUIDToID(const std::string& guid) {
  static int last_id = 1;

  if (!guid::IsValidGUID(guid))
    return 0;

  std::map<std::string, int>::const_iterator iter = guid_id_map_.find(guid);
  if (iter == guid_id_map_.end()) {
    guid_id_map_[guid] = last_id;
    id_guid_map_[last_id] = guid;
    return last_id++;
  } else {
    return iter->second;
  }
}

const std::string AutofillManager::IDToGUID(int id) {
  if (id == 0)
    return std::string();

  std::map<int, std::string>::const_iterator iter = id_guid_map_.find(id);
  if (iter == id_guid_map_.end()) {
    NOTREACHED();
    return std::string();
  }

  return iter->second;
}
