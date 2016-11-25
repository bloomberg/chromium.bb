// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"

#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/translate/language_model_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/browser_sync/profile_sync_service.h"
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
#include "components/ntp_snippets/remote/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/remote/ntp_snippets_scheduler.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/remote/remote_suggestions_status_service.h"
#include "components/ntp_snippets/sessions/foreign_sessions_suggestions_provider.h"
#include "components/ntp_snippets/sessions/tab_delegate_sync_adapter.h"
#include "components/prefs/pref_service.h"
#include "components/safe_json/safe_json_parser.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/translate/core/browser/language_model.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/ntp/ntp_snippets_launcher.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/ntp_snippets/download_suggestions_provider.h"
#include "components/ntp_snippets/offline_pages/recent_tab_suggestions_provider.h"
#include "components/ntp_snippets/physical_web_pages/physical_web_page_suggestions_provider.h"
#include "components/offline_pages/offline_page_model.h"

using content::DownloadManager;
using ntp_snippets::PhysicalWebPageSuggestionsProvider;
using ntp_snippets::RecentTabSuggestionsProvider;
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
using ntp_snippets::ForeignSessionsSuggestionsProvider;
using ntp_snippets::NTPSnippetsFetcher;
using ntp_snippets::NTPSnippetsScheduler;
using ntp_snippets::RemoteSuggestionsDatabase;
using ntp_snippets::RemoteSuggestionsProvider;
using ntp_snippets::RemoteSuggestionsStatusService;
using ntp_snippets::TabDelegateSyncAdapter;
using suggestions::ImageDecoderImpl;
using syncer::SyncService;
using translate::LanguageModel;

namespace {

// Clear the tasks that can be scheduled by running services.
void ClearScheduledTasks() {
#if defined(OS_ANDROID)
  NTPSnippetsLauncher::Get()->Unschedule();
#endif  // OS_ANDROID
}

#if defined(OS_ANDROID)
void RegisterRecentTabProvider(OfflinePageModel* offline_page_model,
                               ContentSuggestionsService* service,
                               CategoryFactory* category_factory,
                               PrefService* pref_service) {
  auto provider = base::MakeUnique<RecentTabSuggestionsProvider>(
      service, category_factory, offline_page_model, pref_service);
  service->RegisterProvider(std::move(provider));
}

void RegisterDownloadsProvider(OfflinePageModel* offline_page_model,
                               DownloadManager* download_manager,
                               ContentSuggestionsService* service,
                               CategoryFactory* category_factory,
                               PrefService* pref_service) {
  bool download_manager_ui_enabled =
      base::FeatureList::IsEnabled(chrome::android::kDownloadsUiFeature);
  auto provider = base::MakeUnique<DownloadSuggestionsProvider>(
      service, category_factory, offline_page_model, download_manager,
      pref_service, download_manager_ui_enabled);
  service->RegisterProvider(std::move(provider));
}
#endif  // OS_ANDROID

void RegisterBookmarkProvider(BookmarkModel* bookmark_model,
                              ContentSuggestionsService* service,
                              CategoryFactory* category_factory,
                              PrefService* pref_service) {
  auto provider = base::MakeUnique<BookmarkSuggestionsProvider>(
      service, category_factory, bookmark_model, pref_service);
  service->RegisterProvider(std::move(provider));
}

#if defined(OS_ANDROID)
void RegisterPhysicalWebPageProvider(ContentSuggestionsService* service,
                                     CategoryFactory* category_factory) {
  auto provider = base::MakeUnique<PhysicalWebPageSuggestionsProvider>(
      service, category_factory);
  service->RegisterProvider(std::move(provider));
}
#endif  // OS_ANDROID

void RegisterArticleProvider(SigninManagerBase* signin_manager,
                             OAuth2TokenService* token_service,
                             ContentSuggestionsService* service,
                             CategoryFactory* category_factory,
                             LanguageModel* language_model,
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
  auto provider = base::MakeUnique<RemoteSuggestionsProvider>(
      service, service->category_factory(), pref_service,
      g_browser_process->GetApplicationLocale(), service->user_classifier(),
      scheduler, base::MakeUnique<NTPSnippetsFetcher>(
                     signin_manager, token_service, request_context,
                     pref_service, category_factory, language_model,
                     base::Bind(&safe_json::SafeJsonParser::Parse),
                     is_stable_channel ? google_apis::GetAPIKey()
                                       : google_apis::GetNonStableAPIKey(),
                     service->user_classifier()),
      base::MakeUnique<ImageFetcherImpl>(base::MakeUnique<ImageDecoderImpl>(),
                                         request_context.get()),
      base::MakeUnique<ImageDecoderImpl>(),
      base::MakeUnique<RemoteSuggestionsDatabase>(database_dir, task_runner),
      base::MakeUnique<RemoteSuggestionsStatusService>(signin_manager,
                                                       pref_service));
  service->set_ntp_snippets_service(provider.get());
  service->RegisterProvider(std::move(provider));
}

void RegisterForeignSessionsProvider(SyncService* sync_service,
                                     ContentSuggestionsService* service,
                                     CategoryFactory* category_factory,
                                     PrefService* pref_service) {
  std::unique_ptr<TabDelegateSyncAdapter> sync_adapter =
      base::MakeUnique<TabDelegateSyncAdapter>(sync_service);
  auto provider = base::MakeUnique<ForeignSessionsSuggestionsProvider>(
      service, category_factory, std::move(sync_adapter), pref_service);
  service->RegisterProvider(std::move(provider));
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

// static
ContentSuggestionsService*
ContentSuggestionsServiceFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<ContentSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
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
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
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
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  PrefService* pref_service = profile->GetPrefs();
  ContentSuggestionsService* service = new ContentSuggestionsService(
      state, signin_manager, history_service, pref_service);
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
  DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(profile);
#endif  // OS_ANDROID
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  OAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  SyncService* sync_service =
      ProfileSyncServiceFactory::GetSyncServiceForBrowserContext(profile);
  LanguageModel* language_model =
      LanguageModelFactory::GetInstance()->GetForBrowserContext(profile);

#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(
          ntp_snippets::kRecentOfflineTabSuggestionsFeature)) {
    RegisterRecentTabProvider(offline_page_model, service, category_factory,
                              pref_service);
  }

  bool show_asset_downloads =
      base::FeatureList::IsEnabled(features::kAssetDownloadSuggestionsFeature);
  bool show_offline_page_downloads = base::FeatureList::IsEnabled(
      features::kOfflinePageDownloadSuggestionsFeature);
  if (show_asset_downloads || show_offline_page_downloads) {
    RegisterDownloadsProvider(
        show_offline_page_downloads ? offline_page_model : nullptr,
        show_asset_downloads ? download_manager : nullptr, service,
        category_factory, pref_service);
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
    RegisterArticleProvider(signin_manager, token_service, service,
                            category_factory, language_model, pref_service,
                            profile);
  }

  if (base::FeatureList::IsEnabled(
          ntp_snippets::kForeignSessionsSuggestionsFeature)) {
    RegisterForeignSessionsProvider(sync_service, service, category_factory,
                                    pref_service);
  }

  return service;
}
