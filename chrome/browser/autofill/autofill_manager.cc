// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_manager.h"

#include <limits>
#include <map>
#include <set>
#include <utility>

#include "base/basictypes.h"
#include "base/command_line.h"
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
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/guid.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
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

// Precondition: |form_structure| and |form| should correspond to the same
// logical form. Returns true if the relevant portion of |form| is auto-filled.
// If |is_filling_credit_card|, the relevant portion is the credit card portion;
// otherwise it is the address and contact info portion.
bool FormIsAutoFilled(const FormStructure* form_structure,
                      const webkit_glue::FormData& form,
                      bool is_filling_credit_card) {
  // TODO(isherman): It would be nice to share most of this code with the loop
  // in |FillAutoFillFormData()|, but I don't see a particularly clean way to do
  // that.

  // The list of fields in |form_structure| and |form.fields| often match
  // directly and we can fill these corresponding fields; however, when the
  // |form_structure| and |form.fields| do not match directly we search
  // ahead in the |form_structure| for the matching field.
  for (size_t i = 0, j = 0;
       i < form_structure->field_count() && j < form.fields.size();
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

    AutoFillType autofill_type(form_structure->field(k)->type());
    bool is_credit_card_field =
        autofill_type.group() == AutoFillType::CREDIT_CARD;
    if (is_filling_credit_card == is_credit_card_field &&
        form.fields[j].is_autofilled())
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

AutoFillManager::AutoFillManager(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      personal_data_(NULL),
      download_manager_(tab_contents_->profile()),
      disable_download_manager_requests_(false),
      metric_logger_(new AutoFillMetrics),
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

void AutoFillManager::DidNavigateMainFramePostCommit(
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  Reset();
}

bool AutoFillManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutoFillManager, message)
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

void AutoFillManager::OnFormSubmitted(const FormData& form) {
  // Let AutoComplete know as well.
  tab_contents_->autocomplete_history_manager()->OnFormSubmitted(form);

  if (!IsAutoFillEnabled())
    return;

  if (tab_contents_->profile()->IsOffTheRecord())
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
  LogMetricsAboutSubmittedForm(form, &submitted_form);

  UploadFormData(submitted_form);

  if (!submitted_form.IsAutoFillable(true))
    return;

  ImportFormData(submitted_form);
}

void AutoFillManager::OnFormsSeen(const std::vector<FormData>& forms) {
  if (!IsAutoFillEnabled())
    return;

  ParseForms(forms);
}

void AutoFillManager::OnQueryFormFieldAutoFill(
    int query_id,
    const webkit_glue::FormData& form,
    const webkit_glue::FormField& field) {
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;

  RenderViewHost* host = NULL;
  FormStructure* form_structure = NULL;
  AutoFillField* autofill_field = NULL;
  if (GetHost(
          personal_data_->profiles(), personal_data_->credit_cards(), &host) &&
      FindCachedFormAndField(form, field, &form_structure, &autofill_field) &&
      // Don't send suggestions for forms that aren't auto-fillable.
      form_structure->IsAutoFillable(false)) {
    AutoFillType type(autofill_field->type());
    bool is_filling_credit_card = (type.group() == AutoFillType::CREDIT_CARD);
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
        // If the form is auto-filled and the renderer is querying for
        // suggestions, then the user is editing the value of a field. In this
        // case, mimic autocomplete: don't display labels or icons, as that
        // information is redundant.
        if (FormIsAutoFilled(form_structure, form, is_filling_credit_card)) {
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
  tab_contents_->autocomplete_history_manager()->OnGetAutocompleteSuggestions(
      query_id, field.name(), field.value(), values, labels, icons, unique_ids);
}

void AutoFillManager::OnFillAutoFillFormData(int query_id,
                                             const FormData& form,
                                             const FormField& field,
                                             int unique_id) {
  const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
  const std::vector<CreditCard*>& credit_cards = personal_data_->credit_cards();
  RenderViewHost* host = NULL;
  FormStructure* form_structure = NULL;
  AutoFillField* autofill_field = NULL;
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
  const AutoFillProfile* profile = NULL;
  if (guid::IsValidGUID(profile_guid)) {
    for (std::vector<AutoFillProfile*>::const_iterator iter = profiles.begin();
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

  FormData result = form;

  // If the form is auto-filled, we should fill |field| but not the rest of the
  // form.
  if (FormIsAutoFilled(form_structure, form, (credit_card != NULL))) {
    for (std::vector<FormField>::iterator iter = result.fields.begin();
         iter != result.fields.end(); ++iter) {
      if ((*iter) == field) {
        AutoFillType autofill_type(autofill_field->type());
        if (credit_card &&
            autofill_type.group() == AutoFillType::CREDIT_CARD) {
          FillCreditCardFormField(credit_card, autofill_type, &(*iter));
        } else if (profile &&
                   autofill_type.group() != AutoFillType::CREDIT_CARD) {
          FillFormField(profile, autofill_type, &(*iter));
        }
        break;
      }
    }

    host->Send(new AutoFillMsg_FormDataFilled(
        host->routing_id(), query_id, result));
    return;
  }

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
    } else if (profile &&
               autofill_type.group() != AutoFillType::CREDIT_CARD) {
      FillFormField(profile, autofill_type, &result.fields[j]);
    }

    // We found a matching field in the |form_structure| so we
    // proceed to the next |result| field, and the next |form_structure|.
    ++i;
  }
  autofilled_forms_signatures_.push_front(form_structure->FormSignature());

  host->Send(new AutoFillMsg_FormDataFilled(
      host->routing_id(), query_id, result));
}

void AutoFillManager::OnShowAutoFillDialog() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableTabbedOptions)) {
    Browser* browser = BrowserList::GetLastActive();
    if (browser)
      browser->ShowOptionsTab(chrome::kAutoFillSubPage);
    return;
  }

  ShowAutoFillDialog(tab_contents_->GetContentNativeView(),
                     personal_data_,
                     tab_contents_->profile()->GetOriginalProfile());
}

void AutoFillManager::OnDidFillAutoFillFormData() {
  NotificationService::current()->Notify(
      NotificationType::AUTOFILL_DID_FILL_FORM_DATA,
      Source<RenderViewHost>(tab_contents_->render_view_host()),
      NotificationService::NoDetails());
}

void AutoFillManager::OnDidShowAutoFillSuggestions() {
  NotificationService::current()->Notify(
      NotificationType::AUTOFILL_DID_SHOW_SUGGESTIONS,
      Source<RenderViewHost>(tab_contents_->render_view_host()),
      NotificationService::NoDetails());
}

void AutoFillManager::OnLoadedAutoFillHeuristics(
    const std::string& heuristic_xml) {
  // TODO(jhawkins): Store |upload_required| in the AutoFillManager.
  UploadRequired upload_required;
  FormStructure::ParseQueryResponse(heuristic_xml,
                                    form_structures_.get(),
                                    &upload_required,
                                    *metric_logger_);
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

void AutoFillManager::DeterminePossibleFieldTypesForUpload(
    FormStructure* submitted_form) {
  for (size_t i = 0; i < submitted_form->field_count(); i++) {
    const AutoFillField* field = submitted_form->field(i);
    FieldTypeSet field_types;
    personal_data_->GetPossibleFieldTypes(field->value(), &field_types);
    DCHECK(!field_types.empty());
    submitted_form->set_possible_types(i, field_types);
  }
}

void AutoFillManager::LogMetricsAboutSubmittedForm(
    const FormData& form,
    const FormStructure* submitted_form) {
  FormStructure* cached_submitted_form;
  if (!FindCachedForm(form, &cached_submitted_form)) {
    NOTREACHED();
    return;
  }

  // Map from field signatures to cached fields.
  std::map<std::string, const AutoFillField*> cached_fields;
  for (size_t i = 0; i < cached_submitted_form->field_count(); ++i) {
    const AutoFillField* field = cached_submitted_form->field(i);
    cached_fields[field->FieldSignature()] = field;
  }

  std::string experiment_id = cached_submitted_form->server_experiment_id();
  for (size_t i = 0; i < submitted_form->field_count(); ++i) {
    const AutoFillField* field = submitted_form->field(i);
    FieldTypeSet field_types;
    personal_data_->GetPossibleFieldTypes(field->value(), &field_types);
    DCHECK(!field_types.empty());

    if (field->form_control_type() == ASCIIToUTF16("select-one")) {
      // TODO(isherman): <select> fields don't support |is_autofilled()|. Since
      // this is heavily relied upon by our metrics, we just don't log anything
      // for all <select> fields. Better to have less data than misleading data.
      continue;
    }

    // Log various quality metrics.
    metric_logger_->Log(AutoFillMetrics::FIELD_SUBMITTED, experiment_id);
    if (field_types.find(EMPTY_TYPE) == field_types.end() &&
        field_types.find(UNKNOWN_TYPE) == field_types.end()) {
      if (field->is_autofilled()) {
        metric_logger_->Log(AutoFillMetrics::FIELD_AUTOFILLED, experiment_id);
      } else {
        metric_logger_->Log(AutoFillMetrics::FIELD_AUTOFILL_FAILED,
                            experiment_id);

        AutoFillFieldType heuristic_type = UNKNOWN_TYPE;
        AutoFillFieldType server_type = NO_SERVER_DATA;
        std::map<std::string, const AutoFillField*>::const_iterator
            cached_field = cached_fields.find(field->FieldSignature());
        if (cached_field != cached_fields.end()) {
          heuristic_type = cached_field->second->heuristic_type();
          server_type = cached_field->second->server_type();
        }

        if (heuristic_type == UNKNOWN_TYPE) {
          metric_logger_->Log(AutoFillMetrics::FIELD_HEURISTIC_TYPE_UNKNOWN,
                              experiment_id);
        } else if (field_types.count(heuristic_type)) {
          metric_logger_->Log(AutoFillMetrics::FIELD_HEURISTIC_TYPE_MATCH,
                              experiment_id);
        } else {
          metric_logger_->Log(AutoFillMetrics::FIELD_HEURISTIC_TYPE_MISMATCH,
                              experiment_id);
        }

        if (server_type == NO_SERVER_DATA) {
          metric_logger_->Log(AutoFillMetrics::FIELD_SERVER_TYPE_UNKNOWN,
                              experiment_id);
        } else if (field_types.count(server_type)) {
          metric_logger_->Log(AutoFillMetrics::FIELD_SERVER_TYPE_MATCH,
                              experiment_id);
        } else {
          metric_logger_->Log(AutoFillMetrics::FIELD_SERVER_TYPE_MISMATCH,
                              experiment_id);
        }
      }

      // TODO(isherman): Other things we might want to log here:
      // * Per Vadim's email, a combination of (1) whether heuristics fired,
      //   (2) whether the server returned something interesting, (3) whether
      //   the user filled the field
      // * Whether the server type matches the heursitic type
      //   - Perhaps only if at least one of the types is not unknown/no data.
    }
  }
}

void AutoFillManager::ImportFormData(const FormStructure& submitted_form) {
  std::vector<const FormStructure*> import;
  import.push_back(&submitted_form);
  if (!personal_data_->ImportFormData(import))
    return;

  // Did we get credit card info?
  AutoFillProfile* profile;
  CreditCard* credit_card;
  personal_data_->GetImportedFormData(&profile, &credit_card);

  // If credit card information was submitted, show an infobar to offer to save
  // it.
  if (credit_card && tab_contents_) {
    tab_contents_->AddInfoBar(new AutoFillCCInfoBarDelegate(tab_contents_,
                                                            this));
  }
}

void AutoFillManager::UploadFormData(const FormStructure& submitted_form) {
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

void AutoFillManager::Reset() {
  form_structures_.reset();
}

void AutoFillManager::OnInfoBarClosed(bool should_save) {
  if (should_save)
    personal_data_->SaveImportedCreditCard();
}

AutoFillManager::AutoFillManager()
    : tab_contents_(NULL),
      personal_data_(NULL),
      download_manager_(NULL),
      disable_download_manager_requests_(true),
      metric_logger_(new AutoFillMetrics),
      cc_infobar_(NULL) {
}

AutoFillManager::AutoFillManager(TabContents* tab_contents,
                                 PersonalDataManager* personal_data)
    : tab_contents_(tab_contents),
      personal_data_(personal_data),
      download_manager_(NULL),
      disable_download_manager_requests_(true),
      metric_logger_(new AutoFillMetrics),
      cc_infobar_(NULL) {
  DCHECK(tab_contents);
}

void AutoFillManager::set_metric_logger(
    const AutoFillMetrics* metric_logger) {
  metric_logger_.reset(metric_logger);
}

bool AutoFillManager::GetHost(const std::vector<AutoFillProfile*>& profiles,
                              const std::vector<CreditCard*>& credit_cards,
                              RenderViewHost** host) {
  if (!IsAutoFillEnabled())
    return false;

  // No autofill data to return if the profiles are empty.
  if (profiles.empty() && credit_cards.empty())
    return false;

  *host = tab_contents_->render_view_host();
  if (!*host)
    return false;

  return true;
}

bool AutoFillManager::FindCachedForm(const FormData& form,
                                     FormStructure** form_structure) {
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

bool AutoFillManager::FindCachedFormAndField(const FormData& form,
                                             const FormField& field,
                                             FormStructure** form_structure,
                                             AutoFillField** autofill_field) {
  // Find the FormStructure that corresponds to |form|.
  if (!FindCachedForm(form, form_structure))
    return false;

  // No data to return if there are no auto-fillable fields.
  if (!(*form_structure)->autofill_count())
    return false;

  // Find the AutoFillField that corresponds to |field|.
  *autofill_field = NULL;
  for (std::vector<AutoFillField*>::const_iterator iter =
           (*form_structure)->begin();
       iter != (*form_structure)->end(); ++iter) {
    // The field list is terminated with a NULL AutoFillField, so don't try to
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

void AutoFillManager::GetProfileSuggestions(FormStructure* form,
                                            const FormField& field,
                                            AutoFillType type,
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
      unique_ids->push_back(PackGUIDs(std::string(), profile->guid()));
    }
  }

  std::vector<AutoFillFieldType> form_fields;
  form_fields.reserve(form->field_count());
  for (std::vector<AutoFillField*>::const_iterator iter = form->begin();
       iter != form->end(); ++iter) {
    // The field list is terminated with a NULL AutoFillField, so don't try to
    // dereference it.
    if (!*iter)
      break;
    form_fields.push_back((*iter)->type());
  }

  AutoFillProfile::CreateInferredLabels(&matched_profiles, &form_fields,
                                        type.field_type(), 1, labels);

  // No icons for profile suggestions.
  icons->resize(values->size());
}

void AutoFillManager::GetCreditCardSuggestions(FormStructure* form,
                                               const FormField& field,
                                               AutoFillType type,
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
        StartsWith(creditcard_field_value, field.value(), false)) {
      if (type.field_type() == CREDIT_CARD_NUMBER)
        creditcard_field_value = credit_card->ObfuscatedNumber();

      values->push_back(creditcard_field_value);
      labels->push_back(kCreditCardPrefix + credit_card->LastFourDigits());
      icons->push_back(credit_card->type());
      unique_ids->push_back(PackGUIDs(credit_card->guid(), std::string()));
    }
  }
}

void AutoFillManager::FillCreditCardFormField(const CreditCard* credit_card,
                                              AutoFillType type,
                                              webkit_glue::FormField* field) {
  DCHECK(credit_card);
  DCHECK(type.group() == AutoFillType::CREDIT_CARD);
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
  DCHECK(type.group() != AutoFillType::CREDIT_CARD);
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
      static_cast<size_t>(PhoneNumber::kPrefixLength +
                          PhoneNumber::kSuffixLength));
  if (has_valid_suffix_and_prefix &&
      field->max_length() == PhoneNumber::kPrefixLength) {
    number = number.substr(PhoneNumber::kPrefixOffset,
                           PhoneNumber::kPrefixLength);
    field->set_value(number);
  } else if (has_valid_suffix_and_prefix &&
             field->max_length() == PhoneNumber::kSuffixLength) {
    number = number.substr(PhoneNumber::kSuffixOffset,
                           PhoneNumber::kSuffixLength);
    field->set_value(number);
  } else {
    field->set_value(number);
  }
}

void AutoFillManager::ParseForms(const std::vector<FormData>& forms) {
  std::vector<FormStructure*> non_queryable_forms;
  for (std::vector<FormData>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    scoped_ptr<FormStructure> form_structure(new FormStructure(*iter));
    if (!form_structure->ShouldBeParsed(false))
      continue;

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
int AutoFillManager::PackGUIDs(const std::string& cc_guid,
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
void AutoFillManager::UnpackGUIDs(int id,
                                  std::string* cc_guid,
                                  std::string* profile_guid) {
  int cc_id = id >> std::numeric_limits<unsigned short>::digits &
      std::numeric_limits<unsigned short>::max();
  int profile_id = id & std::numeric_limits<unsigned short>::max();

  *cc_guid = IDToGUID(cc_id);
  *profile_guid = IDToGUID(profile_id);
}

int AutoFillManager::GUIDToID(const std::string& guid) {
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

const std::string AutoFillManager::IDToGUID(int id) {
  if (id == 0)
    return std::string();

  std::map<int, std::string>::const_iterator iter = id_guid_map_.find(id);
  if (iter == id_guid_map_.end()) {
    NOTREACHED();
    return std::string();
  }

  return iter->second;
}
