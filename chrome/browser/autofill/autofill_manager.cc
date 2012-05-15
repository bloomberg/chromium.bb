// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_manager.h"

#include <stddef.h>

#include <limits>
#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete_history_manager.h"
#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "chrome/browser/autofill/autofill_feedback_infobar_delegate.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/phone_number.h"
#include "chrome/browser/autofill/phone_number_i18n.h"
#include "chrome/browser/autofill/select_control_handler.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/guid.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutofillClient.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"
#include "webkit/forms/form_data.h"
#include "webkit/forms/form_data_predictions.h"
#include "webkit/forms/form_field.h"
#include "webkit/forms/password_form_dom_manager.h"

using base::TimeTicks;
using content::BrowserThread;
using content::RenderViewHost;
using switches::kEnableAutofillFeedback;
using webkit::forms::FormData;
using webkit::forms::FormDataPredictions;
using webkit::forms::FormField;

namespace {

// We only send a fraction of the forms to upload server.
// The rate for positive/negative matches potentially could be different.
const double kAutofillPositiveUploadRateDefaultValue = 0.20;
const double kAutofillNegativeUploadRateDefaultValue = 0.20;

const size_t kMaxRecentFormSignaturesToRemember = 3;

// Set a conservative upper bound on the number of forms we are willing to
// cache, simply to prevent unbounded memory consumption.
const size_t kMaxFormCacheSize = 100;

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
bool SectionIsAutofilled(const FormStructure& form_structure,
                         const FormData& form,
                         const string16& section) {
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
  return form.source_url().SchemeIs(chrome::kHttpsScheme);
}

// Uses the existing personal data in |profiles| and |credit_cards| to determine
// possible field types for the |submitted_form|.  This is potentially
// expensive -- on the order of 50ms even for a small set of |stored_data|.
// Hence, it should not run on the UI thread -- to avoid locking up the UI --
// nor on the IO thread -- to avoid blocking IPC calls.
void DeterminePossibleFieldTypesForUpload(
    const std::vector<AutofillProfile>& profiles,
    const std::vector<CreditCard>& credit_cards,
    FormStructure* submitted_form) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  // For each field in the |submitted_form|, extract the value.  Then for each
  // profile or credit card, identify any stored types that match the value.
  for (size_t i = 0; i < submitted_form->field_count(); ++i) {
    AutofillField* field = submitted_form->field(i);
    string16 value = CollapseWhitespace(field->value, false);

    FieldTypeSet matching_types;
    for (std::vector<AutofillProfile>::const_iterator it = profiles.begin();
         it != profiles.end(); ++it) {
      it->GetMatchingTypes(value, &matching_types);
    }
    for (std::vector<CreditCard>::const_iterator it = credit_cards.begin();
          it != credit_cards.end(); ++it) {
      it->GetMatchingTypes(value, &matching_types);
    }

    if (matching_types.empty())
      matching_types.insert(UNKNOWN_TYPE);

    field->set_possible_types(matching_types);
  }
}

}  // namespace

AutofillManager::AutofillManager(TabContentsWrapper* tab_contents)
    : content::WebContentsObserver(tab_contents->web_contents()),
      tab_contents_wrapper_(tab_contents),
      personal_data_(NULL),
      download_manager_(tab_contents->profile(), this),
      disable_download_manager_requests_(false),
      metric_logger_(new AutofillMetrics),
      has_logged_autofill_enabled_(false),
      has_logged_address_suggestions_count_(false),
      did_show_suggestions_(false),
      user_did_type_(false),
      user_did_autofill_(false),
      user_did_edit_autofilled_field_(false),
      password_generation_enabled_(false),
      external_delegate_(NULL) {
  // |personal_data_| is NULL when using test-enabled WebContents.
  personal_data_ = PersonalDataManagerFactory::GetForProfile(
      tab_contents->profile()->GetOriginalProfile());
  RegisterWithSyncService();
}

