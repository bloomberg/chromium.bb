// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_manager.h"

#include <stddef.h>

#include <limits>
#include <map>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete_history_manager.h"
#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"
#include "chrome/browser/autofill/autofill_feedback_infobar_delegate.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/phone_number.h"
#include "chrome/browser/autofill/phone_number_i18n.h"
#include "chrome/browser/autofill/select_control_handler.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/guid.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_data_predictions.h"
#include "webkit/glue/form_field.h"

using switches::kEnableAutofillFeedback;
using webkit_glue::FormData;
using webkit_glue::FormDataPredictions;
using webkit_glue::FormField;

namespace {

// We only send a fraction of the forms to upload server.
// The rate for positive/negative matches potentially could be different.
const double kAutofillPositiveUploadRateDefaultValue = 0.20;
const double kAutofillNegativeUploadRateDefaultValue = 0.20;

const size_t kMaxRecentFormSignaturesToRemember = 3;

const string16::value_type kCreditCardPrefix[] = {'*', 0};

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
// logical form.  Returns true if any field in the given |section| within |form|
// is auto-filled.
bool SectionIsAutofilled(const FormStructure* form_structure,
                         const webkit_glue::FormData& form,
                         const string16& section) {
  // TODO(isherman): It would be nice to share most of this code with the loop
  // in |OnFillAutofillFormData()|, but I don't see a particularly clean way to
  // do that.

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
           (form_structure->field(k)->section() != section ||
            *form_structure->field(k) != form.fields[j])) {
      k++;
    }

    // If we didn't find a match, continue on to the next |form| field.
    if (k >= form_structure->field_count())
      continue;

    if (form.fields[j].is_autofilled)
      return true;

    // We found a matching field in the |form_structure|, so on the next
    // iteration we should proceed to the next |form_structure| field.
    ++i;
  }

  return false;
}

bool FormIsHTTPS(FormStructure* form) {
  return form->source_url().SchemeIs(chrome::kHttpsScheme);
}

// Check for unidentified forms among those with the most query or upload
// requests.  If found, present an infobar prompting the user to send Google
// Feedback identifying these forms.  Only executes if the appropriate flag is
// set in about:flags.
const char* kPopularFormSignatures[] = {
  "10135289994685082173",
  "7883844738557049416",
  "14651966297402649464",
  "17177862793067325164",
  "15222964025577790589",
  "14138834153984647462",
  "1522221299769735301",
  "8604254969743383026",
  "1080809576396139601",
  "10591138561307360539",
  "3483444043750493124",
  "3764098888295731941",
  "957190629194980629",
  "11314948061179499915",
  "2226179674176240706",
  "9886974103926218264",
  "16089161644523512553",
  "17395441333004474813",
  "7131540066857838464",
  "1799736626243038725",
  "4314535457620699296",
  "16597101416150066076",
  "11571064402466920341",
  "17529644200058912705",
  "17442663271235869548",
  "10423886468225016833",
  "8205718441232482003",
  "12566467866837059201",
  "14998753650075003914",
  "8463873542596795823",
  "3341181348270175432",
  "12047213380448477438",
  "7626117232464424739",
  "6755316823149690927",
  "17343480863386343671",
  "4345267765838738360"
};

void CheckForPopularForms(const std::vector<FormStructure*>& forms,
                          TabContentsWrapper* tab_contents_wrapper,
                          TabContents* tab_contents) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kEnableAutofillFeedback))
    return;

  for (std::vector<FormStructure*>::const_iterator it = forms.begin();
       it != forms.end();
       ++it) {
    std::string form_signature = (*it)->FormSignature();
    for (size_t i = 0; i < arraysize(kPopularFormSignatures); ++i) {
      if (form_signature != kPopularFormSignatures[i])
        continue;

      string16 text =
          l10n_util::GetStringUTF16(IDS_AUTOFILL_FEEDBACK_INFOBAR_TEXT);
      string16 link =
          l10n_util::GetStringUTF16(IDS_AUTOFILL_FEEDBACK_INFOBAR_LINK_TEXT);
      std::string message =
          l10n_util::GetStringFUTF8(IDS_AUTOFILL_FEEDBACK_POPULAR_FORM_MESSAGE,
                                    ASCIIToUTF16(form_signature),
                                    UTF8ToUTF16((*it)->source_url().spec()));

      tab_contents_wrapper->AddInfoBar(
          new AutofillFeedbackInfoBarDelegate(tab_contents, text, link,
                                              message));
      break;
    }
  }
}

}  // namespace

