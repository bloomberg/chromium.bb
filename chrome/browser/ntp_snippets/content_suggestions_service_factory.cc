// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/image_fetcher/image_decoder.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/image_fetcher/image_fetcher_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ntp_snippets/bookmarks/bookmark_suggestions_provider.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/features.h"
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
#include "components/ntp_snippets/physical_web_pages/physical_web_page_suggestions_provider.h"
#include "components/offline_pages/offline_page_model.h"

using ntp_snippets::OfflinePageSuggestionsProvider;
using ntp_snippets::PhysicalWebPageSuggestionsProvider;
using offline_pages::OfflinePageModel;
using offline_pages::OfflinePageModelFactory;
#endif  // OS_ANDROID

using bookmarks::BookmarkModel;
using content::BrowserThread;
using history::HistoryService;
using image_fetcher::ImageFetcherImpl;
using ntp_snippets::BookmarkSuggestionsProvider;
using ntp_snippets::CategoryFactory;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::NTPSnippetsDatabase;
using ntp_snippets::NTPSnippetsFetcher;
using ntp_snippets::NTPSnippetsService;
using ntp_snippets::NTPSnippetsScheduler;
using ntp_snippets::NTPSnippetsStatusService;
using suggestions::ImageDecoderImpl;
using suggestions::SuggestionsService;
using suggestions::SuggestionsServiceFactory;

namespace {

// Clear the tasks that can be scheduled by running services.
void ClearScheduledTasks() {
#if defined(OS_ANDROID)
  NTPSnippetsLauncher::Get()->Unschedule();
#endif  // OS_ANDROID
}

#if defined(OS_ANDROID)
void RegisterOfflinePageProvider(OfflinePageModel* offline_page_model,
                                 ContentSuggestionsService* service,
                                 CategoryFactory* category_factory,
                                 PrefService* pref_service) {
  bool recent_tabs_enabled = base::FeatureList::IsEnabled(
      ntp_snippets::kRecentOfflineTabSuggestionsFeature);
  bool downloads_enabled =
      base::FeatureList::IsEnabled(ntp_snippets::kDownloadSuggestionsFeature);
  bool download_manager_ui_enabled =
      base::FeatureList::IsEnabled(chrome::android::kDownloadsUiFeature);
  std::unique_ptr<OfflinePageSuggestionsProvider>
      offline_page_suggestions_provider =
          base::MakeUnique<OfflinePageSuggestionsProvider>(
              recent_tabs_enabled, downloads_enabled,
              download_manager_ui_enabled, service, category_factory,
              offline_page_model, pref_service);
  service->RegisterProvider(std::move(offline_page_suggestions_provider));
}
#endif  // OS_ANDROID

void RegisterBookmarkProvider(BookmarkModel* bookmark_model,
                              ContentSuggestionsService* service,
                              CategoryFactory* category_factory,
                              PrefService* pref_service) {
  std::unique_ptr<BookmarkSuggestionsProvider> bookmark_suggestions_provider =
      base::MakeUnique<BookmarkSuggestionsProvider>(
          service, category_factory, bookmark_model, pref_service);
  service->RegisterProvider(std::move(bookmark_suggestions_provider));
}

#if defined(OS_ANDROID)
void RegisterPhysicalWebPageProvider(ContentSuggestionsService* service,
                                     CategoryFactory* category_factory) {
  std::unique_ptr<PhysicalWebPageSuggestionsProvider>
      physical_web_page_suggestions_provider =
          base::MakeUnique<PhysicalWebPageSuggestionsProvider>(
              service, category_factory);
  service->RegisterProvider(std::move(physical_web_page_suggestions_provider));
}
#endif  // OS_ANDROID

void RegisterArticleProvider(SigninManagerBase* signin_manager,
                             OAuth2TokenService* token_service,
                             SuggestionsService* suggestions_service,
                             ContentSuggestionsService* service,
                             CategoryFactory* category_factory,
                             PrefService* pref_service,
                             Profile* profile) {
  scoped_refptr<net::URLRequestContextGetter> request_context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLRequestContext();

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
  bool is_stable_channel =
      chrome::GetChannel() == version_info::Channel::STABLE;
  std::unique_ptr<NTPSnippetsService> ntp_snippets_service =
      base::MakeUnique<NTPSnippetsService>(
          service, service->category_factory(), pref_service,
          suggestions_service, g_browser_process->GetApplicationLocale(),
          scheduler,
          base::MakeUnique<NTPSnippetsFetcher>(
              signin_manager, token_service, request_context, pref_service,
              category_factory, base::Bind(&safe_json::SafeJsonParser::Parse),
              is_stable_channel),
          base::MakeUnique<ImageFetcherImpl>(
              base::MakeUnique<ImageDecoderImpl>(), request_context.get()),
          base::MakeUnique<ImageDecoderImpl>(),
          base::MakeUnique<NTPSnippetsDatabase>(database_dir, task_runner),
          base::MakeUnique<NTPSnippetsStatusService>(signin_manager,
                                                     pref_service));
  service->set_ntp_snippets_service(ntp_snippets_service.get());
  service->RegisterProvider(std::move(ntp_snippets_service));
}

}  // namespace

// static
ContentSuggestionsServiceFactory*
ContentSuggestionsServiceFactory::GetInstance() {
  return base::Singleton<ContentSuggestionsServiceFactory>::get();
}

// static
ContentSuggestionsService* ContentSuggestionsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ContentSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ContentSuggestionsServiceFactory::ContentSuggestionsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ContentSuggestionsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
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
  DCHECK(!profile->IsOffTheRecord());

  // Create the ContentSuggestionsService.
  State state =
      base::FeatureList::IsEnabled(ntp_snippets::kContentSuggestionsFeature)
          ? State::ENABLED
          : State::DISABLED;
  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  PrefService* pref_service = profile->GetPrefs();
  ContentSuggestionsService* service =
      new ContentSuggestionsService(state, history_service, pref_service);
  if (state == State::DISABLED) {
    // Since we won't initialise the services, they won't get a chance to
    // unschedule their tasks. We do it explicitly here instead.
    ClearScheduledTasks();
    return service;
  }

  CategoryFactory* category_factory = service->category_factory();
#if defined(OS_ANDROID)
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(profile);
#endif  // OS_ANDROID
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  OAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  SuggestionsService* suggestions_service =
      SuggestionsServiceFactory::GetForProfile(profile);

#if defined(OS_ANDROID)
  bool recent_tabs_enabled = base::FeatureList::IsEnabled(
      ntp_snippets::kRecentOfflineTabSuggestionsFeature);
  bool downloads_enabled =
      base::FeatureList::IsEnabled(ntp_snippets::kDownloadSuggestionsFeature);
  if (recent_tabs_enabled || downloads_enabled) {
    RegisterOfflinePageProvider(offline_page_model, service, category_factory,
                                pref_service);
  }
#endif  // OS_ANDROID

  // |bookmark_model| can be null in tests.
  if (base::FeatureList::IsEnabled(ntp_snippets::kBookmarkSuggestionsFeature) &&
      bookmark_model) {
    RegisterBookmarkProvider(bookmark_model, service, category_factory,
                             pref_service);
  }

#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(
          ntp_snippets::kPhysicalWebPageSuggestionsFeature)) {
    RegisterPhysicalWebPageProvider(service, category_factory);
  }
#endif  // OS_ANDROID

  if (base::FeatureList::IsEnabled(ntp_snippets::kArticleSuggestionsFeature)) {
    RegisterArticleProvider(signin_manager, token_service, suggestions_service,
                            service, category_factory, pref_service, profile);
  }

  return service;
}