AutofillManager::~AutofillManager() {
  if (sync_service_ && sync_service_->HasObserver(this))
    sync_service_->RemoveObserver(this);
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

void AutofillManager::RegisterWithSyncService() {
  ProfileSyncService* temp_sync_service =
      ProfileSyncServiceFactory::GetForProfile(
          tab_contents_wrapper_->profile());
  if (temp_sync_service) {
    sync_service_ = temp_sync_service->AsWeakPtr();
    sync_service_->AddObserver(this);
  }
}

void AutofillManager::SendPasswordGenerationStateToRenderer(
    content::RenderViewHost* host, bool enabled) {
  host->Send(new AutofillMsg_PasswordGenerationEnabled(host->GetRoutingID(),
                                                       enabled));
}

void AutofillManager::UpdatePasswordGenerationState(
    content::RenderViewHost* host,
    bool new_renderer) {
  if (!sync_service_)
    return;

  // Password generation requires sync for passwords and the password manager
  // to both be enabled.
  syncable::ModelTypeSet sync_set = sync_service_->GetPreferredDataTypes();
  bool password_sync_enabled = (sync_service_->HasSyncSetupCompleted() &&
                                sync_set.Has(syncable::PASSWORDS));
  bool new_password_generation_enabled =
      password_sync_enabled &&
      tab_contents_wrapper_->password_manager()->IsSavingEnabled();
  if (new_password_generation_enabled != password_generation_enabled_ ||
      new_renderer) {
    password_generation_enabled_ = new_password_generation_enabled;
    SendPasswordGenerationStateToRenderer(host, password_generation_enabled_);
  }
}

void AutofillManager::RenderViewCreated(content::RenderViewHost* host) {
      UpdatePasswordGenerationState(host, true);
}

void AutofillManager::OnStateChanged() {
  // It is possible for sync state to change during tab contents destruction.
  // In this case, we don't need to update the renderer since it's going away.
  if (web_contents() && web_contents()->GetRenderViewHost()) {
    UpdatePasswordGenerationState(web_contents()->GetRenderViewHost(),
                                  false);
  }
}

void AutofillManager::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  Reset();
}

bool AutofillManager::HasExternalDelegate() {
  return external_delegate_ != NULL;
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
    IPC_MESSAGE_HANDLER(AutofillHostMsg_DidEndTextFieldEditing,
                        OnDidEndTextFieldEditing)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_HideAutofillPopup,
                        OnHideAutofillPopup)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_ShowPasswordGenerationPopup,
                        OnShowPasswordGenerationPopup)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_AddPasswordFormMapping,
                        OnAddPasswordFormMapping)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_ShowPasswordSuggestions,
                        OnShowPasswordSuggestions)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

bool AutofillManager::OnFormSubmitted(const FormData& form,
                                      const TimeTicks& timestamp) {
  // Let AutoComplete know as well.
  tab_contents_wrapper_->autocomplete_history_manager()->OnFormSubmitted(form);

  if (!IsAutofillEnabled())
    return false;

  if (web_contents()->GetBrowserContext()->IsOffTheRecord())
    return false;

  // Don't save data that was submitted through JavaScript.
  if (!form.user_submitted)
    return false;

  // Grab a copy of the form data.
  scoped_ptr<FormStructure> submitted_form(new FormStructure(form));

  // Disregard forms that we wouldn't ever autofill in the first place.
  if (!submitted_form->ShouldBeParsed(true))
    return false;

  // Ignore forms not present in our cache.  These are typically forms with
  // wonky JavaScript that also makes them not auto-fillable.
  FormStructure* cached_submitted_form;
  if (!FindCachedForm(form, &cached_submitted_form))
    return false;

  submitted_form->UpdateFromCache(*cached_submitted_form);
  if (submitted_form->IsAutofillable(true))
    ImportFormData(*submitted_form);

  // Only upload server statistics and UMA metrics if at least some local data
  // is available to use as a baseline.
  const std::vector<AutofillProfile*>& profiles = personal_data_->profiles();
  const std::vector<CreditCard*>& credit_cards = personal_data_->credit_cards();
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
    BrowserThread::GetBlockingPool()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&DeterminePossibleFieldTypesForUpload,
                   copied_profiles,
                   copied_credit_cards,
                   raw_submitted_form),
        base::Bind(&AutofillManager::UploadFormDataAsyncCallback,
                   this,
                   base::Owned(submitted_form.release()),
                   forms_loaded_timestamp_,
                   initial_interaction_timestamp_,
                   timestamp));
  }

  return true;
}

