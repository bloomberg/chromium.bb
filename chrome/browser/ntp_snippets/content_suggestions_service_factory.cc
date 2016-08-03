// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"

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
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/image_fetcher/image_decoder.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/image_fetcher/image_fetcher_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/ntp_snippets/content_suggestions_service.h"
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
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "components/ntp_snippets/offline_pages/offline_page_suggestions_provider.h"
#include "components/offline_pages/offline_page_model.h"

using ntp_snippets::OfflinePageSuggestionsProvider;
using offline_pages::OfflinePageModel;
using offline_pages::OfflinePageModelFactory;
#endif  // OS_ANDROID

using content::BrowserThread;
using image_fetcher::ImageFetcherImpl;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::NTPSnippetsDatabase;
using ntp_snippets::NTPSnippetsFetcher;
using ntp_snippets::NTPSnippetsService;
using ntp_snippets::NTPSnippetsScheduler;
using ntp_snippets::NTPSnippetsStatusService;
using suggestions::ImageDecoderImpl;
using suggestions::SuggestionsService;
using suggestions::SuggestionsServiceFactory;

// static
ContentSuggestionsServiceFactory*
ContentSuggestionsServiceFactory::GetInstance() {
  return base::Singleton<ContentSuggestionsServiceFactory>::get();
}

// static
ContentSuggestionsService* ContentSuggestionsServiceFactory::GetForProfile(
    Profile* profile) {
  DCHECK(!profile->IsOffTheRecord());
  return static_cast<ContentSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ContentSuggestionsServiceFactory::ContentSuggestionsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ContentSuggestionsService",
          BrowserContextDependencyManager::GetInstance()) {
#if defined(OS_ANDROID)
  DependsOn(OfflinePageModelFactory::GetInstance());
#endif  // OS_ANDROID
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(SuggestionsServiceFactory::GetInstance());
}

ContentSuggestionsServiceFactory::~ContentSuggestionsServiceFactory() {}

KeyedService* ContentSuggestionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  using State = ContentSuggestionsService::State;
  Profile* profile = Profile::FromBrowserContext(context);

  // Create the ContentSuggestionsService.
  State enabled = State::DISABLED;
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(chrome::android::kNTPSnippetsFeature))
    enabled = State::ENABLED;
#endif  // OS_ANDROID
  ContentSuggestionsService* service = new ContentSuggestionsService(enabled);
  if (enabled == State::DISABLED)
    return service;

// Create the OfflinePageSuggestionsProvider.
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(
          chrome::android::kNTPOfflinePageSuggestionsFeature)) {
    OfflinePageModel* offline_page_model =
        OfflinePageModelFactory::GetForBrowserContext(profile);

    std::unique_ptr<OfflinePageSuggestionsProvider>
        offline_page_suggestions_provider =
            base::MakeUnique<OfflinePageSuggestionsProvider>(
                service, service->category_factory(), offline_page_model);
    service->RegisterProvider(std::move(offline_page_suggestions_provider));
  }
#endif  // OS_ANDROID

  // Create the NTPSnippetsService (articles provider).
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  OAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  scoped_refptr<net::URLRequestContextGetter> request_context =
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetURLRequestContext();
  SuggestionsService* suggestions_service =
      SuggestionsServiceFactory::GetForProfile(profile);
  NTPSnippetsScheduler* scheduler = nullptr;
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
  std::unique_ptr<NTPSnippetsService> ntp_snippets_service =
      base::MakeUnique<NTPSnippetsService>(
          service, service->category_factory(), profile->GetPrefs(),
          suggestions_service, g_browser_process->GetApplicationLocale(),
          scheduler, base::MakeUnique<NTPSnippetsFetcher>(
                         signin_manager, token_service, request_context,
                         profile->GetPrefs(),
                         base::Bind(&safe_json::SafeJsonParser::Parse),
                         chrome::GetChannel() == version_info::Channel::STABLE),
          base::MakeUnique<ImageFetcherImpl>(
              base::MakeUnique<ImageDecoderImpl>(), request_context.get()),
          base::MakeUnique<ImageDecoderImpl>(),
          base::MakeUnique<NTPSnippetsDatabase>(database_dir, task_runner),
          base::MakeUnique<NTPSnippetsStatusService>(signin_manager,
                                                     profile->GetPrefs()));
  service->set_ntp_snippets_service(ntp_snippets_service.get());
  service->RegisterProvider(std::move(ntp_snippets_service));

  return service;
}
