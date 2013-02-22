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
#include "base/guid.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/api/sync/profile_sync_service_base.h"
#include "chrome/browser/autofill/autocheckout/whitelist_manager.h"
#include "chrome/browser/autofill/autocheckout_manager.h"
#include "chrome/browser/autofill/autocomplete_history_manager.h"
#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/autofill_manager_delegate.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/password_generator.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/phone_number.h"
#include "chrome/browser/autofill/phone_number_i18n.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/form_data.h"
#include "chrome/common/form_data_predictions.h"
#include "chrome/common/form_field_data.h"
#include "chrome/common/password_form_fill_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutofillClient.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"

typedef PersonalDataManager::GUIDPair GUIDPair;
using base::TimeTicks;
using content::BrowserThread;
using content::RenderViewHost;
using WebKit::WebFormElement;

namespace {

const char* kAutofillManagerWebContentsUserDataKey = "web_contents_autofill";

// We only send a fraction of the forms to upload server.
// The rate for positive/negative matches potentially could be different.
const double kAutofillPositiveUploadRateDefaultValue = 0.20;
const double kAutofillNegativeUploadRateDefaultValue = 0.20;

const size_t kMaxRecentFormSignaturesToRemember = 3;

// Set a conservative upper bound on the number of forms we are willing to
// cache, simply to prevent unbounded memory consumption.
const size_t kMaxFormCacheSize = 100;

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
    const std::string& app_locale,
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

// static
void AutofillManager::CreateForWebContentsAndDelegate(
    content::WebContents* contents,
    autofill::AutofillManagerDelegate* delegate) {
  if (FromWebContents(contents))
    return;

  contents->SetUserData(kAutofillManagerWebContentsUserDataKey,
                        new AutofillManager(contents, delegate));
}

// static
AutofillManager* AutofillManager::FromWebContents(
    content::WebContents* contents) {
  return static_cast<AutofillManager*>(
      contents->GetUserData(kAutofillManagerWebContentsUserDataKey));
}

AutofillManager::AutofillManager(content::WebContents* web_contents,
                                 autofill::AutofillManagerDelegate* delegate)
    : content::WebContentsObserver(web_contents),
      manager_delegate_(delegate),
      personal_data_(delegate->GetPersonalDataManager()),
      download_manager_(web_contents->GetBrowserContext(), this),
      disable_download_manager_requests_(false),
      autocomplete_history_manager_(web_contents),
      autocheckout_manager_(this),
      metric_logger_(new AutofillMetrics),
      has_logged_autofill_enabled_(false),
      has_logged_address_suggestions_count_(false),
      did_show_suggestions_(false),
      user_did_type_(false),
      user_did_autofill_(false),
      user_did_edit_autofilled_field_(false),
      password_generation_enabled_(false),
      external_delegate_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  RegisterWithSyncService();
  registrar_.Init(manager_delegate_->GetPrefs());
  registrar_.Add(
      prefs::kPasswordGenerationEnabled,
      base::Bind(&AutofillManager::OnPasswordGenerationEnabledChanged,
                 base::Unretained(this)));
}

AutofillManager::~AutofillManager() {
}

// static
void AutofillManager::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kAutofillEnabled,
                                true,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kPasswordGenerationEnabled,
                                true,
                                PrefRegistrySyncable::SYNCABLE_PREF);
#if defined(OS_MACOSX)
  registry->RegisterBooleanPref(prefs::kAutofillAuxiliaryProfilesEnabled,
                                true,
                                PrefRegistrySyncable::SYNCABLE_PREF);