void AutofillManager::OnFormsSeen(const std::vector<FormData>& forms,
                                  const TimeTicks& timestamp) {
  bool enabled = IsAutofillEnabled();
  if (!has_logged_autofill_enabled_) {
    metric_logger_->LogIsAutofillEnabledAtPageLoad(enabled);
    has_logged_autofill_enabled_ = true;
  }

  if (!enabled)
    return;

  forms_loaded_timestamp_ = timestamp;
  ParseForms(forms);
}

void AutofillManager::OnTextFieldDidChange(const FormData& form,
                                           const FormField& field,
                                           const TimeTicks& timestamp) {
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
                                               const FormField& field,
                                               const gfx::Rect& bounding_box,
                                               bool display_warning) {
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;

  if (external_delegate_) {
    external_delegate_->OnQuery(query_id,
                                form,
                                field,
                                bounding_box,
                                display_warning);
  }

  RenderViewHost* host = NULL;
  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (GetHost(
          personal_data_->profiles(), personal_data_->credit_cards(), &host) &&
      GetCachedFormAndField(form, field, &form_structure, &autofill_field) &&
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
      else if (is_filling_credit_card && !FormIsHTTPS(*form_structure))
        warning = IDS_AUTOFILL_WARNING_INSECURE_CONNECTION;
      if (warning) {
        values.assign(1, l10n_util::GetStringUTF16(warning));
        labels.assign(1, string16());
        icons.assign(1, string16());
        unique_ids.assign(1,
                          WebKit::WebAutofillClient::MenuItemIDWarningMessage);
      } else {
        bool section_is_autofilled =
            SectionIsAutofilled(*form_structure, form,
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
  const AutofillProfile* profile = NULL;
  const CreditCard* credit_card = NULL;
  size_t variant = 0;
  RenderViewHost* host = NULL;
  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (!GetProfileOrCreditCard(unique_id, profiles, credit_cards, &profile,
                              &credit_card, &variant) ||
      !GetHost(profiles, credit_cards, &host) ||
      !GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  DCHECK(host);
  DCHECK(form_structure);
  DCHECK(autofill_field);
  DCHECK(profile || credit_card);

  FormData result = form;

  // If the relevant section is auto-filled, we should fill |field| but not the
  // rest of the form.
  if (SectionIsAutofilled(*form_structure, form, autofill_field->section())) {
    for (std::vector<FormField>::iterator iter = result.fields.begin();
         iter != result.fields.end(); ++iter) {
      if ((*iter) == field) {
        AutofillFieldType field_type = autofill_field->type();
        if (profile) {
          DCHECK_NE(AutofillType::CREDIT_CARD,
                    AutofillType(field_type).group());
          FillFormField(*profile, *autofill_field, variant, &(*iter));
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

    host->Send(new AutofillMsg_FormDataFilled(host->GetRoutingID(), query_id,
                                              result));
    return;
  }

  // Cache the field type for the field from which the user initiated autofill.
  FieldTypeGroup initiating_group_type =
      AutofillType(autofill_field->type()).group();
  DCHECK_EQ(form_structure->field_count(), form.fields.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    if (form_structure->field(i)->section() != autofill_field->section())
      continue;

    DCHECK_EQ(*form_structure->field(i), result.fields[i]);

    const AutofillField* cached_field = form_structure->field(i);
    AutofillFieldType field_type = cached_field->type();
    FieldTypeGroup field_group_type = AutofillType(field_type).group();
    if (field_group_type != AutofillType::NO_GROUP) {
      if (profile) {
        DCHECK_NE(AutofillType::CREDIT_CARD, field_group_type);
        // If the field being filled is either
        //   (a) the field that the user initiated the fill from, or
        //   (b) part of the same logical unit, e.g. name or phone number,
        // then take the multi-profile "variant" into account.
        // Otherwise fill with the default (zeroth) variant.
        if (result.fields[i] == field ||
            field_group_type == initiating_group_type) {
          FillFormField(*profile, *cached_field, variant, &result.fields[i]);
        } else {
          FillFormField(*profile, *cached_field, 0, &result.fields[i]);
        }
      } else {
        DCHECK_EQ(AutofillType::CREDIT_CARD, field_group_type);
        FillCreditCardFormField(*credit_card, field_type, &result.fields[i]);
      }

      // Mark the cached field as autofilled, so that we can detect when a user
      // edits an autofilled field (for metrics).
      form_structure->field(i)->is_autofilled = true;
    }
  }

  autofilled_form_signatures_.push_front(form_structure->FormSignature());
  // Only remember the last few forms that we've seen, both to avoid false
  // positives and to avoid wasting memory.
  if (autofilled_form_signatures_.size() > kMaxRecentFormSignaturesToRemember)
    autofilled_form_signatures_.pop_back();

  host->Send(new AutofillMsg_FormDataFilled(
      host->GetRoutingID(), query_id, result));
}

void AutofillManager::OnShowAutofillDialog() {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
#else
  Browser* browser = BrowserList::GetLastActiveWithProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (browser)
    browser->ShowOptionsTab(chrome::kAutofillSubPage);
#endif  // #if defined(OS_ANDROID)
}

void AutofillManager::OnDidPreviewAutofillFormData() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA,
      content::Source<RenderViewHost>(web_contents()->GetRenderViewHost()),
      content::NotificationService::NoDetails());
}

void AutofillManager::OnDidFillAutofillFormData(const TimeTicks& timestamp) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA,
      content::Source<RenderViewHost>(web_contents()->GetRenderViewHost()),
      content::NotificationService::NoDetails());

  metric_logger_->LogUserHappinessMetric(AutofillMetrics::USER_DID_AUTOFILL);
  if (!user_did_autofill_) {
    user_did_autofill_ = true;
    metric_logger_->LogUserHappinessMetric(
        AutofillMetrics::USER_DID_AUTOFILL_ONCE);
  }

  UpdateInitialInteractionTimestamp(timestamp);
}

void AutofillManager::OnDidShowAutofillSuggestions(bool is_new_popup) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_DID_SHOW_SUGGESTIONS,
      content::Source<RenderViewHost>(web_contents()->GetRenderViewHost()),
      content::NotificationService::NoDetails());

  if (is_new_popup) {
    metric_logger_->LogUserHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);

    if (!did_show_suggestions_) {
      did_show_suggestions_ = true;
      metric_logger_->LogUserHappinessMetric(
          AutofillMetrics::SUGGESTIONS_SHOWN_ONCE);
    }
  }
}

