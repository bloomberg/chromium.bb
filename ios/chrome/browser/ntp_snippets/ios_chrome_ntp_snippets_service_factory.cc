// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ntp_snippets/ios_chrome_ntp_snippets_service_factory.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/ntp_snippets/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_thread.h"
#include "net/url_request/url_request_context_getter.h"

// static
IOSChromeNTPSnippetsServiceFactory*
IOSChromeNTPSnippetsServiceFactory::GetInstance() {
  return base::Singleton<IOSChromeNTPSnippetsServiceFactory>::get();
}

// static
ntp_snippets::NTPSnippetsService*
IOSChromeNTPSnippetsServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  DCHECK(!browser_state->IsOffTheRecord());
  return static_cast<ntp_snippets::NTPSnippetsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

IOSChromeNTPSnippetsServiceFactory::IOSChromeNTPSnippetsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "NTPSnippetsService",
          BrowserStateDependencyManager::GetInstance()) {}

IOSChromeNTPSnippetsServiceFactory::~IOSChromeNTPSnippetsServiceFactory() {}

scoped_ptr<KeyedService>
IOSChromeNTPSnippetsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state);
  DCHECK(!browser_state->IsOffTheRecord());
  SigninManager* signin_manager =
      ios::SigninManagerFactory::GetForBrowserState(chrome_browser_state);
  OAuth2TokenService* token_service =
      OAuth2TokenServiceFactory::GetForBrowserState(chrome_browser_state);
  scoped_refptr<net::URLRequestContextGetter> request_context =
      browser_state->GetRequestContext();

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      web::WebThread::GetBlockingPool()
          ->GetSequencedTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::GetSequenceToken(),
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  return make_scoped_ptr(new ntp_snippets::NTPSnippetsService(
      task_runner, GetApplicationContext()->GetApplicationLocale(),
      make_scoped_ptr(new ntp_snippets::NTPSnippetsFetcher(
          task_runner, (SigninManagerBase*)signin_manager, token_service,
          request_context, browser_state->GetStatePath()))));
}
