// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/ntp_snippets/dependent_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/ntp_snippets/bookmarks/bookmark_suggestions_provider.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/logger.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/prefetched_pages_tracker.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/remote/remote_suggestions_fetcher_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_status_service_impl.h"
#include "components/ntp_snippets/sessions/foreign_sessions_suggestions_provider.h"
#include "components/ntp_snippets/sessions/tab_delegate_sync_adapter.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/offline_pages/features/features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_json/safe_json_parser.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/feature_utilities.h"
#include "chrome/browser/android/ntp/ntp_snippets_launcher.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/ntp_snippets/download_suggestions_provider.h"
#include "components/ntp_snippets/breaking_news/breaking_news_gcm_app_handler.h"
#include "components/ntp_snippets/breaking_news/subscription_manager.h"
#include "components/ntp_snippets/breaking_news/subscription_manager_impl.h"
#include "components/ntp_snippets/physical_web_pages/physical_web_page_suggestions_provider.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "components/ntp_snippets/offline_pages/recent_tab_suggestions_provider.h"
#include "components/ntp_snippets/remote/prefetched_pages_tracker_impl.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#include "components/offline_pages/core/recent_tabs/recent_tabs_ui_adapter_delegate.h"
#endif

using bookmarks::BookmarkModel;
using content::BrowserThread;
using history::HistoryService;
using image_fetcher::ImageFetcherImpl;
using language::UrlLanguageHistogram;
using ntp_snippets::AreAssetDownloadsEnabled;
using ntp_snippets::AreOfflinePageDownloadsEnabled;
using ntp_snippets::BookmarkSuggestionsProvider;
using ntp_snippets::BreakingNewsListener;
using ntp_snippets::CategoryRanker;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::ForeignSessionsSuggestionsProvider;
using ntp_snippets::GetFetchEndpoint;
using ntp_snippets::IsBookmarkProviderEnabled;
using ntp_snippets::IsDownloadsProviderEnabled;
using ntp_snippets::IsForeignSessionsProviderEnabled;
using ntp_snippets::IsPhysicalWebPageProviderEnabled;
using ntp_snippets::IsRecentTabProviderEnabled;
using ntp_snippets::PersistentScheduler;
using ntp_snippets::PrefetchedPagesTracker;
using ntp_snippets::RemoteSuggestionsDatabase;
using ntp_snippets::RemoteSuggestionsFetcherImpl;
using ntp_snippets::RemoteSuggestionsProviderImpl;
using ntp_snippets::RemoteSuggestionsSchedulerImpl;
using ntp_snippets::RemoteSuggestionsStatusServiceImpl;
using ntp_snippets::TabDelegateSyncAdapter;
using ntp_snippets::UserClassifier;
using suggestions::ImageDecoderImpl;
using syncer::SyncService;

#if defined(OS_ANDROID)
using chrome::android::GetIsChromeHomeEnabled;
using content::DownloadManager;
using ntp_snippets::BreakingNewsGCMAppHandler;
using ntp_snippets::GetPushUpdatesSubscriptionEndpoint;
using ntp_snippets::GetPushUpdatesUnsubscriptionEndpoint;
using ntp_snippets::PhysicalWebPageSuggestionsProvider;
using ntp_snippets::SubscriptionManagerImpl;
using physical_web::PhysicalWebDataSource;
#endif  // OS_ANDROID

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
using ntp_snippets::PrefetchedPagesTrackerImpl;
using ntp_snippets::RecentTabSuggestionsProvider;
using offline_pages::OfflinePageModel;
using offline_pages::OfflinePageModelFactory;
using offline_pages::RequestCoordinator;
using offline_pages::RequestCoordinatorFactory;
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

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

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)

void RegisterRecentTabProviderIfEnabled(ContentSuggestionsService* service,
                                        Profile* profile,
                                        OfflinePageModel* offline_page_model) {
  if (!IsRecentTabProviderEnabled()) {
    return;
  }

  RequestCoordinator* request_coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(profile);
  offline_pages::DownloadUIAdapter* ui_adapter = offline_pages::
      RecentTabsUIAdapterDelegate::GetOrCreateRecentTabsUIAdapter(
          offline_page_model, request_coordinator);
  auto provider = base::MakeUnique<RecentTabSuggestionsProvider>(
      service, ui_adapter, profile->GetPrefs());
  service->RegisterProvider(std::move(provider));
}

void RegisterPrefetchingObserver(ContentSuggestionsService* service,
                                 Profile* profile) {
  // The observer is always there, but the Prefetch Dispatcher will do nothing
  // if the feature (or preference) is off.
  offline_pages::SuggestedArticlesObserver* observer =
      offline_pages::PrefetchServiceFactory::GetForBrowserContext(profile)
          ->GetSuggestedArticlesObserver();
  observer->SetContentSuggestionsServiceAndObserve(service);
}

#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

#if defined(OS_ANDROID)