void AutofillManager::OnHideAutofillPopup() {
  if (external_delegate_)
    external_delegate_->HideAutofillPopup();
}

void AutofillManager::OnShowPasswordGenerationPopup(const gfx::Rect& bounds) {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
#else
  Browser* browser = BrowserList::GetLastActiveWithProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  browser->window()->ShowPasswordGenerationBubble(bounds);
#endif  // #if defined(OS_ANDROID)
}

void AutofillManager::RemoveAutofillProfileOrCreditCard(int unique_id) {
  const std::vector<AutofillProfile*>& profiles = personal_data_->profiles();
  const std::vector<CreditCard*>& credit_cards = personal_data_->credit_cards();
  const AutofillProfile* profile = NULL;
  const CreditCard* credit_card = NULL;
  size_t variant = 0;
  if (!GetProfileOrCreditCard(unique_id, profiles, credit_cards, &profile,
                              &credit_card, &variant)) {
    NOTREACHED();
    return;
  }

  // TODO(csharp): If we are dealing with a variant only the variant should
  // be deleted, instead of doing nothing.
  // http://crbug.com/124211
  if (variant != 0)
    return;

  if (profile)
    personal_data_->RemoveProfile(profile->guid());
  else
    personal_data_->RemoveCreditCard(credit_card->guid());
}

void AutofillManager::OnAddPasswordFormMapping(
      const webkit::forms::FormField& form,
      const webkit::forms::PasswordFormFillData& fill_data) {
  if (external_delegate_)
    external_delegate_->AddPasswordFormMapping(form, fill_data);
}

