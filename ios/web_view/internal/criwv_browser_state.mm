// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/criwv_browser_state.h"

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_service_factory.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "ios/web/public/web_thread.h"
#include "ios/web_view/internal/criwv_url_request_context_getter.h"
#include "ios/web_view/internal/pref_names.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {
const char kPreferencesFilename[] = FILE_PATH_LITERAL("Preferences");
}

namespace ios_web_view {

CRIWVBrowserState::CRIWVBrowserState() : web::BrowserState() {
  CHECK(PathService::Get(base::DIR_APP_DATA, &path_));

  request_context_getter_ = new CRIWVURLRequestContextGetter(
      GetStatePath(),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::CACHE));

  // Initialize prefs.
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry =
      new user_prefs::PrefRegistrySyncable;
  RegisterPrefs(pref_registry.get());
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
      JsonPrefStore::GetTaskRunnerForFile(path_,
                                          web::WebThread::GetBlockingPool());

  scoped_refptr<PersistentPrefStore> user_pref_store = new JsonPrefStore(
      path_.Append(kPreferencesFilename), sequenced_task_runner, nullptr);

  PrefServiceFactory factory;
  factory.set_user_prefs(user_pref_store);
  prefs_ = factory.Create(pref_registry.get());
}

CRIWVBrowserState::~CRIWVBrowserState() {}

PrefService* CRIWVBrowserState::GetPrefs() {
  DCHECK(prefs_);
  return prefs_.get();
}

// static
CRIWVBrowserState* CRIWVBrowserState::FromBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<CRIWVBrowserState*>(browser_state);
}

bool CRIWVBrowserState::IsOffTheRecord() const {
  return false;
}

base::FilePath CRIWVBrowserState::GetStatePath() const {
  return path_;
}

net::URLRequestContextGetter* CRIWVBrowserState::GetRequestContext() {
  return request_context_getter_.get();
}

void CRIWVBrowserState::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* pref_registry) {
  // TODO(crbug.com/679895): Find a good value for the kAcceptLanguages pref.
  // TODO(crbug.com/679895): Pass this value to the network stack somehow, for
  // the HTTP header.
  pref_registry->RegisterStringPref(prefs::kAcceptLanguages,
                                    l10n_util::GetLocaleOverride());
  pref_registry->RegisterBooleanPref(prefs::kEnableTranslate, true);
  translate::TranslatePrefs::RegisterProfilePrefs(pref_registry);
}

}  // namespace ios_web_view