AutofillManager::AutofillManager(TabContentsWrapper* tab_contents)
    : TabContentsObserver(tab_contents->tab_contents()),
      tab_contents_wrapper_(tab_contents),
      personal_data_(NULL),
      download_manager_(tab_contents->profile()),
      disable_download_manager_requests_(false),
      metric_logger_(new AutofillMetrics),
      has_logged_autofill_enabled_(false),
      has_logged_address_suggestions_count_(false),
      did_show_suggestions_(false),
      user_did_type_(false),
      user_did_autofill_(false),
      user_did_edit_autofilled_field_(false) {
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
  prefs->RegisterDictionaryPref(prefs::kAutofillDialogPlacement);
}

// static
void AutofillManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kAutofillEnabled,
                             true,
                             PrefService::SYNCABLE_PREF);
#if defined(OS_MACOSX)
  prefs->RegisterBooleanPref(prefs::kAutofillAuxiliaryProfilesEnabled,
                             true,
                             PrefService::SYNCABLE_PREF);
#else
  prefs->RegisterBooleanPref(prefs::kAutofillAuxiliaryProfilesEnabled,
                             false,
                             PrefService::UNSYNCABLE_PREF);
#endif
  prefs->RegisterDoublePref(prefs::kAutofillPositiveUploadRate,
                            kAutofillPositiveUploadRateDefaultValue,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(prefs::kAutofillNegativeUploadRate,
                            kAutofillNegativeUploadRateDefaultValue,
                            PrefService::UNSYNCABLE_PREF);
}

void AutofillManager::DidNavigateMainFramePostCommit(
    const content::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  Reset();
}

bool AutofillManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutofillManager, message)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_FormsSeen, OnFormsSeen)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_FormSubmitted, OnFormSubmitted)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_TextFieldDidChange,
                        OnTextFieldDidChange)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_QueryFormFieldAutofill,
                        OnQueryFormFieldAutofill)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_ShowAutofillDialog,
                        OnShowAutofillDialog)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_FillAutofillFormData,
                        OnFillAutofillFormData)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_DidPreviewAutofillFormData,
                        OnDidPreviewAutofillFormData)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_DidFillAutofillFormData,
                        OnDidFillAutofillFormData)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_DidShowAutofillSuggestions,
                        OnDidShowAutofillSuggestions)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void AutofillManager::OnFormSubmitted(const FormData& form) {
  // Let AutoComplete know as well.
  tab_contents_wrapper_->autocomplete_history_manager()->OnFormSubmitted(form);

  if (!IsAutofillEnabled())
    return;

  if (tab_contents()->browser_context()->IsOffTheRecord())
    return;

  // Don't save data that was submitted through JavaScript.
  if (!form.user_submitted)
    return;

  // Grab a copy of the form data.
  FormStructure submitted_form(form);

  // Disregard forms that we wouldn't ever autofill in the first place.
  if (!submitted_form.ShouldBeParsed(true))
    return;

  // Ignore forms not present in our cache.  These are typically forms with
  // wonky JavaScript that also makes them not auto-fillable.
  FormStructure* cached_submitted_form;
  if (!FindCachedForm(form, &cached_submitted_form))
    return;
  submitted_form.UpdateFromCache(*cached_submitted_form);

  // Only upload server statistics and UMA metrics if at least some local data
  // is available to use as a baseline.
  if (!personal_data_->profiles().empty() ||
      !personal_data_->credit_cards().empty()) {
    DeterminePossibleFieldTypesForUpload(&submitted_form);
    submitted_form.LogQualityMetrics(*metric_logger_);

    if (submitted_form.ShouldBeCrowdsourced())
      UploadFormData(submitted_form);
  }

  if (!submitted_form.IsAutofillable(true))
    return;

  ImportFormData(submitted_form);
}