void AutofillManager::OnShowPasswordSuggestions(
    const webkit::forms::FormField& field,
    const gfx::Rect& bounds,
    const std::vector<string16>& suggestions) {
  if (external_delegate_)
    external_delegate_->OnShowPasswordSuggestions(suggestions, field, bounds);
}

void AutofillManager::OnLoadedServerPredictions(
    const std::string& response_xml) {
  // Parse and store the server predictions.
  FormStructure::ParseQueryResponse(response_xml,
                                    form_structures_.get(),
                                    *metric_logger_);

  // If the corresponding flag is set, annotate forms with the predicted types.
  SendAutofillTypePredictions(form_structures_.get());
}

void AutofillManager::OnDidEndTextFieldEditing() {
  if (external_delegate_)
    external_delegate_->DidEndTextFieldEditing();
}

bool AutofillManager::IsAutofillEnabled() const {
  Profile* profile = Profile::FromBrowserContext(
      const_cast<AutofillManager*>(this)->web_contents()->GetBrowserContext());
  return profile->GetPrefs()->GetBoolean(prefs::kAutofillEnabled);
}

void AutofillManager::SendAutofillTypePredictions(
    const std::vector<FormStructure*>& forms) const {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kShowAutofillTypePredictions))
    return;

  RenderViewHost* host = web_contents()->GetRenderViewHost();
  if (!host)
    return;

  std::vector<FormDataPredictions> type_predictions;
  FormStructure::GetFieldTypePredictions(forms, &type_predictions);
  host->Send(
      new AutofillMsg_FieldTypePredictionsAvailable(host->GetRoutingID(),
                                                    type_predictions));
}

