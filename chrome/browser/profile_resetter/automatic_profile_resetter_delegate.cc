// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/automatic_profile_resetter_delegate.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/profile_reset_global_error.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

#if defined(OS_WIN)
#include "chrome/browser/enumerate_modules_model_win.h"
#endif

namespace {

scoped_ptr<base::DictionaryValue> BuildSubTreeFromTemplateURL(
    const TemplateURL* template_url) {
  // If this value contains a placeholder in the pre-populated data, it will
  // have been replaced as it was loaded into a TemplateURL.
  // BuildSubTreeFromTemplateURL works with TemplateURL (not TemplateURLData)
  // in order to maintain this behaviour.
  // TODO(engedy): Confirm the expected behaviour and convert to use
  // TemplateURLData if possible."
  scoped_ptr<base::DictionaryValue> tree(new base::DictionaryValue);
  tree->SetString("name", template_url->short_name());
  tree->SetString("short_name", template_url->short_name());
  tree->SetString("keyword", template_url->keyword());
  tree->SetString("search_url", template_url->url());
  tree->SetString("url", template_url->url());
  tree->SetString("suggestions_url", template_url->suggestions_url());
  tree->SetString("instant_url", template_url->instant_url());
  tree->SetString("image_url", template_url->image_url());
  tree->SetString("new_tab_url", template_url->new_tab_url());
  tree->SetString("search_url_post_params",
                  template_url->search_url_post_params());
  tree->SetString("suggestions_url_post_params",
                  template_url->suggestions_url_post_params());
  tree->SetString("instant_url_post_params",
                  template_url->instant_url_post_params());
  tree->SetString("image_url_post_params",
                  template_url->image_url_post_params());
  base::ListValue* alternate_urls = new base::ListValue;
  alternate_urls->AppendStrings(template_url->alternate_urls());
  tree->Set("alternate_urls", alternate_urls);
  tree->SetString("favicon_url", template_url->favicon_url().spec());
  tree->SetString("originating_url", template_url->originating_url().spec());
  tree->SetBoolean("safe_for_autoreplace",
                   template_url->safe_for_autoreplace());
  base::ListValue* input_encodings = new base::ListValue;
  input_encodings->AppendStrings(template_url->input_encodings());
  tree->Set("input_encodings", input_encodings);
  tree->SetString("id", base::Int64ToString(template_url->id()));
  tree->SetString("date_created",
                  base::Int64ToString(
                      template_url->date_created().ToInternalValue()));
  tree->SetString("last_modified",
                  base::Int64ToString(
                      template_url->last_modified().ToInternalValue()));
  tree->SetBoolean("created_by_policy", template_url->created_by_policy());
  tree->SetInteger("usage_count", template_url->usage_count());
  tree->SetInteger("prepopulate_id", template_url->prepopulate_id());
  tree->SetString("search_terms_replacement_key",
                  template_url->search_terms_replacement_key());
  return tree.Pass();
}

#if defined(OS_WIN)
void ExtractLoadedModuleNameDigests(
    const base::ListValue& module_list,
    base::ListValue* module_name_digests) {
  DCHECK(module_name_digests);

  // EnumerateModulesModel produces a list of dictionaries.
  // Each dictionary corresponds to a module and exposes a number of properties.
  // We care only about 'type' and 'name'.
  for (size_t i = 0; i < module_list.GetSize(); ++i) {
    const base::DictionaryValue* module_dictionary = NULL;
    if (!module_list.GetDictionary(i, &module_dictionary))
      continue;
    ModuleEnumerator::ModuleType module_type =
        ModuleEnumerator::LOADED_MODULE;
    if (!module_dictionary->GetInteger(
            "type", reinterpret_cast<int*>(&module_type)) ||
        module_type != ModuleEnumerator::LOADED_MODULE) {
      continue;
    }
    std::string module_name;
    if (!module_dictionary->GetString("name", &module_name))
      continue;
    StringToLowerASCII(&module_name);
    module_name_digests->AppendString(base::MD5String(module_name));
  }
}
#endif

}  // namespace


// AutomaticProfileResetterDelegateImpl --------------------------------------

AutomaticProfileResetterDelegateImpl::AutomaticProfileResetterDelegateImpl(
    Profile* profile,
    ProfileResetter::ResettableFlags resettable_aspects)
    : profile_(profile),
      global_error_service_(GlobalErrorServiceFactory::GetForProfile(profile_)),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile_)),
      resettable_aspects_(resettable_aspects) {
  DCHECK(profile_);
  if (template_url_service_) {
    template_url_service_->AddObserver(this);
    // Needed so that |template_url_service_ready_event_| will be signaled even
    // when TemplateURLService had been already initialized before this point.
    OnTemplateURLServiceChanged();
  }

#if defined(OS_WIN)
  module_list_.reset(EnumerateModulesModel::GetInstance()->GetModuleList());
#endif
  if (module_list_) {
    // Having a non-empty module list proves that enumeration had been already
    // performed before this point.
    modules_have_been_enumerated_event_.Signal();
  }
  registrar_.Add(this,
                 chrome::NOTIFICATION_MODULE_LIST_ENUMERATED,
                 content::NotificationService::AllSources());
}