void AutofillManager::OnFormsSeen(const std::vector<FormData>& forms) {
  bool enabled = IsAutofillEnabled();
  if (!has_logged_autofill_enabled_) {
    metric_logger_->LogIsAutofillEnabledAtPageLoad(enabled);
    has_logged_autofill_enabled_ = true;
  }

  if (!enabled)
    return;

  ParseForms(forms);
}

void AutofillManager::OnTextFieldDidChange(const FormData& form,
                                           const FormField& field) {
  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (!FindCachedFormAndField(form, field, &form_structure, &autofill_field))
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
}

void AutofillManager::OnQueryFormFieldAutofill(
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
      form_structure->IsAutofillable(false)) {
    AutofillFieldType type = autofill_field->type();
    bool is_filling_credit_card =
        (AutofillType(type).group() == AutofillType::CREDIT_CARD);
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
      // Don't provide Autofill suggestions when Autofill is disabled, and don't
      // provide credit card suggestions for non-HTTPS pages. However, provide a
      // warning to the user in these cases.
      int warning = 0;
      if (!form_structure->IsAutofillable(true))
        warning = IDS_AUTOFILL_WARNING_FORM_DISABLED;
      else if (is_filling_credit_card && !FormIsHTTPS(form_structure))
        warning = IDS_AUTOFILL_WARNING_INSECURE_CONNECTION;
      if (warning) {
        values.assign(1, l10n_util::GetStringUTF16(warning));
        labels.assign(1, string16());
        icons.assign(1, string16());
        unique_ids.assign(1, -1);
      } else {
        bool section_is_autofilled =
            SectionIsAutofilled(form_structure, form,
                                autofill_field->section());
        if (section_is_autofilled) {
          // If the relevant section is auto-filled and the renderer is querying
          // for suggestions, then the user is editing the value of a field.
          // In this case, mimic autocomplete: don't display labels or icons,
          // as that information is redundant.
          labels.assign(labels.size(), string16());
          icons.assign(icons.size(), string16());
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
  tab_contents_wrapper_->autocomplete_history_manager()->
      OnGetAutocompleteSuggestions(
          query_id, field.name, field.value, values, labels, icons, unique_ids);
}

void AutofillManager::OnFillAutofillFormData(int query_id,
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
  GUIDPair cc_guid;
  GUIDPair profile_guid;
  UnpackGUIDs(unique_id, &cc_guid, &profile_guid);
  DCHECK(!guid::IsValidGUID(cc_guid.first) ||
         !guid::IsValidGUID(profile_guid.first));

  // Find the profile that matches the |profile_id|, if one is specified.
  const AutofillProfile* profile = NULL;
  if (guid::IsValidGUID(profile_guid.first)) {
    for (std::vector<AutofillProfile*>::const_iterator iter = profiles.begin();
         iter != profiles.end(); ++iter) {
      if ((*iter)->guid() == profile_guid.first) {
        profile = *iter;
        break;
      }
    }
    DCHECK(profile);
  }

  // Find the credit card that matches the |cc_id|, if one is specified.
  const CreditCard* credit_card = NULL;
  if (guid::IsValidGUID(cc_guid.first)) {
    for (std::vector<CreditCard*>::const_iterator iter = credit_cards.begin();
         iter != credit_cards.end(); ++iter) {
      if ((*iter)->guid() == cc_guid.first) {
        credit_card = *iter;
        break;
      }
    }
    DCHECK(credit_card);
  }

  if (!profile && !credit_card)
    return;

  FormData result = form;

  // If the relevant section is auto-filled, we should fill |field| but not the
  // rest of the form.
  if (SectionIsAutofilled(form_structure, form, autofill_field->section())) {
    for (std::vector<FormField>::iterator iter = result.fields.begin();
         iter != result.fields.end(); ++iter) {
      if ((*iter) == field) {
        AutofillFieldType field_type = autofill_field->type();
        if (profile) {
          DCHECK_NE(AutofillType::CREDIT_CARD,
                    AutofillType(field_type).group());
          FillFormField(*profile, *autofill_field, profile_guid.second,
                        &(*iter));
        } else {
          DCHECK_EQ(AutofillType::CREDIT_CARD,
                    AutofillType(field_type).group());
          FillCreditCardFormField(*credit_card, field_type, &(*iter));
        }

        // Mark the cached field as autofilled, so that we can detect when a
        // user edits an autofilled field (for metrics).
        autofill_field->is_autofilled = true;
        break;
      }
    }

    host->Send(new AutofillMsg_FormDataFilled(host->routing_id(), query_id,
                                              result));
    return;
  }

  // The list of fields in |form_structure| and |result.fields| often match
  // directly and we can fill these corresponding fields; however, when the
  // |form_structure| and |result.fields| do not match directly we search
  // ahead in the |form_structure| for the matching field.
  // See unit tests: AutofillManagerTest.FormChangesRemoveField and
  // AutofillManagerTest.FormChangesAddField for usage.
  for (size_t i = 0, j = 0;
       i < form_structure->field_count() && j < result.fields.size();
       j++) {
    size_t k = i;

    // Search forward in the |form_structure| for a corresponding field.
    while (k < form_structure->field_count() &&
           (form_structure->field(k)->section() != autofill_field->section() ||
            *form_structure->field(k) != result.fields[j])) {
      k++;
    }

    // If we've found a match then fill the |result| field with the found
    // field in the |form_structure|.
    if (k >= form_structure->field_count())
      continue;

    const AutofillField* cached_field = form_structure->field(k);
    AutofillFieldType field_type = cached_field->type();
    FieldTypeGroup field_group_type = AutofillType(field_type).group();
    if (field_group_type != AutofillType::NO_GROUP) {
      if (profile) {
        DCHECK_NE(AutofillType::CREDIT_CARD, field_group_type);
        // If the field being filled is the field that the user initiated the
        // fill from, then take the multi-profile "variant" into account.
        // Otherwise fill with the default (zeroth) variant.
        if (result.fields[j] == field) {
          FillFormField(*profile, *cached_field, profile_guid.second,
                        &result.fields[j]);
        } else {
          FillFormField(*profile, *cached_field, 0, &result.fields[j]);
        }
      } else {
        DCHECK_EQ(AutofillType::CREDIT_CARD, field_group_type);
        FillCreditCardFormField(*credit_card, field_type, &result.fields[j]);
      }

      // Mark the cached field as autofilled, so that we can detect when a user
      // edits an autofilled field (for metrics).
      form_structure->field(k)->is_autofilled = true;
    }

    // We found a matching field in the |form_structure|, so on the next
    // iteration we should proceed to the next |form_structure| field.
    ++i;
  }

  autofilled_form_signatures_.push_front(form_structure->FormSignature());
  // Only remember the last few forms that we've seen, both to avoid false
  // positives and to avoid wasting memory.
  if (autofilled_form_signatures_.size() > kMaxRecentFormSignaturesToRemember)
    autofilled_form_signatures_.pop_back();

  host->Send(new AutofillMsg_FormDataFilled(
      host->routing_id(), query_id, result));
}

void AutofillManager::OnShowAutofillDialog() {
  Browser* browser = BrowserList::GetLastActiveWithProfile(
      Profile::FromBrowserContext(tab_contents()->browser_context()));
  if (browser)
    browser->ShowOptionsTab(chrome::kAutofillSubPage);
}

void AutofillManager::OnDidPreviewAutofillFormData() {
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA,
      Source<RenderViewHost>(tab_contents()->render_view_host()),
      NotificationService::NoDetails());
}


void AutofillManager::OnDidFillAutofillFormData() {
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA,
      Source<RenderViewHost>(tab_contents()->render_view_host()),
      NotificationService::NoDetails());

  metric_logger_->LogUserHappinessMetric(AutofillMetrics::USER_DID_AUTOFILL);
  if (!user_did_autofill_) {
    user_did_autofill_ = true;
    metric_logger_->LogUserHappinessMetric(
        AutofillMetrics::USER_DID_AUTOFILL_ONCE);
  }
}

