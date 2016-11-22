// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/image_fetcher/image_decoder.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/ntp_snippets/bookmarks/bookmark_suggestions_provider.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/remote/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/remote/ntp_snippets_status_service.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/signin/oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/suggestions/image_fetcher_impl.h"
#include "ios/chrome/browser/suggestions/ios_image_decoder_impl.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_thread.h"
#include "net/url_request/url_request_context_getter.h"

using bookmarks::BookmarkModel;
using history::HistoryService;
using ios::BookmarkModelFactory;
using ntp_snippets::BookmarkSuggestionsProvider;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::NTPSnippetsFetcher;
using ntp_snippets::NTPSnippetsScheduler;
using ntp_snippets::NTPSnippetsStatusService;
using ntp_snippets::RemoteSuggestionsDatabase;
using ntp_snippets::RemoteSuggestionsProvider;
using suggestions::CreateIOSImageDecoder;
using suggestions::ImageFetcherImpl;

namespace {

void ParseJson(const std::string& json,
               const NTPSnippetsFetcher::SuccessCallback& success_callback,
               const NTPSnippetsFetcher::ErrorCallback& error_callback) {
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
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(OAuth2TokenServiceFactory::GetInstance());
  DependsOn(ios::SigninManagerFactory::GetInstance());
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

  // Create the ContentSuggestionsService.
  State state =
      base::FeatureList::IsEnabled(ntp_snippets::kContentSuggestionsFeature)
          ? State::ENABLED
          : State::DISABLED;
  SigninManager* signin_manager =
      ios::SigninManagerFactory::GetForBrowserState(chrome_browser_state);
  HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          chrome_browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  std::unique_ptr<ContentSuggestionsService> service =
      base::MakeUnique<ContentSuggestionsService>(state, signin_manager,
                                                  history_service, prefs);
  if (state == State::DISABLED)
    return std::move(service);

  // Create the BookmarkSuggestionsProvider.
  if (base::FeatureList::IsEnabled(ntp_snippets::kBookmarkSuggestionsFeature)) {
    BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserState(chrome_browser_state);
    std::unique_ptr<BookmarkSuggestionsProvider> bookmark_suggestions_provider =
        base::MakeUnique<BookmarkSuggestionsProvider>(
            service.get(), service->category_factory(), bookmark_model, prefs);
    service->RegisterProvider(std::move(bookmark_suggestions_provider));
  }

  if (base::FeatureList::IsEnabled(ntp_snippets::kArticleSuggestionsFeature)) {
    // Create the RemoteSuggestionsProvider (articles provider).
    OAuth2TokenService* token_service =
        OAuth2TokenServiceFactory::GetForBrowserState(chrome_browser_state);
    scoped_refptr<net::URLRequestContextGetter> request_context =
        browser_state->GetRequestContext();
    NTPSnippetsScheduler* scheduler = nullptr;
    base::FilePath database_dir(
        browser_state->GetStatePath().Append(ntp_snippets::kDatabaseFolder));
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        web::WebThread::GetBlockingPool()
            ->GetSequencedTaskRunnerWithShutdownBehavior(
                base::SequencedWorkerPool::GetSequenceToken(),
                base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
    std::unique_ptr<RemoteSuggestionsProvider> ntp_snippets_service =
        base::MakeUnique<RemoteSuggestionsProvider>(
            service.get(), service->category_factory(), prefs,
            GetApplicationContext()->GetApplicationLocale(),
            service->user_classifier(), scheduler,
            base::MakeUnique<NTPSnippetsFetcher>(
                signin_manager, token_service, request_context, prefs,
                service->category_factory(), nullptr, base::Bind(&ParseJson),
                GetChannel() == version_info::Channel::STABLE
                    ? google_apis::GetAPIKey()
                    : google_apis::GetNonStableAPIKey(),
                service->user_classifier()),
            base::MakeUnique<ImageFetcherImpl>(
                request_context.get(), web::WebThread::GetBlockingPool()),
            CreateIOSImageDecoder(task_runner),
            base::MakeUnique<RemoteSuggestionsDatabase>(database_dir,
                                                        task_runner),
            base::MakeUnique<NTPSnippetsStatusService>(signin_manager, prefs));
    service->set_ntp_snippets_service(ntp_snippets_service.get());
    service->RegisterProvider(std::move(ntp_snippets_service));
  }

  return std::move(service);
}
