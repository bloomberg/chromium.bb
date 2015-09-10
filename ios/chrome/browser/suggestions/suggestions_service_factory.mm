// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/suggestions/suggestions_service_factory.h"

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/image_fetcher.h"
#include "components/suggestions/image_manager.h"
#include "components/suggestions/suggestions_service.h"
#include "components/suggestions/suggestions_store.h"
#include "ios/chrome/browser/suggestions/image_fetcher_impl.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_thread.h"

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
  // No dependencies.
}

SuggestionsServiceFactory::~SuggestionsServiceFactory() {
}

scoped_ptr<KeyedService> SuggestionsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  base::SequencedWorkerPool* sequenced_worker_pool =
      web::WebThread::GetBlockingPool();
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      sequenced_worker_pool->GetSequencedTaskRunner(
          sequenced_worker_pool->GetSequenceToken());

  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  base::FilePath database_dir(
      browser_state->GetStatePath().Append(kThumbnailDirectory));
  scoped_ptr<SuggestionsStore> suggestions_store(
      new SuggestionsStore(browser_state->GetPrefs()));
  scoped_ptr<BlacklistStore> blacklist_store(
      new BlacklistStore(browser_state->GetPrefs()));
  scoped_ptr<leveldb_proto::ProtoDatabaseImpl<ImageData>> db(
      new leveldb_proto::ProtoDatabaseImpl<ImageData>(background_task_runner));
  scoped_ptr<ImageFetcher> image_fetcher(new ImageFetcherImpl(
      browser_state->GetRequestContext(), sequenced_worker_pool));
  scoped_ptr<ImageManager> thumbnail_manager(new ImageManager(
      image_fetcher.Pass(), db.Pass(), database_dir,
      web::WebThread::GetTaskRunnerForThread(web::WebThread::DB)));
  return make_scoped_ptr(new SuggestionsService(
      browser_state->GetRequestContext(), suggestions_store.Pass(),
      thumbnail_manager.Pass(), blacklist_store.Pass()));
}

void SuggestionsServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SuggestionsService::RegisterProfilePrefs(registry);
}

}  // namespace suggestions
