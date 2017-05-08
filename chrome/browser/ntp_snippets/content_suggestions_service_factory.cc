// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"

#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/default_clock.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/translate/language_model_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ntp_snippets/bookmarks/bookmark_suggestions_provider.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/remote/remote_suggestions_fetcher.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_status_service.h"
#include "components/ntp_snippets/sessions/foreign_sessions_suggestions_provider.h"
#include "components/ntp_snippets/sessions/tab_delegate_sync_adapter.h"
#include "components/ntp_snippets/user_classifier.h"
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
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/ntp_snippets/download_suggestions_provider.h"
#include "components/ntp_snippets/offline_pages/recent_tab_suggestions_provider.h"
#include "components/ntp_snippets/physical_web_pages/physical_web_page_suggestions_provider.h"
#include "components/offline_pages/content/suggested_articles_observer.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/recent_tabs/recent_tabs_ui_adapter_delegate.h"
#include "components/physical_web/data_source/physical_web_data_source.h"

using content::DownloadManager;
using ntp_snippets::PhysicalWebPageSuggestionsProvider;
using ntp_snippets::RecentTabSuggestionsProvider;
using offline_pages::OfflinePageModel;
using offline_pages::RequestCoordinator;
using offline_pages::OfflinePageModelFactory;
using offline_pages::RequestCoordinatorFactory;
using physical_web::PhysicalWebDataSource;
#endif  // OS_ANDROID

using bookmarks::BookmarkModel;
using content::BrowserThread;
using history::HistoryService;
using image_fetcher::ImageFetcherImpl;
using ntp_snippets::BookmarkSuggestionsProvider;
using ntp_snippets::CategoryRanker;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::ForeignSessionsSuggestionsProvider;
using ntp_snippets::GetFetchEndpoint;
using ntp_snippets::PersistentScheduler;
using ntp_snippets::RemoteSuggestionsDatabase;
using ntp_snippets::RemoteSuggestionsFetcher;
using ntp_snippets::RemoteSuggestionsProviderImpl;
using ntp_snippets::RemoteSuggestionsSchedulerImpl;
using ntp_snippets::RemoteSuggestionsStatusService;
using ntp_snippets::TabDelegateSyncAdapter;
using ntp_snippets::UserClassifier;
using suggestions::ImageDecoderImpl;
using syncer::SyncService;
using translate::LanguageModel;

// For now, ContentSuggestionsService must only be instantiated on Android.
// See also crbug.com/688366.
#if defined(OS_ANDROID)
#define CONTENT_SUGGESTIONS_ENABLED 1
#else
#define CONTENT_SUGGESTIONS_ENABLED 0
#endif  // OS_ANDROID

// The actual #if that does the work is below in BuildServiceInstanceFor. This
// one is just required to avoid "unused code" compiler errors.
#if CONTENT_SUGGESTIONS_ENABLED