void AutofillManager::OnDidShowAutofillSuggestions(bool is_new_popup) {
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_DID_SHOW_SUGGESTIONS,
      Source<RenderViewHost>(tab_contents()->render_view_host()),
      NotificationService::NoDetails());

  if (is_new_popup) {
    metric_logger_->LogUserHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);

    if (!did_show_suggestions_) {
      did_show_suggestions_ = true;
      metric_logger_->LogUserHappinessMetric(
          AutofillMetrics::SUGGESTIONS_SHOWN_ONCE);
    }
  }
}

void AutofillManager::OnLoadedServerPredictions(
    const std::string& response_xml) {
  // Parse and store the server predictions.
  FormStructure::ParseQueryResponse(response_xml,
                                    form_structures_.get(),
                                    *metric_logger_);

  // If the corresponding flag is set, annotate forms with the predicted types.
  RenderViewHost* host = tab_contents()->render_view_host();
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShowAutofillTypePredictions) || !host) {
    return;
  }

  std::vector<FormDataPredictions> forms;
  FormStructure::GetFieldTypePredictions(form_structures_.get(), &forms);
  host->Send(new AutofillMsg_FieldTypePredictionsAvailable(host->routing_id(),
                                                           forms));
}

void AutofillManager::OnUploadedPossibleFieldTypes() {
}