void RegisterDownloadsProviderIfEnabled(ContentSuggestionsService* service,
                                        Profile* profile,
                                        OfflinePageModel* offline_page_model) {
  if (!IsDownloadsProviderEnabled()) {
    return;
  }

  offline_page_model =
      AreOfflinePageDownloadsEnabled() ? offline_page_model : nullptr;
  DownloadManager* download_manager =
      AreAssetDownloadsEnabled()
          ? content::BrowserContext::GetDownloadManager(profile)
          : nullptr;
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(profile);
  DownloadHistory* download_history =
      download_core_service->GetDownloadHistory();

  auto provider = base::MakeUnique<DownloadSuggestionsProvider>(
      service, offline_page_model, download_manager, download_history,
      profile->GetPrefs(), base::MakeUnique<base::DefaultClock>());
  service->RegisterProvider(std::move(provider));
}

#endif  // OS_ANDROID

void RegisterBookmarkProviderIfEnabled(ContentSuggestionsService* service,
                                       Profile* profile) {
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  if (!bookmark_model || !IsBookmarkProviderEnabled()) {
    // bookmark_model may be null in tests.
    return;
  }

  auto provider =
      base::MakeUnique<BookmarkSuggestionsProvider>(service, bookmark_model);
  service->RegisterProvider(std::move(provider));
}

#if defined(OS_ANDROID)

void RegisterPhysicalWebPageProviderIfEnabled(
    ContentSuggestionsService* service,
    Profile* profile) {
  if (!IsPhysicalWebPageProviderEnabled()) {
    return;
  }

  PhysicalWebDataSource* physical_web_data_source =
      g_browser_process->GetPhysicalWebDataSource();
  auto provider = base::MakeUnique<PhysicalWebPageSuggestionsProvider>(
      service, physical_web_data_source, profile->GetPrefs());
  service->RegisterProvider(std::move(provider));
}

#endif  // OS_ANDROID

#if defined(OS_ANDROID)

bool AreGCMPushUpdatesEnabled() {
  return base::FeatureList::IsEnabled(ntp_snippets::kBreakingNewsPushFeature);
}

std::unique_ptr<BreakingNewsGCMAppHandler>
MakeBreakingNewsGCMAppHandlerIfEnabled(
    Profile* profile,
    const std::string& locale,
    variations::VariationsService* variations_service) {
  PrefService* pref_service = profile->GetPrefs();

  if (!AreGCMPushUpdatesEnabled()) {
    BreakingNewsGCMAppHandler::ClearProfilePrefs(pref_service);
    SubscriptionManagerImpl::ClearProfilePrefs(pref_service);
    return nullptr;
  }

  gcm::GCMDriver* gcm_driver =
      gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver();

  OAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);

  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);

  scoped_refptr<net::URLRequestContextGetter> request_context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLRequestContext();

  std::string api_key;
  // The API is private. If we don't have the official API key, don't even try.
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    bool is_stable_channel =
        chrome::GetChannel() == version_info::Channel::STABLE;
    api_key = is_stable_channel ? google_apis::GetAPIKey()
                                : google_apis::GetNonStableAPIKey();
  }

  auto subscription_manager = base::MakeUnique<SubscriptionManagerImpl>(
      request_context, pref_service, variations_service, signin_manager,
      token_service, api_key, locale,
      GetPushUpdatesSubscriptionEndpoint(chrome::GetChannel()),
      GetPushUpdatesUnsubscriptionEndpoint(chrome::GetChannel()));

  instance_id::InstanceIDProfileService* instance_id_profile_service =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile);
  DCHECK(instance_id_profile_service);
  DCHECK(instance_id_profile_service->driver());

  return base::MakeUnique<BreakingNewsGCMAppHandler>(
      gcm_driver, instance_id_profile_service->driver(), pref_service,
      std::move(subscription_manager),
      base::Bind(&safe_json::SafeJsonParser::Parse),
      base::MakeUnique<base::DefaultClock>(),
      /*token_validation_timer=*/base::MakeUnique<base::OneShotTimer>(),
      /*forced_subscription_timer=*/base::MakeUnique<base::OneShotTimer>());
}

#endif  // OS_ANDROID

bool IsArticleProviderEnabled() {
  return base::FeatureList::IsEnabled(ntp_snippets::kArticleSuggestionsFeature);
}

bool IsKeepingPrefetchedSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(
      ntp_snippets::kKeepPrefetchedContentSuggestions);
}