AutomaticProfileResetterDelegateImpl::~AutomaticProfileResetterDelegateImpl() {
  if (template_url_service_)
    template_url_service_->RemoveObserver(this);
}

void AutomaticProfileResetterDelegateImpl::EnumerateLoadedModulesIfNeeded() {
  if (!modules_have_been_enumerated_event_.is_signaled()) {
#if defined(OS_WIN)
    EnumerateModulesModel::GetInstance()->ScanNow();
#else
    modules_have_been_enumerated_event_.Signal();
#endif
  }
}

void AutomaticProfileResetterDelegateImpl::
    RequestCallbackWhenLoadedModulesAreEnumerated(
    const base::Closure& ready_callback) const {
  DCHECK(!ready_callback.is_null());
  modules_have_been_enumerated_event_.Post(FROM_HERE, ready_callback);
}

void AutomaticProfileResetterDelegateImpl::LoadTemplateURLServiceIfNeeded() {
  DCHECK(template_url_service_);
  template_url_service_->Load();  // Safe to call even if it has loaded already.
}

void AutomaticProfileResetterDelegateImpl::
    RequestCallbackWhenTemplateURLServiceIsLoaded(
    const base::Closure& ready_callback) const {
  DCHECK(!ready_callback.is_null());
  template_url_service_ready_event_.Post(FROM_HERE, ready_callback);
}

void AutomaticProfileResetterDelegateImpl::
    FetchBrandcodedDefaultSettingsIfNeeded() {
  if (brandcoded_config_fetcher_ ||
      brandcoded_defaults_fetched_event_.is_signaled())
    return;

  std::string brandcode;
  google_brand::GetBrand(&brandcode);
  if (brandcode.empty()) {
    brandcoded_defaults_.reset(new BrandcodedDefaultSettings);
    brandcoded_defaults_fetched_event_.Signal();
  } else {
    brandcoded_config_fetcher_.reset(new BrandcodeConfigFetcher(
        base::Bind(
            &AutomaticProfileResetterDelegateImpl::OnBrandcodedDefaultsFetched,
            base::Unretained(this)),
        GURL("https://tools.google.com/service/update2"),
        brandcode));
  }
}

void AutomaticProfileResetterDelegateImpl::
    RequestCallbackWhenBrandcodedDefaultsAreFetched(
    const base::Closure& ready_callback) const {
  DCHECK(!ready_callback.is_null());
  brandcoded_defaults_fetched_event_.Post(FROM_HERE, ready_callback);
}

scoped_ptr<base::ListValue> AutomaticProfileResetterDelegateImpl::
    GetLoadedModuleNameDigests() const {
  DCHECK(modules_have_been_enumerated_event_.is_signaled());
  scoped_ptr<base::ListValue> result(new base::ListValue);
#if defined(OS_WIN)
  if (module_list_)
    ExtractLoadedModuleNameDigests(*module_list_, result.get());
#endif
  return result.Pass();
}

scoped_ptr<base::DictionaryValue> AutomaticProfileResetterDelegateImpl::
    GetDefaultSearchProviderDetails() const {
  DCHECK(template_url_service_);
  DCHECK(template_url_service_->loaded());

  const TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();

  // Having a NULL default search provider is due to either:
  //  1.) default search providers being disabled by policy,
  //  2.) directly tampering with the Preferences and/or the SQLite DBs.
  // In this state, Omnibox non-keyword search functionality is disabled.
  return default_search_provider ?
      BuildSubTreeFromTemplateURL(default_search_provider) :
      scoped_ptr<base::DictionaryValue>(new base::DictionaryValue);
}

bool AutomaticProfileResetterDelegateImpl::
    IsDefaultSearchProviderManaged() const {
  DCHECK(template_url_service_);
  DCHECK(template_url_service_->loaded());
  return template_url_service_->is_default_search_managed();
}

scoped_ptr<base::ListValue> AutomaticProfileResetterDelegateImpl::
    GetPrepopulatedSearchProvidersDetails() const {
  size_t default_search_index = 0;
  ScopedVector<TemplateURLData> engines(
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          profile_->GetPrefs(), &default_search_index));
  scoped_ptr<base::ListValue> engines_details_list(new base::ListValue);
  for (ScopedVector<TemplateURLData>::const_iterator it = engines.begin();
       it != engines.end(); ++it) {
    TemplateURL template_url(**it);
    engines_details_list->Append(
        BuildSubTreeFromTemplateURL(&template_url).release());
  }
  return engines_details_list.Pass();
}