void AutofillManager::OnServerRequestError(
    const std::string& form_signature,
    AutofillDownloadManager::AutofillRequestType request_type,
    int http_error) {
}

bool AutofillManager::IsAutofillEnabled() const {
  Profile* profile = Profile::FromBrowserContext(
      const_cast<AutofillManager*>(this)->tab_contents()->browser_context());
  return profile->GetPrefs()->GetBoolean(prefs::kAutofillEnabled);
}

void AutofillManager::DeterminePossibleFieldTypesForUpload(
    FormStructure* submitted_form) {
  // Combine all the profiles and credit cards stored in |personal_data_| into
  // one vector for ease of iteration.
  const std::vector<AutofillProfile*>& profiles = personal_data_->profiles();
  const std::vector<CreditCard*>& credit_cards = personal_data_->credit_cards();
  std::vector<FormGroup*> stored_data;
  stored_data.insert(stored_data.end(), profiles.begin(), profiles.end());
  stored_data.insert(stored_data.end(), credit_cards.begin(),
                     credit_cards.end());

  // For each field in the |submitted_form|, extract the value.  Then for each
  // profile or credit card, identify any stored types that match the value.
  for (size_t i = 0; i < submitted_form->field_count(); i++) {
    AutofillField* field = submitted_form->field(i);
    string16 value = CollapseWhitespace(field->value, false);
    FieldTypeSet matching_types;
    for (std::vector<FormGroup*>::const_iterator it = stored_data.begin();
         it != stored_data.end(); ++it) {
      (*it)->GetMatchingTypes(value, &matching_types);
    }

    if (matching_types.empty())
      matching_types.insert(UNKNOWN_TYPE);

    field->set_possible_types(matching_types);
  }
}