namespace {

bool IsChromeHomeEnabled() {
#if defined(OS_ANDROID)
  return base::FeatureList::IsEnabled(chrome::android::kChromeHomeFeature);
#else
  return false;
#endif
}

#if defined(OS_ANDROID)

bool IsRecentTabProviderEnabled() {
  return base::FeatureList::IsEnabled(
             ntp_snippets::kRecentOfflineTabSuggestionsFeature) &&
         base::FeatureList::IsEnabled(
             offline_pages::kOffliningRecentPagesFeature);
}

void RegisterRecentTabProvider(OfflinePageModel* offline_page_model,
                               RequestCoordinator* request_coordinator,
                               ContentSuggestionsService* service,
                               PrefService* pref_service) {
  offline_pages::DownloadUIAdapter* ui_adapter = offline_pages::
      RecentTabsUIAdapterDelegate::GetOrCreateRecentTabsUIAdapter(
          offline_page_model, request_coordinator);
  auto provider = base::MakeUnique<RecentTabSuggestionsProvider>(
      service, ui_adapter, pref_service);
  service->RegisterProvider(std::move(provider));
}

void RegisterDownloadsProvider(OfflinePageModel* offline_page_model,
                               DownloadManager* download_manager,
                               DownloadHistory* download_history,
                               ContentSuggestionsService* service,
                               PrefService* pref_service) {
  auto provider = base::MakeUnique<DownloadSuggestionsProvider>(
      service, offline_page_model, download_manager, download_history,
      pref_service, base::MakeUnique<base::DefaultClock>());
  service->RegisterProvider(std::move(provider));
}

#endif  // OS_ANDROID

void RegisterBookmarkProvider(BookmarkModel* bookmark_model,
                              ContentSuggestionsService* service,
                              PrefService* pref_service) {
  auto provider = base::MakeUnique<BookmarkSuggestionsProvider>(
      service, bookmark_model, pref_service);
  service->RegisterProvider(std::move(provider));
}

#if defined(OS_ANDROID)

bool IsPhysicalWebPageProviderEnabled() {
  return base::FeatureList::IsEnabled(
             ntp_snippets::kPhysicalWebPageSuggestionsFeature) &&
         base::FeatureList::IsEnabled(chrome::android::kPhysicalWebFeature);
}

void RegisterPhysicalWebPageProvider(
    ContentSuggestionsService* service,
    PhysicalWebDataSource* physical_web_data_source,
    PrefService* pref_service) {
  auto provider = base::MakeUnique<PhysicalWebPageSuggestionsProvider>(
      service, physical_web_data_source, pref_service);
  service->RegisterProvider(std::move(provider));
}

#endif  // OS_ANDROID

void RegisterArticleProvider(SigninManagerBase* signin_manager,
                             OAuth2TokenService* token_service,
                             ContentSuggestionsService* service,
                             LanguageModel* language_model,
                             UserClassifier* user_classifier,
                             PrefService* pref_service,
                             Profile* profile) {
  scoped_refptr<net::URLRequestContextGetter> request_context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLRequestContext();

  base::FilePath database_dir(
      profile->GetPath().Append(ntp_snippets::kDatabaseFolder));
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  std::string api_key;
  // The API is private. If we don't have the official API key, don't even try.
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    bool is_stable_channel =
        chrome::GetChannel() == version_info::Channel::STABLE;
    api_key = is_stable_channel ? google_apis::GetAPIKey()
                                : google_apis::GetNonStableAPIKey();
  }

  // Pass the pref name associated to the search suggest toggle, and use it to
  // also guard the remote suggestions feature.
  // TODO(https://crbug.com/710636): Cleanup this clunky dependency once the
  // preference design is stable.
  std::string additional_toggle_pref;
  if (base::FeatureList::IsEnabled(
          chrome::android::kContentSuggestionsSettings)) {
    additional_toggle_pref = prefs::kSearchSuggestEnabled;
  }
  auto suggestions_fetcher = base::MakeUnique<RemoteSuggestionsFetcher>(
      signin_manager, token_service, request_context, pref_service,
      language_model, base::Bind(&safe_json::SafeJsonParser::Parse),
      GetFetchEndpoint(chrome::GetChannel()), api_key, user_classifier);
  auto provider = base::MakeUnique<RemoteSuggestionsProviderImpl>(
      service, pref_service, g_browser_process->GetApplicationLocale(),
      service->category_ranker(), service->remote_suggestions_scheduler(),
      std::move(suggestions_fetcher),
      base::MakeUnique<ImageFetcherImpl>(base::MakeUnique<ImageDecoderImpl>(),
                                         request_context.get()),
      base::MakeUnique<RemoteSuggestionsDatabase>(database_dir, task_runner),
      base::MakeUnique<RemoteSuggestionsStatusService>(
          signin_manager, pref_service, additional_toggle_pref));

  service->remote_suggestions_scheduler()->SetProvider(provider.get());
  service->set_remote_suggestions_provider(provider.get());
  service->RegisterProvider(std::move(provider));
}

void RegisterForeignSessionsProvider(SyncService* sync_service,
                                     ContentSuggestionsService* service,
                                     PrefService* pref_service) {
  std::unique_ptr<TabDelegateSyncAdapter> sync_adapter =
      base::MakeUnique<TabDelegateSyncAdapter>(sync_service);
  auto provider = base::MakeUnique<ForeignSessionsSuggestionsProvider>(
      service, std::move(sync_adapter), pref_service);
  service->RegisterProvider(std::move(provider));
}

}  // namespace

#endif  // CONTENT_SUGGESTIONS_ENABLED

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
  DependsOn(LargeIconServiceFactory::GetInstance());
#if defined(OS_ANDROID)
  DependsOn(OfflinePageModelFactory::GetInstance());
#endif  // OS_ANDROID
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
}

