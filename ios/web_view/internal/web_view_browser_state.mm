// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/web_view_browser_state.h"

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_service_factory.h"
#include "components/signin/ios/browser/active_state_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "ios/web/public/web_thread.h"
#include "ios/web_view/internal/content_settings/web_view_cookie_settings_factory.h"
#include "ios/web_view/internal/content_settings/web_view_host_content_settings_map_factory.h"
#include "ios/web_view/internal/language/web_view_language_model_factory.h"
#include "ios/web_view/internal/language/web_view_url_language_histogram_factory.h"
#include "ios/web_view/internal/pref_names.h"
#include "ios/web_view/internal/signin/web_view_account_fetcher_service_factory.h"
#include "ios/web_view/internal/signin/web_view_account_tracker_service_factory.h"
#include "ios/web_view/internal/signin/web_view_gaia_cookie_manager_service_factory.h"
#include "ios/web_view/internal/signin/web_view_oauth2_token_service_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_client_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_error_controller_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_manager_factory.h"
#include "ios/web_view/internal/translate/web_view_translate_ranker_factory.h"
#include "ios/web_view/internal/web_view_url_request_context_getter.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kPreferencesFilename[] = FILE_PATH_LITERAL("Preferences");
}

namespace ios_web_view {

WebViewBrowserState::WebViewBrowserState(bool off_the_record)
    : web::BrowserState(), off_the_record_(off_the_record) {
  // IO access is required to setup the browser state. In Chrome, this is
  // already allowed during thread startup. However, startup time of
  // ChromeWebView is not predetermined, so IO access is temporarily allowed.
  bool wasIOAllowed = base::ThreadRestrictions::SetIOAllowed(true);

  CHECK(PathService::Get(base::DIR_APP_DATA, &path_));

  request_context_getter_ = new WebViewURLRequestContextGetter(
      GetStatePath(),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO));

  BrowserState::Initialize(this, path_);

  // Initialize prefs.
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry =
      new user_prefs::PrefRegistrySyncable;
  RegisterPrefs(pref_registry.get());

  scoped_refptr<PersistentPrefStore> user_pref_store;
  if (off_the_record) {
    user_pref_store = new InMemoryPrefStore();
  } else {
    user_pref_store = new JsonPrefStore(path_.Append(kPreferencesFilename));
  }

  PrefServiceFactory factory;
  factory.set_user_prefs(user_pref_store);
  prefs_ = factory.Create(pref_registry.get());

  base::ThreadRestrictions::SetIOAllowed(wasIOAllowed);

  ActiveStateManager::FromBrowserState(this)->SetActive(true);

  BrowserStateDependencyManager::GetInstance()->CreateBrowserStateServices(
      this);
}

WebViewBrowserState::~WebViewBrowserState() {
  BrowserStateDependencyManager::GetInstance()->DestroyBrowserStateServices(
      this);

  ActiveStateManager::FromBrowserState(this)->SetActive(false);
}

PrefService* WebViewBrowserState::GetPrefs() {
  DCHECK(prefs_);
  return prefs_.get();
}

// static
WebViewBrowserState* WebViewBrowserState::FromBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<WebViewBrowserState*>(browser_state);
}

bool WebViewBrowserState::IsOffTheRecord() const {
  return off_the_record_;
}

base::FilePath WebViewBrowserState::GetStatePath() const {
  return path_;
}

net::URLRequestContextGetter* WebViewBrowserState::GetRequestContext() {
  return request_context_getter_.get();
}

void WebViewBrowserState::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* pref_registry) {
  // TODO(crbug.com/679895): Find a good value for the kAcceptLanguages pref.
  // TODO(crbug.com/679895): Pass this value to the network stack somehow, for
  // the HTTP header.
  pref_registry->RegisterStringPref(prefs::kAcceptLanguages,
                                    l10n_util::GetLocaleOverride());
  pref_registry->RegisterBooleanPref(prefs::kOfferTranslateEnabled, true);
  translate::TranslatePrefs::RegisterProfilePrefs(pref_registry);

  // Instantiate all factories to setup dependency graph for pref registration.
  WebViewCookieSettingsFactory::GetInstance();
  WebViewHostContentSettingsMapFactory::GetInstance();
  WebViewAccountFetcherServiceFactory::GetInstance();
  WebViewAccountTrackerServiceFactory::GetInstance();
  WebViewGaiaCookieManagerServiceFactory::GetInstance();
  WebViewLanguageModelFactory::GetInstance();
  WebViewOAuth2TokenServiceFactory::GetInstance();
  WebViewSigninClientFactory::GetInstance();
  WebViewSigninErrorControllerFactory::GetInstance();
  WebViewSigninManagerFactory::GetInstance();
  WebViewTranslateRankerFactory::GetInstance();
  WebViewUrlLanguageHistogramFactory::GetInstance();

  BrowserStateDependencyManager::GetInstance()
      ->RegisterBrowserStatePrefsForServices(this, pref_registry);

  autofill::AutofillManager::RegisterProfilePrefs(pref_registry);
}

}  // namespace ios_web_view