void AutofillManager::ImportFormData(const FormStructure& submitted_form) {
  const CreditCard* imported_credit_card;
  if (!personal_data_->ImportFormData(submitted_form, &imported_credit_card))
    return;

  // If credit card information was submitted, show an infobar to offer to save
  // it.
  scoped_ptr<const CreditCard> scoped_credit_card(imported_credit_card);
  if (imported_credit_card && tab_contents()) {
    tab_contents_wrapper_->AddInfoBar(
        new AutofillCCInfoBarDelegate(tab_contents(),
                                      scoped_credit_card.release(),
                                      personal_data_,
                                      metric_logger_.get()));
  }
}

void AutofillManager::UploadFormData(const FormStructure& submitted_form) {
  if (disable_download_manager_requests_)
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

  FieldTypeSet non_empty_types;
  personal_data_->GetNonEmptyTypes(&non_empty_types);

  download_manager_.StartUploadRequest(submitted_form, was_autofilled,
                                       non_empty_types);
}

void AutofillManager::Reset() {
  form_structures_.reset();
  has_logged_autofill_enabled_ = false;
  has_logged_address_suggestions_count_ = false;
  did_show_suggestions_ = false;
  user_did_type_ = false;
  user_did_autofill_ = false;
  user_did_edit_autofilled_field_ = false;
}

AutofillManager::AutofillManager(TabContentsWrapper* tab_contents,
                                 PersonalDataManager* personal_data)
    : TabContentsObserver(tab_contents->tab_contents()),
      tab_contents_wrapper_(tab_contents),
      personal_data_(personal_data),
      download_manager_(NULL),
      disable_download_manager_requests_(true),
      metric_logger_(new AutofillMetrics),
      has_logged_autofill_enabled_(false),
      has_logged_address_suggestions_count_(false),
      did_show_suggestions_(false),
      user_did_type_(false),
      user_did_autofill_(false),
      user_did_edit_autofilled_field_(false)  {
  DCHECK(tab_contents);
}

void AutofillManager::set_metric_logger(const AutofillMetrics* metric_logger) {
  metric_logger_.reset(metric_logger);
}

bool AutofillManager::GetHost(const std::vector<AutofillProfile*>& profiles,
                              const std::vector<CreditCard*>& credit_cards,
                              RenderViewHost** host) const {
  if (!IsAutofillEnabled())
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
                                            AutofillFieldType type,
                                            std::vector<string16>* values,
                                            std::vector<string16>* labels,
                                            std::vector<string16>* icons,
                                            std::vector<int>* unique_ids) {
  const std::vector<AutofillProfile*>& profiles = personal_data_->profiles();
  if (!field.is_autofilled) {
    std::vector<AutofillProfile*> matched_profiles;
    for (std::vector<AutofillProfile*>::const_iterator iter = profiles.begin();
         iter != profiles.end(); ++iter) {
      AutofillProfile* profile = *iter;

      // The value of the stored data for this field type in the |profile|.
      std::vector<string16> multi_values;
      profile->GetCanonicalizedMultiInfo(type, &multi_values);

      for (size_t i = 0; i < multi_values.size(); ++i) {
        if (!multi_values[i].empty() &&
            StartsWith(multi_values[i], field.value, false)) {
          matched_profiles.push_back(profile);
          values->push_back(multi_values[i]);
          unique_ids->push_back(PackGUIDs(GUIDPair(std::string(), 0),
                                          GUIDPair(profile->guid(), i)));
        }
      }
    }

    std::vector<AutofillFieldType> form_fields;
    form_fields.reserve(form->field_count());
    for (std::vector<AutofillField*>::const_iterator iter = form->begin();
         iter != form->end(); ++iter) {
      form_fields.push_back((*iter)->type());
    }

    AutofillProfile::CreateInferredLabels(&matched_profiles, &form_fields,
                                          type, 1, labels);

    // No icons for profile suggestions.
    icons->resize(values->size());
  } else {
    for (std::vector<AutofillProfile*>::const_iterator iter = profiles.begin();
         iter != profiles.end(); ++iter) {
      AutofillProfile* profile = *iter;

      // The value of the stored data for this field type in the |profile|.
      std::vector<string16> multi_values;
      profile->GetCanonicalizedMultiInfo(type, &multi_values);

      for (size_t i = 0; i < multi_values.size(); ++i) {
        if (multi_values[i].empty())
          continue;
        string16 profile_value_lower_case(StringToLowerASCII(multi_values[i]));
        string16 field_value_lower_case(StringToLowerASCII(field.value));
        // Phone numbers could be split in US forms, so field value could be
        // either prefix or suffix of the phone.
        bool matched_phones = false;
        if ((type == PHONE_HOME_NUMBER || type == PHONE_FAX_NUMBER) &&
            !field_value_lower_case.empty() &&
            (profile_value_lower_case.find(field_value_lower_case) !=
             string16::npos)) {
          matched_phones = true;
        }
        if (matched_phones ||
            profile_value_lower_case == field_value_lower_case) {
          for (size_t j = 0; j < multi_values.size(); ++j) {
            if (!multi_values[j].empty()) {
              values->push_back(multi_values[j]);
              unique_ids->push_back(PackGUIDs(GUIDPair(std::string(), 0),
                                              GUIDPair(profile->guid(), j)));
            }
          }
          // We've added all the values for this profile so move on to the next.
          break;
        }
      }
    }

    // No labels for previously filled fields.
    labels->resize(values->size());

    // No icons for profile suggestions.
    icons->resize(values->size());
  }
}