ContentSuggestionsServiceFactory::~ContentSuggestionsServiceFactory() = default;

KeyedService* ContentSuggestionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if CONTENT_SUGGESTIONS_ENABLED

  using State = ContentSuggestionsService::State;
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(!profile->IsOffTheRecord());
  PrefService* pref_service = profile->GetPrefs();

  auto user_classifier = base::MakeUnique<UserClassifier>(
      pref_service, base::MakeUnique<base::DefaultClock>());
  auto* user_classifier_raw = user_classifier.get();

  // Create the RemoteSuggestionsScheduler.
  PersistentScheduler* persistent_scheduler = nullptr;
#if defined(OS_ANDROID)
  persistent_scheduler = NTPSnippetsLauncher::Get();
#endif  // OS_ANDROID
  auto scheduler = base::MakeUnique<RemoteSuggestionsSchedulerImpl>(
      persistent_scheduler, user_classifier_raw, pref_service,
      g_browser_process->local_state(), base::MakeUnique<base::DefaultClock>());

  // Create the ContentSuggestionsService.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  favicon::LargeIconService* large_icon_service =
      LargeIconServiceFactory::GetForBrowserContext(profile);
  std::unique_ptr<CategoryRanker> category_ranker =
      ntp_snippets::BuildSelectedCategoryRanker(
          pref_service, base::MakeUnique<base::DefaultClock>());
  auto* service = new ContentSuggestionsService(
      State::ENABLED, signin_manager, history_service, large_icon_service,
      pref_service, std::move(category_ranker), std::move(user_classifier),
      std::move(scheduler));

#if defined(OS_ANDROID)
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(profile);
  if (IsRecentTabProviderEnabled()) {
    RequestCoordinator* request_coordinator =
        RequestCoordinatorFactory::GetForBrowserContext(profile);
    RegisterRecentTabProvider(offline_page_model, request_coordinator, service,
                              pref_service);
  }

  bool show_asset_downloads =
      !IsChromeHomeEnabled() &&
      base::FeatureList::IsEnabled(features::kAssetDownloadSuggestionsFeature);
  bool show_offline_page_downloads =
      !IsChromeHomeEnabled() &&
      base::FeatureList::IsEnabled(
          features::kOfflinePageDownloadSuggestionsFeature);
  if (show_asset_downloads || show_offline_page_downloads) {
    DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(profile);
    DownloadCoreService* download_core_service =
        DownloadCoreServiceFactory::GetForBrowserContext(profile);
    DownloadHistory* download_history =
        download_core_service->GetDownloadHistory();
    RegisterDownloadsProvider(
        show_offline_page_downloads ? offline_page_model : nullptr,
        show_asset_downloads ? download_manager : nullptr, download_history,
        service, pref_service);
  }

  offline_pages::SuggestedArticlesObserver::ObserveContentSuggestionsService(
      profile, service);
#endif  // OS_ANDROID

  // |bookmark_model| can be null in tests.
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  if (base::FeatureList::IsEnabled(ntp_snippets::kBookmarkSuggestionsFeature) &&
      bookmark_model && !IsChromeHomeEnabled()) {
    RegisterBookmarkProvider(bookmark_model, service, pref_service);
  }

#if defined(OS_ANDROID)
  if (IsPhysicalWebPageProviderEnabled()) {
    PhysicalWebDataSource* physical_web_data_source =
        g_browser_process->GetPhysicalWebDataSource();
    RegisterPhysicalWebPageProvider(service, physical_web_data_source,
                                    pref_service);
  }
#endif  // OS_ANDROID

  if (base::FeatureList::IsEnabled(ntp_snippets::kArticleSuggestionsFeature)) {
    OAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
    LanguageModel* language_model =
        LanguageModelFactory::GetInstance()->GetForBrowserContext(profile);
    RegisterArticleProvider(signin_manager, token_service, service,
                            language_model, user_classifier_raw, pref_service,
                            profile);
  }

  if (base::FeatureList::IsEnabled(
          ntp_snippets::kForeignSessionsSuggestionsFeature)) {
    SyncService* sync_service =
        ProfileSyncServiceFactory::GetSyncServiceForBrowserContext(profile);
    RegisterForeignSessionsProvider(sync_service, service, pref_service);
  }

  return service;

#else
  return nullptr;
#endif  // CONTENT_SUGGESTIONS_ENABLED
}
