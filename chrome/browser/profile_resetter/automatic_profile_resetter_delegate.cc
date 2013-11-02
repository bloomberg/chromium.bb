// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/automatic_profile_resetter_delegate.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

#if defined(OS_WIN)
#include "chrome/browser/enumerate_modules_model_win.h"
#include "chrome/browser/install_module_verifier_win.h"
#endif

namespace {

scoped_ptr<base::DictionaryValue> BuildSubTreeFromTemplateURL(
    const TemplateURL* template_url) {
  scoped_ptr<base::DictionaryValue> tree(new base::DictionaryValue);
  tree->SetString("search_url", template_url->url());
  tree->SetString("search_terms_replacement_key",
                  template_url->search_terms_replacement_key());
  tree->SetString("suggest_url", template_url->suggestions_url());
  tree->SetString("instant_url", template_url->instant_url());
  tree->SetString("image_url", template_url->image_url());
  tree->SetString("new_tab_url", template_url->new_tab_url());
  tree->SetString("search_url_post_params",
                  template_url->search_url_post_params());
  tree->SetString("suggest_url_post_params",
                  template_url->suggestions_url_post_params());
  tree->SetString("instant_url_post_params",
                  template_url->instant_url_post_params());
  tree->SetString("image_url_post_params",
                  template_url->image_url_post_params());
  tree->SetString("icon_url", template_url->favicon_url().spec());
  tree->SetString("name", template_url->short_name());
  tree->SetString("keyword", template_url->keyword());
  base::ListValue* input_encodings = new base::ListValue;
  input_encodings->AppendStrings(template_url->input_encodings());
  tree->Set("encodings", input_encodings);
  tree->SetString("id", base::Int64ToString(template_url->id()));
  tree->SetString("prepopulate_id",
                  base::IntToString(template_url->prepopulate_id()));
  base::ListValue* alternate_urls = new base::ListValue;
  alternate_urls->AppendStrings(template_url->alternate_urls());
  tree->Set("alternate_urls", alternate_urls);
  return tree.Pass();
}

}  // namespace


// AutomaticProfileResetterDelegateImpl --------------------------------------

AutomaticProfileResetterDelegateImpl::AutomaticProfileResetterDelegateImpl(
    TemplateURLService* template_url_service)
    : template_url_service_(template_url_service) {
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

scoped_ptr<base::ListValue> AutomaticProfileResetterDelegateImpl::
    GetLoadedModuleNameDigests() const {
  DCHECK(modules_have_been_enumerated_event_.is_signaled());
  std::set<std::string> module_name_digests;
#if defined(OS_WIN)
  if (module_list_)
    ExtractLoadedModuleNameDigests(*module_list_, &module_name_digests);
#endif
  scoped_ptr<base::ListValue> result(new base::ListValue);
  for (std::set<std::string>::const_iterator it = module_name_digests.begin();
       it != module_name_digests.end(); ++it)
    result->AppendString(*it);
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
  DCHECK(template_url_service_);
  size_t default_search_index = 0;
  ScopedVector<TemplateURL> engines(
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          template_url_service_->profile(), &default_search_index));
  scoped_ptr<base::ListValue> engines_details_list(new base::ListValue);
  for (ScopedVector<TemplateURL>::const_iterator it = engines.begin();
       it != engines.end(); ++it)
    engines_details_list->Append(BuildSubTreeFromTemplateURL(*it).release());
  return engines_details_list.Pass();
}

void AutomaticProfileResetterDelegateImpl::ShowPrompt() {
  // TODO(engedy): Call the UI from here once we have it.
}

void AutomaticProfileResetterDelegateImpl::OnTemplateURLServiceChanged() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(template_url_service_);
  if (template_url_service_->loaded() &&
      !template_url_service_ready_event_.is_signaled())
    template_url_service_ready_event_.Signal();
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