void AutofillManager::GetCreditCardSuggestions(FormStructure* form,
                                               const FormField& field,
                                               AutofillFieldType type,
                                               std::vector<string16>* values,
                                               std::vector<string16>* labels,
                                               std::vector<string16>* icons,
                                               std::vector<int>* unique_ids) {
  for (std::vector<CreditCard*>::const_iterator iter =
           personal_data_->credit_cards().begin();
       iter != personal_data_->credit_cards().end(); ++iter) {
    CreditCard* credit_card = *iter;

    // The value of the stored data for this field type in the |credit_card|.
    string16 creditcard_field_value =
        credit_card->GetCanonicalizedInfo(type);
    if (!creditcard_field_value.empty() &&
        StartsWith(creditcard_field_value, field.value, false)) {
      if (type == CREDIT_CARD_NUMBER)
        creditcard_field_value = credit_card->ObfuscatedNumber();

      string16 label;
      if (credit_card->number().empty()) {
        // If there is no CC number, return name to show something.
        label = credit_card->GetCanonicalizedInfo(CREDIT_CARD_NAME);
      } else {
        label = kCreditCardPrefix;
        label.append(credit_card->LastFourDigits());
      }

      values->push_back(creditcard_field_value);
      labels->push_back(label);
      icons->push_back(UTF8ToUTF16(credit_card->type()));
      unique_ids->push_back(PackGUIDs(GUIDPair(credit_card->guid(), 0),
                                      GUIDPair(std::string(), 0)));
    }
  }
}

void AutofillManager::FillCreditCardFormField(const CreditCard& credit_card,
                                              AutofillFieldType type,
                                              webkit_glue::FormField* field) {
  DCHECK_EQ(AutofillType::CREDIT_CARD, AutofillType(type).group());
  DCHECK(field);

  if (field->form_control_type == ASCIIToUTF16("select-one")) {
    autofill::FillSelectControl(credit_card, type, field);
  } else if (field->form_control_type == ASCIIToUTF16("month")) {
    // HTML5 input="month" consists of year-month.
    string16 year =
        credit_card.GetCanonicalizedInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR);
    string16 month = credit_card.GetCanonicalizedInfo(CREDIT_CARD_EXP_MONTH);
    if (!year.empty() && !month.empty()) {
      // Fill the value only if |credit_card| includes both year and month
      // information.
      field->value = year + ASCIIToUTF16("-") + month;
    }
  } else {
    field->value = credit_card.GetCanonicalizedInfo(type);
  }
}