bool AutomaticProfileResetterDelegateImpl::TriggerPrompt() {
  DCHECK(global_error_service_);

  if (!ProfileResetGlobalError::IsSupportedOnPlatform())
    return false;

  ProfileResetGlobalError* global_error = new ProfileResetGlobalError(profile_);
  global_error_service_->AddGlobalError(global_error);

  // Do not try to show bubble if another GlobalError is already showing one.
  const GlobalErrorService::GlobalErrorList& global_errors(
      global_error_service_->errors());
  GlobalErrorService::GlobalErrorList::const_iterator it;
  for (it = global_errors.begin(); it != global_errors.end(); ++it) {
    if ((*it)->GetBubbleView())
      break;
  }
  if (it == global_errors.end()) {
    Browser* browser = chrome::FindTabbedBrowser(
        profile_,
        false /*match_original_profiles*/,
        chrome::GetActiveDesktop());
    if (browser)
      global_error->ShowBubbleView(browser);
  }
  return true;
}

void AutomaticProfileResetterDelegateImpl::TriggerProfileSettingsReset(
    bool send_feedback,
    const base::Closure& completion) {
  DCHECK(!profile_resetter_);
  DCHECK(!completion.is_null());

  profile_resetter_.reset(new ProfileResetter(profile_));
  FetchBrandcodedDefaultSettingsIfNeeded();
  RequestCallbackWhenBrandcodedDefaultsAreFetched(base::Bind(
      &AutomaticProfileResetterDelegateImpl::RunProfileSettingsReset,
      AsWeakPtr(),
      send_feedback,
      completion));
}

void AutomaticProfileResetterDelegateImpl::OnTemplateURLServiceChanged() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(template_url_service_);
  if (template_url_service_->loaded() &&
      !template_url_service_ready_event_.is_signaled())
    template_url_service_ready_event_.Signal();
}

void AutomaticProfileResetterDelegateImpl::DismissPrompt() {
  DCHECK(global_error_service_);
  GlobalError* global_error =
      global_error_service_->GetGlobalErrorByMenuItemCommandID(
          IDC_SHOW_SETTINGS_RESET_BUBBLE);
  if (global_error) {
    // This will also close/destroy the Bubble UI if it is currently shown.
    global_error_service_->RemoveGlobalError(global_error);
    delete global_error;
  }
}

void AutomaticProfileResetterDelegateImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (type == chrome::NOTIFICATION_MODULE_LIST_ENUMERATED &&
      !modules_have_been_enumerated_event_.is_signaled()) {
#if defined(OS_WIN)
    module_list_.reset(EnumerateModulesModel::GetInstance()->GetModuleList());
#endif
    modules_have_been_enumerated_event_.Signal();
  }
}

void AutomaticProfileResetterDelegateImpl::SendFeedback(
    const std::string& report) const {
  SendSettingsFeedback(report, profile_, PROFILE_RESET_PROMPT);
}

void AutomaticProfileResetterDelegateImpl::RunProfileSettingsReset(
    bool send_feedback,
    const base::Closure& completion) {
  DCHECK(brandcoded_defaults_);
  scoped_ptr<ResettableSettingsSnapshot> old_settings_snapshot;
  if (send_feedback) {
    old_settings_snapshot.reset(new ResettableSettingsSnapshot(profile_));
    old_settings_snapshot->RequestShortcuts(base::Closure());
  }
  profile_resetter_->Reset(resettable_aspects_,
                           brandcoded_defaults_.Pass(),
                           send_feedback,
                           base::Bind(&AutomaticProfileResetterDelegateImpl::
                                          OnProfileSettingsResetCompleted,
                                      AsWeakPtr(),
                                      completion,
                                      base::Passed(&old_settings_snapshot)));
}

void AutomaticProfileResetterDelegateImpl::
    OnBrandcodedDefaultsFetched() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(brandcoded_config_fetcher_);
  DCHECK(!brandcoded_config_fetcher_->IsActive());
  brandcoded_defaults_ = brandcoded_config_fetcher_->GetSettings();
  if (!brandcoded_defaults_)
    brandcoded_defaults_.reset(new BrandcodedDefaultSettings);
  brandcoded_defaults_fetched_event_.Signal();
}

void AutomaticProfileResetterDelegateImpl::OnProfileSettingsResetCompleted(
    const base::Closure& user_callback,
    scoped_ptr<ResettableSettingsSnapshot> old_settings_snapshot) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (old_settings_snapshot) {
    ResettableSettingsSnapshot new_settings_snapshot(profile_);
    int difference =
        old_settings_snapshot->FindDifferentFields(new_settings_snapshot);
    if (difference) {
      old_settings_snapshot->Subtract(new_settings_snapshot);
      std::string report =
          SerializeSettingsReport(*old_settings_snapshot, difference);
      SendFeedback(report);
    }
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE, user_callback);
}