#else
  registry->RegisterBooleanPref(prefs::kAutofillAuxiliaryProfilesEnabled,
                                false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
  registry->RegisterDoublePref(prefs::kAutofillPositiveUploadRate,
                               kAutofillPositiveUploadRateDefaultValue,
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(prefs::kAutofillNegativeUploadRate,
                               kAutofillNegativeUploadRateDefaultValue,
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void AutofillManager::RegisterWithSyncService() {
  ProfileSyncServiceBase* service = manager_delegate_->GetProfileSyncService();
  if (service)
    service->AddObserver(this);
}

void AutofillManager::SendPasswordGenerationStateToRenderer(
    content::RenderViewHost* host, bool enabled) {
  host->Send(new AutofillMsg_PasswordGenerationEnabled(host->GetRoutingID(),
                                                       enabled));
}

// In order for password generation to be enabled, we need to make sure:
// (1) Password sync is enabled,
// (2) Password manager is enabled, and
// (3) Password generation preference check box is checked.
void AutofillManager::UpdatePasswordGenerationState(
    content::RenderViewHost* host,
    bool new_renderer) {
  ProfileSyncServiceBase* service = manager_delegate_->GetProfileSyncService();

  bool password_sync_enabled = false;
  if (service) {
    syncer::ModelTypeSet sync_set = service->GetPreferredDataTypes();
    password_sync_enabled =
      service->HasSyncSetupCompleted() && sync_set.Has(syncer::PASSWORDS);
  }

  bool saving_passwords_enabled = manager_delegate_->IsSavingPasswordsEnabled();
  bool preference_checked = manager_delegate_->GetPrefs()->GetBoolean(
      prefs::kPasswordGenerationEnabled);

  bool new_password_generation_enabled =
      password_sync_enabled &&
      saving_passwords_enabled &&
      preference_checked;

  if (new_password_generation_enabled != password_generation_enabled_ ||
      new_renderer) {
    password_generation_enabled_ = new_password_generation_enabled;
    SendPasswordGenerationStateToRenderer(host, password_generation_enabled_);
  }
}

void AutofillManager::RenderViewCreated(content::RenderViewHost* host) {
  UpdatePasswordGenerationState(host, true);
}

void AutofillManager::OnPasswordGenerationEnabledChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UpdatePasswordGenerationState(web_contents()->GetRenderViewHost(), false);
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

void AutofillManager::SetExternalDelegate(AutofillExternalDelegate* delegate) {
  // TODO(jrg): consider passing delegate into the ctor.  That won't
  // work if the delegate has a pointer to the AutofillManager, but
  // future directions may not need such a pointer.
  external_delegate_ = delegate;
  autocomplete_history_manager_.SetExternalDelegate(delegate);
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
    IPC_MESSAGE_HANDLER(AutofillHostMsg_SetDataList,
                        OnSetDataList)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_RequestAutocomplete,
                        OnRequestAutocomplete)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_ClickFailed,
                        OnClickFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void AutofillManager::WebContentsDestroyed(content::WebContents* web_contents) {
  ProfileSyncServiceBase* service = manager_delegate_->GetProfileSyncService();
  if (service && service->HasObserver(this))
    service->RemoveObserver(this);
}

bool AutofillManager::OnFormSubmitted(const FormData& form,
                                      const TimeTicks& timestamp) {
  // Let AutoComplete know as well.
  autocomplete_history_manager_.OnFormSubmitted(form);

  if (!IsAutofillEnabled())
    return false;

  if (web_contents()->GetBrowserContext()->IsOffTheRecord())
    return false;

  // Don't save data that was submitted through JavaScript.
  if (!form.user_submitted)
    return false;

  // Grab a copy of the form data.
  scoped_ptr<FormStructure> submitted_form(
      new FormStructure(form, GetAutocheckoutURLPrefix()));

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
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
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
                   AutofillCountry::ApplicationLocale(),
                   raw_submitted_form),
        base::Bind(&AutofillManager::UploadFormDataAsyncCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Owned(submitted_form.release()),
                   forms_loaded_timestamp_,
                   initial_interaction_timestamp_,
                   timestamp));
  }

  return true;
}