void AutofillManager::ImportFormData(const FormStructure& submitted_form) {
  const CreditCard* imported_credit_card;
  if (!personal_data_->ImportFormData(submitted_form, &imported_credit_card))
    return;

  // If credit card information was submitted, show an infobar to offer to save
  // it.
  scoped_ptr<const CreditCard> scoped_credit_card(imported_credit_card);
  if (imported_credit_card && web_contents()) {
    InfoBarTabHelper* infobar_helper =
        tab_contents_wrapper_->infobar_tab_helper();
    infobar_helper->AddInfoBar(
        new AutofillCCInfoBarDelegate(infobar_helper,
                                      scoped_credit_card.release(),
                                      personal_data_,
                                      metric_logger_.get()));
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
  forms_loaded_timestamp_ = TimeTicks();
  initial_interaction_timestamp_ = TimeTicks();

  if (external_delegate_)
    external_delegate_->Reset();
}

AutofillManager::AutofillManager(TabContentsWrapper* tab_contents,
                                 PersonalDataManager* personal_data)
    : content::WebContentsObserver(tab_contents->web_contents()),
      tab_contents_wrapper_(tab_contents),
      personal_data_(personal_data),
      download_manager_(tab_contents->profile(), this),
      disable_download_manager_requests_(true),
      metric_logger_(new AutofillMetrics),
      has_logged_autofill_enabled_(false),
      has_logged_address_suggestions_count_(false),
      did_show_suggestions_(false),
      user_did_type_(false),
      user_did_autofill_(false),
      user_did_edit_autofilled_field_(false),
      password_generation_enabled_(false),
      external_delegate_(NULL) {
  DCHECK(tab_contents);
  RegisterWithSyncService();
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

  *host = web_contents()->GetRenderViewHost();
  if (!*host)
    return false;

  return true;
}

bool AutofillManager::GetProfileOrCreditCard(
    int unique_id,
    const std::vector<AutofillProfile*>& profiles,
    const std::vector<CreditCard*>& credit_cards,
    const AutofillProfile** profile,
    const CreditCard** credit_card,
    size_t* variant) const {
  // Unpack the |unique_id| into component parts.
  GUIDPair credit_card_guid;
  GUIDPair profile_guid;
  UnpackGUIDs(unique_id, &credit_card_guid, &profile_guid);
  DCHECK(!guid::IsValidGUID(credit_card_guid.first) ||
         !guid::IsValidGUID(profile_guid.first));

  // Find the profile that matches the |profile_guid|, if one is specified.
  if (guid::IsValidGUID(profile_guid.first)) {
    for (std::vector<AutofillProfile*>::const_iterator iter = profiles.begin();
         iter != profiles.end(); ++iter) {
      if ((*iter)->guid() == profile_guid.first) {
        *profile = *iter;
        break;
      }
    }
    DCHECK(*profile);

    *variant = profile_guid.second;
  }

  // Find the credit card that matches the |credit_card_guid|, if specified.
  if (guid::IsValidGUID(credit_card_guid.first)) {
    for (std::vector<CreditCard*>::const_iterator iter = credit_cards.begin();
         iter != credit_cards.end(); ++iter) {
      if ((*iter)->guid() == credit_card_guid.first) {
        *credit_card = *iter;
        break;
      }
    }
    DCHECK(*credit_card);

    *variant = credit_card_guid.second;
  }

  return (*profile) || (*credit_card);
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
                                            const FormField& field,
                                            FormStructure** form_structure,
                                            AutofillField** autofill_field) {
  // Find the FormStructure that corresponds to |form|.
  // If we do not have this form in our cache but it is parseable, we'll add it
  // in the call to |UpdateCachedForm()|.
  if (!FindCachedForm(form, form_structure) &&
      !FormStructure(form).ShouldBeParsed(false)) {
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

  // We always update the cache, so we should be guaranteed to find the field.
  DCHECK(*autofill_field);
  return true;
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
  (*updated_form)->DetermineHeuristicTypes();

  // If we have cached data, propagate it to the updated form.
  if (cached_form) {
    std::map<string16, const AutofillField*> cached_fields;
    for (size_t i = 0; i < cached_form->field_count(); ++i) {
      const AutofillField* field = cached_form->field(i);
      cached_fields[field->unique_name()] = field;
    }

    for (size_t i = 0; i < (*updated_form)->field_count(); ++i) {
      AutofillField* field = (*updated_form)->field(i);
      std::map<string16, const AutofillField*>::iterator cached_field =
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
  SendAutofillTypePredictions(forms);

  return true;
}

void AutofillManager::GetProfileSuggestions(
    FormStructure* form,
    const FormField& field,
    AutofillFieldType type,
    std::vector<string16>* values,
    std::vector<string16>* labels,
    std::vector<string16>* icons,
    std::vector<int>* unique_ids) const {
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
        if (type == PHONE_HOME_NUMBER && !field_value_lower_case.empty() &&
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

void AutofillManager::GetCreditCardSuggestions(
    FormStructure* form,
    const FormField& field,
    AutofillFieldType type,
    std::vector<string16>* values,
    std::vector<string16>* labels,
    std::vector<string16>* icons,
    std::vector<int>* unique_ids) const {
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
                                              FormField* field) {
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
                                    FormField* field) {
  AutofillFieldType type = cached_field.type();
  DCHECK_NE(AutofillType::CREDIT_CARD, AutofillType(type).group());
  DCHECK(field);

  if (type == PHONE_HOME_NUMBER) {
    FillPhoneNumberField(profile, cached_field, variant, field);
  } else {
    if (field->form_control_type == ASCIIToUTF16("select-one")) {
      autofill::FillSelectControl(profile, type, field);
    } else {
      std::vector<string16> values;
      profile.GetCanonicalizedMultiInfo(type, &values);
      if (variant >= values.size()) {
        // If the variant is unavailable, bail.  This case is reachable, for
        // example if Sync updates a profile during the filling process.
        return;
      }

      field->value = values[variant];
    }
  }
}

void AutofillManager::FillPhoneNumberField(const AutofillProfile& profile,
                                           const AutofillField& cached_field,
                                           size_t variant,
                                           FormField* field) {
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
  if (!form_structures_.empty() && !disable_download_manager_requests_) {
    download_manager_.StartQueryRequest(form_structures_.get(),
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
  SendAutofillTypePredictions(non_queryable_forms);
}

int AutofillManager::GUIDToID(const GUIDPair& guid) const {
  if (!guid::IsValidGUID(guid.first))
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

const AutofillManager::GUIDPair AutofillManager::IDToGUID(int id) const {
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