void AutofillManager::FillFormField(const AutofillProfile& profile,
                                    const AutofillField& cached_field,
                                    size_t variant,
                                    webkit_glue::FormField* field) {
  AutofillFieldType type = cached_field.type();
  DCHECK_NE(AutofillType::CREDIT_CARD, AutofillType(type).group());
  DCHECK(field);

  if (AutofillType(type).subgroup() == AutofillType::PHONE_NUMBER) {
    FillPhoneNumberField(profile, cached_field, variant, field);
  } else {
    if (field->form_control_type == ASCIIToUTF16("select-one")) {
      autofill::FillSelectControl(profile, type, field);
    } else {
      std::vector<string16> values;
      profile.GetCanonicalizedMultiInfo(type, &values);
      DCHECK(variant < values.size());
      field->value = values[variant];
    }
  }
}

void AutofillManager::FillPhoneNumberField(const AutofillProfile& profile,
                                           const AutofillField& cached_field,
                                           size_t variant,
                                           webkit_glue::FormField* field) {
  std::vector<string16> values;
  profile.GetCanonicalizedMultiInfo(cached_field.type(), &values);
  DCHECK(variant < values.size());

  // If we are filling a phone number, check to see if the size field
  // matches the "prefix" or "suffix" sizes and fill accordingly.
  string16 number = values[variant];
  if (number.length() ==
          (PhoneNumber::kPrefixLength + PhoneNumber::kSuffixLength)) {
    if (cached_field.phone_part() == AutofillField::PHONE_PREFIX ||
        field->max_length == PhoneNumber::kPrefixLength) {
      number = number.substr(PhoneNumber::kPrefixOffset,
                             PhoneNumber::kPrefixLength);
    } else if (cached_field.phone_part() == AutofillField::PHONE_SUFFIX ||
               field->max_length == PhoneNumber::kSuffixLength) {
      number = number.substr(PhoneNumber::kSuffixOffset,
                             PhoneNumber::kSuffixLength);
    }
  }

  field->value = number;
}

void AutofillManager::ParseForms(const std::vector<FormData>& forms) {
  std::vector<FormStructure*> non_queryable_forms;
  for (std::vector<FormData>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    scoped_ptr<FormStructure> form_structure(new FormStructure(*iter));
    if (!form_structure->ShouldBeParsed(false))
      continue;

    form_structure->DetermineHeuristicTypes();

    // Set aside forms with method GET or author-specified types, so that they
    // are not included in the query to the server.
    if (form_structure->ShouldBeCrowdsourced())
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

  if (!form_structures_.empty())
    metric_logger_->LogUserHappinessMetric(AutofillMetrics::FORMS_LOADED);

  CheckForPopularForms(form_structures_.get(), tab_contents_wrapper_,
                       tab_contents());
}

int AutofillManager::GUIDToID(const GUIDPair& guid) {
  static int last_id = 1;

  if (!guid::IsValidGUID(guid.first))
    return 0;

  std::map<GUIDPair, int>::const_iterator iter = guid_id_map_.find(guid);
  if (iter == guid_id_map_.end()) {
    guid_id_map_[guid] = last_id;
    id_guid_map_[last_id] = guid;
    return last_id++;
  } else {
    return iter->second;
  }
}

const AutofillManager::GUIDPair AutofillManager::IDToGUID(int id) {
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
                               const GUIDPair& profile_guid) {
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
                                  GUIDPair* profile_guid) {
  int cc_id = id >> std::numeric_limits<unsigned short>::digits &
      std::numeric_limits<unsigned short>::max();
  int profile_id = id & std::numeric_limits<unsigned short>::max();

  *cc_guid = IDToGUID(cc_id);
  *profile_guid = IDToGUID(profile_id);
}