void AutofillManager::OnFormsSeen(const std::vector<FormData>& forms,
                                  const TimeTicks& timestamp) {
  autocheckout_manager_.OnFormsSeen();
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
                                           const FormFieldData& field,
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
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box,
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
  if (GetHost(&host) &&
      GetCachedFormAndField(form, field, &form_structure, &autofill_field) &&
      // Don't send suggestions for forms that aren't auto-fillable.
      form_structure->IsAutofillable(false)) {
    AutofillFieldType type = autofill_field->type();
    bool is_filling_credit_card =
        (AutofillType(type).group() == AutofillType::CREDIT_CARD);
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

    // If form is known to be at the start of the autofillable flow (i.e, when
    // Autofill server said so), then trigger payments UI while also returning
    // standard autofill suggestions to renderer process.
    if (autocheckout_manager_.IsStartOfAutofillableFlow()) {
      bool bubble_shown =
          autocheckout_manager_.MaybeShowAutocheckoutBubble(
              form.origin,
              form.ssl_status,
              web_contents()->GetView()->GetContentNativeView(),
              bounding_box);
      if (bubble_shown)
        return;
    }
  }

  // Add the results from AutoComplete.  They come back asynchronously, so we
  // hand off what we generated and they will send the results back to the
  // renderer.
  autocomplete_history_manager_.OnGetAutocompleteSuggestions(
      query_id, field.name, field.value, values, labels, icons, unique_ids);
}

