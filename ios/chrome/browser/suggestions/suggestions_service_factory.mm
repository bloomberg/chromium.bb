// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/suggestions/suggestions_service_factory.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/default_tick_clock.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/image_manager.h"
#include "components/suggestions/suggestions_service_impl.h"
#include "components/suggestions/suggestions_store.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace suggestions {
namespace {
const base::FilePath::CharType kThumbnailDirectory[] =
    FILE_PATH_LITERAL("Thumbnail");
}

// static
SuggestionsServiceFactory* SuggestionsServiceFactory::GetInstance() {
  return base::Singleton<SuggestionsServiceFactory>::get();
}

// static
SuggestionsService* SuggestionsServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<SuggestionsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

SuggestionsServiceFactory::SuggestionsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SuggestionsService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::SigninManagerFactory::GetInstance());
  DependsOn(OAuth2TokenServiceFactory::GetInstance());
  DependsOn(IOSChromeProfileSyncServiceFactory::GetInstance());
}

SuggestionsServiceFactory::~SuggestionsServiceFactory() {
}

std::unique_ptr<KeyedService>
SuggestionsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  SigninManager* signin_manager =
      ios::SigninManagerFactory::GetForBrowserState(browser_state);
  ProfileOAuth2TokenService* token_service =
      OAuth2TokenServiceFactory::GetForBrowserState(browser_state);
  browser_sync::ProfileSyncService* sync_service =
      IOSChromeProfileSyncServiceFactory::GetForBrowserState(browser_state);
  base::FilePath database_dir(
      browser_state->GetStatePath().Append(kThumbnailDirectory));

  std::unique_ptr<SuggestionsStore> suggestions_store(
      new SuggestionsStore(browser_state->GetPrefs()));
  std::unique_ptr<BlacklistStore> blacklist_store(
      new BlacklistStore(browser_state->GetPrefs()));

  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND});
  std::unique_ptr<leveldb_proto::ProtoDatabaseImpl<ImageData>> db(
      new leveldb_proto::ProtoDatabaseImpl<ImageData>(db_task_runner));

  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher =
      std::make_unique<image_fetcher::ImageFetcherImpl>(
          image_fetcher::CreateIOSImageDecoder(),
          browser_state->GetRequestContext());

  std::unique_ptr<ImageManager> thumbnail_manager(
      new ImageManager(std::move(image_fetcher), std::move(db), database_dir));

  return std::make_unique<SuggestionsServiceImpl>(
      signin_manager, token_service, sync_service,
      browser_state->GetRequestContext(), std::move(suggestions_store),
      std::move(thumbnail_manager), std::move(blacklist_store),
      std::make_unique<base::DefaultTickClock>());
}

void SuggestionsServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SuggestionsServiceImpl::RegisterProfilePrefs(registry);
}

}  // namespace suggestions
