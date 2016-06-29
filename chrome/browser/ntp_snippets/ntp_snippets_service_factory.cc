// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/ntp_snippets_service_factory.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/image_fetcher/image_decoder.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/image_fetcher/image_fetcher_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/ntp_snippets_database.h"
#include "components/ntp_snippets/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/ntp_snippets_scheduler.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "components/ntp_snippets/ntp_snippets_status_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_json/safe_json_parser.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/ntp/ntp_snippets_launcher.h"
#endif  // OS_ANDROID

using content::BrowserThread;
using image_fetcher::ImageFetcherImpl;
using suggestions::ImageDecoderImpl;
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
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(SuggestionsServiceFactory::GetInstance());
}

NTPSnippetsServiceFactory::~NTPSnippetsServiceFactory() {}

KeyedService* NTPSnippetsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // TODO(mvanouwerkerk): Move the enable logic into the service once we start
  // observing pref changes.
  bool enabled = profile->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled);
#if defined(OS_ANDROID)
  enabled = enabled &&
            base::FeatureList::IsEnabled(chrome::android::kNTPSnippetsFeature);
#endif  // OS_ANDROID

  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  OAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  scoped_refptr<net::URLRequestContextGetter> request_context =
      content::BrowserContext::GetDefaultStoragePartition(context)->
            GetURLRequestContext();
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  SuggestionsService* suggestions_service =
      SuggestionsServiceFactory::GetForProfile(profile);

  ntp_snippets::NTPSnippetsScheduler* scheduler = nullptr;
#if defined(OS_ANDROID)
  scheduler = NTPSnippetsLauncher::Get();
#endif  // OS_ANDROID

  base::FilePath database_dir(
      profile->GetPath().Append(ntp_snippets::kDatabaseFolder));
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()
          ->GetSequencedTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::GetSequenceToken(),
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  return new ntp_snippets::NTPSnippetsService(
      enabled, profile->GetPrefs(), suggestions_service,
      g_browser_process->GetApplicationLocale(), scheduler,
      base::MakeUnique<ntp_snippets::NTPSnippetsFetcher>(
          signin_manager, token_service, request_context,
          base::Bind(&safe_json::SafeJsonParser::Parse),
          chrome::GetChannel() == version_info::Channel::STABLE),
      base::MakeUnique<ImageFetcherImpl>(
          base::MakeUnique<ImageDecoderImpl>(), request_context.get()),
      base::MakeUnique<ImageDecoderImpl>(),
      base::MakeUnique<ntp_snippets::NTPSnippetsDatabase>(database_dir,
                                                          task_runner),
      base::MakeUnique<ntp_snippets::NTPSnippetsStatusService>(signin_manager,
                                                               sync_service));
}
