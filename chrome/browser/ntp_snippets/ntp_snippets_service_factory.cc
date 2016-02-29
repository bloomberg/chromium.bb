// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/ntp_snippets_service_factory.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/ntp_snippets/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/ntp_snippets_scheduler.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/ntp_snippets_launcher.h"
#endif  // OS_ANDROID

using content::BrowserThread;

// static
NTPSnippetsServiceFactory* NTPSnippetsServiceFactory::GetInstance() {
  return base::Singleton<NTPSnippetsServiceFactory>::get();
}

// static
ntp_snippets::NTPSnippetsService* NTPSnippetsServiceFactory::GetForProfile(
    Profile* profile) {
  DCHECK(!profile->IsOffTheRecord());
  return static_cast<ntp_snippets::NTPSnippetsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

NTPSnippetsServiceFactory::NTPSnippetsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "NTPSnippetsService",
          BrowserContextDependencyManager::GetInstance()) {}

NTPSnippetsServiceFactory::~NTPSnippetsServiceFactory() {}

KeyedService* NTPSnippetsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  OAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  scoped_refptr<net::URLRequestContextGetter> request_context =
      context->GetRequestContext();

  ntp_snippets::NTPSnippetsScheduler* scheduler = nullptr;
#if defined(OS_ANDROID)
  scheduler = NTPSnippetsLauncher::Get();
#endif  // OS_ANDROID

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()
          ->GetSequencedTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::GetSequenceToken(),
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  return new ntp_snippets::NTPSnippetsService(
      profile->GetPrefs(), task_runner,
      g_browser_process->GetApplicationLocale(), scheduler,
      make_scoped_ptr(new ntp_snippets::NTPSnippetsFetcher(
          task_runner, signin_manager, token_service, request_context,
          profile->GetPath())));
}
