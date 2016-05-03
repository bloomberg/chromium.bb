// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/ntp_snippets_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/ntp_snippets/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/ntp_snippets_scheduler.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "components/safe_json/safe_json_parser.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/ntp/ntp_snippets_launcher.h"
#endif  // OS_ANDROID

using content::BrowserThread;
using suggestions::SuggestionsService;
using suggestions::SuggestionsServiceFactory;

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
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SuggestionsServiceFactory::GetInstance());
}

NTPSnippetsServiceFactory::~NTPSnippetsServiceFactory() {}

KeyedService* NTPSnippetsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  scoped_refptr<net::URLRequestContextGetter> request_context =
      content::BrowserContext::GetDefaultStoragePartition(context)->
            GetURLRequestContext();
  SuggestionsService* suggestions_service =
      SuggestionsServiceFactory::GetForProfile(profile);

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
      profile->GetPrefs(), suggestions_service, task_runner,
      g_browser_process->GetApplicationLocale(), scheduler,
      base::WrapUnique(new ntp_snippets::NTPSnippetsFetcher(
          request_context,
          chrome::GetChannel() == version_info::Channel::STABLE)),
      base::Bind(&safe_json::SafeJsonParser::Parse));
}
