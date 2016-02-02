// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/suggestions_service_factory.h"

#include <utility>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_fetcher_impl.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/image_fetcher.h"
#include "components/suggestions/image_manager.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_service.h"
#include "components/suggestions/suggestions_store.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace suggestions {

// static
SuggestionsService* SuggestionsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SuggestionsServiceFactory* SuggestionsServiceFactory::GetInstance() {
  return base::Singleton<SuggestionsServiceFactory>::get();
}

SuggestionsServiceFactory::SuggestionsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SuggestionsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

SuggestionsServiceFactory::~SuggestionsServiceFactory() {}

KeyedService* SuggestionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
          base::SequencedWorkerPool::GetSequenceToken());

  Profile* the_profile = static_cast<Profile*>(profile);

  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(the_profile);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(the_profile);

  scoped_ptr<SuggestionsStore> suggestions_store(
      new SuggestionsStore(the_profile->GetPrefs()));
  scoped_ptr<BlacklistStore> blacklist_store(
      new BlacklistStore(the_profile->GetPrefs()));

  scoped_ptr<leveldb_proto::ProtoDatabaseImpl<ImageData> > db(
      new leveldb_proto::ProtoDatabaseImpl<ImageData>(background_task_runner));

  base::FilePath database_dir(
      the_profile->GetPath().Append(FILE_PATH_LITERAL("Thumbnails")));

  scoped_ptr<ImageFetcherImpl> image_fetcher(
      new ImageFetcherImpl(the_profile->GetRequestContext()));
  scoped_ptr<ImageManager> thumbnail_manager(
      new ImageManager(
          std::move(image_fetcher), std::move(db), database_dir,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
  return new SuggestionsService(
      signin_manager, token_service, the_profile->GetRequestContext(),
      std::move(suggestions_store), std::move(thumbnail_manager),
      std::move(blacklist_store));
}

void SuggestionsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SuggestionsService::RegisterProfilePrefs(registry);
}

}  // namespace suggestions