void AutofillManager::OnFillAutofillFormData(int query_id,
                                             const FormData& form,
                                             const FormFieldData& field,
                                             int unique_id) {
  RenderViewHost* host = NULL;
  const FormGroup* form_group = NULL;
  size_t variant = 0;
  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  // NOTE: GetHost may invalidate |form_group| because it causes the
  // PersonalDataManager to reload Mac address book entries. Thus it must
  // come before GetProfileOrCreditCard.
  if (!GetHost(&host) ||
      !GetProfileOrCreditCard(unique_id, &form_group, &variant) ||
      !GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  DCHECK(host);
  DCHECK(form_structure);
  DCHECK(autofill_field);

  FormData result = form;

  // If the relevant section is auto-filled, we should fill |field| but not the
  // rest of the form.
  if (SectionIsAutofilled(*form_structure, form, autofill_field->section())) {
    for (std::vector<FormFieldData>::iterator iter = result.fields.begin();
         iter != result.fields.end(); ++iter) {
      if ((*iter) == field) {
        form_group->FillFormField(*autofill_field, variant, &(*iter));
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
    FieldTypeGroup field_group_type =
        AutofillType(cached_field->type()).group();
    if (field_group_type != AutofillType::NO_GROUP) {
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
      form_group->FillFormField(*cached_field,
                                use_variant,
                                &result.fields[i]);
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
  manager_delegate_->ShowAutofillSettings();
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

void AutofillManager::OnShowPasswordGenerationPopup(
    const gfx::Rect& bounds,
    int max_length,
    const content::PasswordForm& form) {
  password_generator_.reset(new autofill::PasswordGenerator(max_length));
  manager_delegate_->ShowPasswordGenerationBubble(
      bounds, form, password_generator_.get());
}

void AutofillManager::RemoveAutofillProfileOrCreditCard(int unique_id) {
  const FormGroup* form_group = NULL;
  size_t variant = 0;
  if (!GetProfileOrCreditCard(unique_id, &form_group, &variant)) {
    NOTREACHED();
    return;
  }

  // TODO(csharp): If we are dealing with a variant only the variant should
  // be deleted, instead of doing nothing.
  // http://crbug.com/124211
  if (variant != 0)
    return;

  personal_data_->RemoveByGUID(form_group->GetGUID());
}

void AutofillManager::RemoveAutocompleteEntry(const string16& name,
                                              const string16& value) {
  autocomplete_history_manager_.OnRemoveAutocompleteEntry(name, value);
}

content::WebContents* AutofillManager::GetWebContents() const {
  return web_contents();
}

const std::vector<FormStructure*>& AutofillManager::GetFormStructures() {
  return form_structures_.get();
}

void AutofillManager::ShowRequestAutocompleteDialog(
    const FormData& form,
    const GURL& source_url,
    const content::SSLStatus& ssl_status,
    autofill::DialogType dialog_type,
    const base::Callback<void(const FormStructure*)>& callback) {
  manager_delegate_->ShowRequestAutocompleteDialog(
      form, source_url, ssl_status, *metric_logger_, dialog_type, callback);
}

void AutofillManager::RequestAutocompleteDialogClosed() {
  manager_delegate_->RequestAutocompleteDialogClosed();
}

void AutofillManager::OnAddPasswordFormMapping(
      const FormFieldData& form,
      const PasswordFormFillData& fill_data) {
  if (external_delegate_)
    external_delegate_->AddPasswordFormMapping(form, fill_data);
}

void AutofillManager::OnShowPasswordSuggestions(
    const FormFieldData& field,
    const gfx::RectF& bounds,
    const std::vector<string16>& suggestions) {
  if (external_delegate_)
    external_delegate_->OnShowPasswordSuggestions(suggestions, field, bounds);
}

void AutofillManager::OnSetDataList(const std::vector<string16>& values,
                                    const std::vector<string16>& labels,
                                    const std::vector<string16>& icons,
                                    const std::vector<int>& unique_ids) {
  if (labels.size() != values.size() ||
      icons.size() != values.size() ||
      unique_ids.size() != values.size()) {
    return;
  }
  if (external_delegate_) {
    external_delegate_->SetCurrentDataListValues(values,
                                                 labels,
                                                 icons,
                                                 unique_ids);
  }
}

void AutofillManager::OnRequestAutocomplete(
    const FormData& form,
    const GURL& frame_url,
    const content::SSLStatus& ssl_status) {
  if (!IsAutofillEnabled()) {
    ReturnAutocompleteResult(WebFormElement::AutocompleteResultErrorDisabled,
                             FormData());
    return;
  }

  base::Callback<void(const FormStructure*)> callback =
      base::Bind(&AutofillManager::ReturnAutocompleteData,
                 weak_ptr_factory_.GetWeakPtr());
  ShowRequestAutocompleteDialog(
      form, frame_url, ssl_status,
      autofill::DIALOG_TYPE_REQUEST_AUTOCOMPLETE, callback);
}

void AutofillManager::ReturnAutocompleteResult(
    WebFormElement::AutocompleteResult result, const FormData& form_data) {
  // web_contents() will be NULL when the interactive autocomplete is closed due
  // to a tab or browser window closing.
  if (!web_contents())
    return;

  RenderViewHost* host = web_contents()->GetRenderViewHost();
  if (!host)
    return;

  host->Send(new AutofillMsg_RequestAutocompleteResult(host->GetRoutingID(),
                                                       result,
                                                       form_data));
}

void AutofillManager::ReturnAutocompleteData(const FormStructure* result) {
  RequestAutocompleteDialogClosed();
  if (!result) {
    ReturnAutocompleteResult(WebFormElement::AutocompleteResultErrorCancel,
                             FormData());
  } else {
    ReturnAutocompleteResult(WebFormElement::AutocompleteResultSuccess,
                             result->ToFormData());
  }
}

void AutofillManager::OnLoadedServerPredictions(
    const std::string& response_xml) {
  scoped_ptr<autofill::AutocheckoutPageMetaData> page_meta_data(
      new autofill::AutocheckoutPageMetaData());

  // Parse and store the server predictions.
  FormStructure::ParseQueryResponse(response_xml,
                                    form_structures_.get(),
                                    page_meta_data.get(),
                                    *metric_logger_);

  autocheckout_manager_.OnLoadedPageMetaData(page_meta_data.Pass());

  // If the corresponding flag is set, annotate forms with the predicted types.
  SendAutofillTypePredictions(form_structures_.get());
}

void AutofillManager::OnDidEndTextFieldEditing() {
  if (external_delegate_)
    external_delegate_->DidEndTextFieldEditing();
}

void AutofillManager::OnClickFailed(autofill::AutocheckoutStatus status) {
  // TODO(ahutter): Plug into WalletClient.
}

std::string AutofillManager::GetAutocheckoutURLPrefix() const {
  if (!web_contents())
    return std::string();

  autofill::autocheckout::WhitelistManager* whitelist_manager =
      autofill::autocheckout::WhitelistManager::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  return whitelist_manager->GetMatchedURLPrefix(web_contents()->GetURL());
}

bool AutofillManager::IsAutofillEnabled() const {
  return manager_delegate_->GetPrefs()->GetBoolean(prefs::kAutofillEnabled);
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
    AutofillCCInfoBarDelegate::Create(manager_delegate_->GetInfoBarService(),
        scoped_credit_card.release(), personal_data_, metric_logger_.get());
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
  form_structures_.clear();
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

AutofillManager::AutofillManager(content::WebContents* web_contents,
                                 autofill::AutofillManagerDelegate* delegate,
                                 PersonalDataManager* personal_data)
    : content::WebContentsObserver(web_contents),
      manager_delegate_(delegate),
      personal_data_(personal_data),
      download_manager_(web_contents->GetBrowserContext(), this),
      disable_download_manager_requests_(true),
      autocomplete_history_manager_(web_contents),
      autocheckout_manager_(this),
      metric_logger_(new AutofillMetrics),
      has_logged_autofill_enabled_(false),
      has_logged_address_suggestions_count_(false),
      did_show_suggestions_(false),
      user_did_type_(false),
      user_did_autofill_(false),
      user_did_edit_autofilled_field_(false),
      password_generation_enabled_(false),
      external_delegate_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(web_contents);
  DCHECK(manager_delegate_);
  RegisterWithSyncService();
  // Test code doesn't need registrar_.
}

void AutofillManager::set_metric_logger(const AutofillMetrics* metric_logger) {
  metric_logger_.reset(metric_logger);
}

bool AutofillManager::GetHost(RenderViewHost** host) const {
  if (!IsAutofillEnabled())
    return false;

  // No autofill data to return if the profiles are empty.
  if (personal_data_->GetProfiles().empty() &&
      personal_data_->credit_cards().empty()) {
    return false;
  }

  *host = web_contents()->GetRenderViewHost();
  if (!*host)
    return false;

  return true;
}

bool AutofillManager::GetProfileOrCreditCard(
    int unique_id,
    const FormGroup** form_group,
    size_t* variant) const {
  // Unpack the |unique_id| into component parts.
  GUIDPair credit_card_guid;
  GUIDPair profile_guid;
  UnpackGUIDs(unique_id, &credit_card_guid, &profile_guid);
  DCHECK(!base::IsValidGUID(credit_card_guid.first) ||
         !base::IsValidGUID(profile_guid.first));

  // Find the profile that matches the |profile_guid|, if one is specified.
  // Otherwise find the credit card that matches the |credit_card_guid|,
  // if specified.
  if (base::IsValidGUID(profile_guid.first)) {
    *form_group = personal_data_->GetProfileByGUID(profile_guid.first);
    *variant = profile_guid.second;
  } else if (base::IsValidGUID(credit_card_guid.first)) {
    *form_group =
        personal_data_->GetCreditCardByGUID(credit_card_guid.first);
    *variant = credit_card_guid.second;
  }

  return !!*form_group;
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
      !FormStructure(form, GetAutocheckoutURLPrefix()).ShouldBeParsed(false)) {
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
  form_structures_.push_back(
      new FormStructure(live_form, GetAutocheckoutURLPrefix()));
  *updated_form = *form_structures_.rbegin();
  (*updated_form)->DetermineHeuristicTypes(*metric_logger_);

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
    const FormFieldData& field,
    AutofillFieldType type,
    std::vector<string16>* values,
    std::vector<string16>* labels,
    std::vector<string16>* icons,
    std::vector<int>* unique_ids) const {
  std::vector<AutofillFieldType> field_types(form->field_count());
  for (size_t i = 0; i < form->field_count(); ++i) {
    field_types[i] = form->field(i)->type();
  }
  std::vector<GUIDPair> guid_pairs;

  personal_data_->GetProfileSuggestions(
      type, field.value, field.is_autofilled, field_types,
      values, labels, icons, &guid_pairs);

  for (size_t i = 0; i < guid_pairs.size(); ++i) {
    unique_ids->push_back(PackGUIDs(GUIDPair(std::string(), 0),
                                    guid_pairs[i]));
  }
}

void AutofillManager::GetCreditCardSuggestions(
    const FormFieldData& field,
    AutofillFieldType type,
    std::vector<string16>* values,
    std::vector<string16>* labels,
    std::vector<string16>* icons,
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
  std::string autocheckout_url_prefix = GetAutocheckoutURLPrefix();
  for (std::vector<FormData>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    scoped_ptr<FormStructure> form_structure(
        new FormStructure(*iter, autocheckout_url_prefix));
    if (!form_structure->ShouldBeParsed(false))
      continue;

    form_structure->DetermineHeuristicTypes(*metric_logger_);

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
