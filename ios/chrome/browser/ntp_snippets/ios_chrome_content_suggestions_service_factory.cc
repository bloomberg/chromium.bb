// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/reading_list/reading_list_suggestions_provider.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/remote/remote_suggestions_fetcher_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_status_service.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/signin/oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_thread.h"
#include "net/url_request/url_request_context_getter.h"

using history::HistoryService;
using image_fetcher::CreateIOSImageDecoder;
using image_fetcher::ImageFetcherImpl;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::GetFetchEndpoint;
using ntp_snippets::PersistentScheduler;
using ntp_snippets::RemoteSuggestionsDatabase;
using ntp_snippets::RemoteSuggestionsFetcherImpl;
using ntp_snippets::RemoteSuggestionsProviderImpl;
using ntp_snippets::RemoteSuggestionsSchedulerImpl;
using ntp_snippets::RemoteSuggestionsStatusService;
using ntp_snippets::UserClassifier;

namespace {

void ParseJson(const std::string& json,
               const ntp_snippets::SuccessCallback& success_callback,
               const ntp_snippets::ErrorCallback& error_callback) {
  base::JSONReader json_reader;
  std::unique_ptr<base::Value> value = json_reader.ReadToValue(json);
  if (value) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(success_callback, base::Passed(&value)));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(error_callback, json_reader.GetErrorMessage()));
  }
}

}  // namespace

// static
IOSChromeContentSuggestionsServiceFactory*
IOSChromeContentSuggestionsServiceFactory::GetInstance() {
  return base::Singleton<IOSChromeContentSuggestionsServiceFactory>::get();
}

// static
ContentSuggestionsService*
IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  DCHECK(!browser_state->IsOffTheRecord());
  return static_cast<ContentSuggestionsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

IOSChromeContentSuggestionsServiceFactory::
    IOSChromeContentSuggestionsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ContentSuggestionsService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(IOSChromeLargeIconServiceFactory::GetInstance());
  DependsOn(OAuth2TokenServiceFactory::GetInstance());
  DependsOn(ios::SigninManagerFactory::GetInstance());
  DependsOn(ReadingListModelFactory::GetInstance());
}

IOSChromeContentSuggestionsServiceFactory::
    ~IOSChromeContentSuggestionsServiceFactory() {}

std::unique_ptr<KeyedService>
IOSChromeContentSuggestionsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  using State = ContentSuggestionsService::State;
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state);
  DCHECK(!browser_state->IsOffTheRecord());
  PrefService* prefs = chrome_browser_state->GetPrefs();

  auto user_classifier = base::MakeUnique<UserClassifier>(
      prefs, base::MakeUnique<base::DefaultClock>());
  auto* user_classifier_raw = user_classifier.get();

  // TODO(jkrcal): Implement a persistent scheduler for iOS. crbug.com/676249
  auto scheduler = base::MakeUnique<RemoteSuggestionsSchedulerImpl>(
      /*persistent_scheduler=*/nullptr, user_classifier_raw, prefs,
      GetApplicationContext()->GetLocalState(),
      base::MakeUnique<base::DefaultClock>());

  // Create the ContentSuggestionsService.
  SigninManager* signin_manager =
      ios::SigninManagerFactory::GetForBrowserState(chrome_browser_state);
  HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          chrome_browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  favicon::LargeIconService* large_icon_service =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(
          chrome_browser_state);
  std::unique_ptr<ntp_snippets::CategoryRanker> category_ranker =
      ntp_snippets::BuildSelectedCategoryRanker(
          prefs, base::MakeUnique<base::DefaultClock>());
  std::unique_ptr<ContentSuggestionsService> service =
      base::MakeUnique<ContentSuggestionsService>(
          State::ENABLED, signin_manager, history_service, large_icon_service,
          prefs, std::move(category_ranker), std::move(user_classifier),
          std::move(scheduler));

  // Create the ReadingListSuggestionsProvider.
  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetForBrowserState(chrome_browser_state);
  std::unique_ptr<ntp_snippets::ReadingListSuggestionsProvider>
      reading_list_suggestions_provider =
          base::MakeUnique<ntp_snippets::ReadingListSuggestionsProvider>(
              service.get(), reading_list_model);
  service->RegisterProvider(std::move(reading_list_suggestions_provider));

  if (base::FeatureList::IsEnabled(ntp_snippets::kArticleSuggestionsFeature)) {
    // Create the RemoteSuggestionsProvider (articles provider).
    OAuth2TokenService* token_service =
        OAuth2TokenServiceFactory::GetForBrowserState(chrome_browser_state);
    scoped_refptr<net::URLRequestContextGetter> request_context =
        browser_state->GetRequestContext();
    base::FilePath database_dir(
        browser_state->GetStatePath().Append(ntp_snippets::kDatabaseFolder));
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BACKGROUND,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

    std::string api_key;
    // This API needs whitelisted API keys. Get the key only if it is not a
    // dummy key.
    if (google_apis::HasKeysConfigured()) {
      bool is_stable_channel = GetChannel() == version_info::Channel::STABLE;
      api_key = is_stable_channel ? google_apis::GetAPIKey()
                                  : google_apis::GetNonStableAPIKey();
    }
    auto suggestions_fetcher = base::MakeUnique<RemoteSuggestionsFetcherImpl>(
        signin_manager, token_service, request_context, prefs, nullptr,
        base::Bind(&ParseJson), GetFetchEndpoint(GetChannel()), api_key,
        user_classifier_raw);

    auto provider = base::MakeUnique<RemoteSuggestionsProviderImpl>(
        service.get(), prefs, GetApplicationContext()->GetApplicationLocale(),
        service->category_ranker(), service->remote_suggestions_scheduler(),
        std::move(suggestions_fetcher),
        base::MakeUnique<ImageFetcherImpl>(
            CreateIOSImageDecoder(web::WebThread::GetBlockingPool()),
            request_context.get()),
        base::MakeUnique<RemoteSuggestionsDatabase>(database_dir, task_runner),
        base::MakeUnique<RemoteSuggestionsStatusService>(signin_manager, prefs,
                                                         std::string()),
        /*prefetched_pages_tracker=*/nullptr);

    service->remote_suggestions_scheduler()->SetProvider(provider.get());
    service->set_remote_suggestions_provider(provider.get());
    service->RegisterProvider(std::move(provider));
  }

  // TODO(crbug.com/703565): remove std::move() once Xcode 9.0+ is required.
  return std::move(service);
}