void RegisterArticleProviderIfEnabled(ContentSuggestionsService* service,
                                      Profile* profile,
                                      SigninManagerBase* signin_manager,
                                      UserClassifier* user_classifier,
                                      OfflinePageModel* offline_page_model,
                                      ntp_snippets::Logger* debug_logger) {
  if (!IsArticleProviderEnabled()) {
    return;
  }

  PrefService* pref_service = profile->GetPrefs();
  OAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  UrlLanguageHistogram* language_histogram =
      UrlLanguageHistogramFactory::GetInstance()->GetForBrowserContext(profile);

  scoped_refptr<net::URLRequestContextGetter> request_context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLRequestContext();

  base::FilePath database_dir(
      profile->GetPath().Append(ntp_snippets::kDatabaseFolder));
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
  std::unique_ptr<PrefetchedPagesTracker> prefetched_pages_tracker;
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  if (IsKeepingPrefetchedSuggestionsEnabled()) {
    prefetched_pages_tracker =
        base::MakeUnique<PrefetchedPagesTrackerImpl>(offline_page_model);
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
  auto suggestions_fetcher = base::MakeUnique<RemoteSuggestionsFetcherImpl>(
      signin_manager, token_service, request_context, pref_service,
      language_histogram, base::Bind(&safe_json::SafeJsonParser::Parse),
      GetFetchEndpoint(chrome::GetChannel()), api_key, user_classifier);

  std::unique_ptr<BreakingNewsListener> breaking_news_raw_data_provider;
#if defined(OS_ANDROID)
  breaking_news_raw_data_provider = MakeBreakingNewsGCMAppHandlerIfEnabled(
      profile, g_browser_process->GetApplicationLocale(),
      g_browser_process->variations_service());
#endif  //  OS_ANDROID

  auto provider = base::MakeUnique<RemoteSuggestionsProviderImpl>(
      service, pref_service, g_browser_process->GetApplicationLocale(),
      service->category_ranker(), service->remote_suggestions_scheduler(),
      std::move(suggestions_fetcher),
      base::MakeUnique<ImageFetcherImpl>(base::MakeUnique<ImageDecoderImpl>(),
                                         request_context.get()),
      base::MakeUnique<RemoteSuggestionsDatabase>(database_dir),
      base::MakeUnique<RemoteSuggestionsStatusServiceImpl>(
          signin_manager, pref_service, additional_toggle_pref),
      std::move(prefetched_pages_tracker),
      std::move(breaking_news_raw_data_provider), debug_logger,
      std::make_unique<base::OneShotTimer>());

  service->remote_suggestions_scheduler()->SetProvider(provider.get());
  service->set_remote_suggestions_provider(provider.get());
  service->RegisterProvider(std::move(provider));
}

void RegisterForeignSessionsProviderIfEnabled(
    ContentSuggestionsService* service,
    Profile* profile) {
  if (!IsForeignSessionsProviderEnabled()) {
    return;
  }

  SyncService* sync_service =
      ProfileSyncServiceFactory::GetSyncServiceForBrowserContext(profile);
  std::unique_ptr<TabDelegateSyncAdapter> sync_adapter =
      base::MakeUnique<TabDelegateSyncAdapter>(sync_service);
  auto provider = base::MakeUnique<ForeignSessionsSuggestionsProvider>(
      service, std::move(sync_adapter), profile->GetPrefs());
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
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  DependsOn(OfflinePageModelFactory::GetInstance());
  DependsOn(offline_pages::PrefetchServiceFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
#if defined(OS_ANDROID)
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());
#endif  // defined(OS_ANDROID)
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

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(profile);
#else
  OfflinePageModel* offline_page_model = nullptr;
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  auto debug_logger = base::MakeUnique<ntp_snippets::Logger>();
  ntp_snippets::Logger* raw_debug_logger = debug_logger.get();

  // Create the RemoteSuggestionsScheduler.
  PersistentScheduler* persistent_scheduler = nullptr;
#if defined(OS_ANDROID)
  persistent_scheduler = NTPSnippetsLauncher::Get();
#endif  // OS_ANDROID
  auto scheduler = base::MakeUnique<RemoteSuggestionsSchedulerImpl>(
      persistent_scheduler, user_classifier_raw, pref_service,
      g_browser_process->local_state(), base::MakeUnique<base::DefaultClock>(),
      raw_debug_logger);

  // Create the ContentSuggestionsService.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  favicon::LargeIconService* large_icon_service =
      LargeIconServiceFactory::GetForBrowserContext(profile);
  std::unique_ptr<CategoryRanker> category_ranker =
      ntp_snippets::BuildSelectedCategoryRanker(
          pref_service, base::MakeUnique<base::DefaultClock>(),
          GetIsChromeHomeEnabled());

  auto* service = new ContentSuggestionsService(
      State::ENABLED, signin_manager, history_service, large_icon_service,
      pref_service, std::move(category_ranker), std::move(user_classifier),
      std::move(scheduler), std::move(debug_logger));

  RegisterArticleProviderIfEnabled(service, profile, signin_manager,
                                   user_classifier_raw, offline_page_model,
                                   raw_debug_logger);
  RegisterBookmarkProviderIfEnabled(service, profile);
  RegisterForeignSessionsProviderIfEnabled(service, profile);

#if defined(OS_ANDROID)
  RegisterDownloadsProviderIfEnabled(service, profile, offline_page_model);
  RegisterPhysicalWebPageProviderIfEnabled(service, profile);
#endif  // OS_ANDROID

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  RegisterRecentTabProviderIfEnabled(service, profile, offline_page_model);
  RegisterPrefetchingObserver(service, profile);
#endif

  return service;

#else
  return nullptr;
#endif  // CONTENT_SUGGESTIONS_ENABLED
}
