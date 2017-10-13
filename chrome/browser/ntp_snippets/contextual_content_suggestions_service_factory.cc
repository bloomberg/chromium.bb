// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/contextual_content_suggestions_service_factory.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/ntp_snippets/contextual/contextual_content_suggestions_service.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_fetcher_impl.h"
#include "components/ntp_snippets/remote/cached_image_fetcher.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/service_manager_connection.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#endif

using ntp_snippets::ContextualSuggestionsFetcherImpl;
using ntp_snippets::ContextualContentSuggestionsService;
using ntp_snippets::RemoteSuggestionsDatabase;

namespace {

bool IsContextualContentSuggestionsEnabled() {
#if defined(OS_ANDROID)
  return base::FeatureList::IsEnabled(
      chrome::android::kContextualSuggestionsCarousel);
#else
  return false;
#endif  // OS_ANDROID
}

}  // namespace

// static
ContextualContentSuggestionsServiceFactory*
ContextualContentSuggestionsServiceFactory::GetInstance() {
  return base::Singleton<ContextualContentSuggestionsServiceFactory>::get();
}

// static
ContextualContentSuggestionsService*
ContextualContentSuggestionsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ContextualContentSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContextualContentSuggestionsService*
ContextualContentSuggestionsServiceFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<ContextualContentSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

ContextualContentSuggestionsServiceFactory::
    ContextualContentSuggestionsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ContextualContentSuggestionsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
}

ContextualContentSuggestionsServiceFactory::
    ~ContextualContentSuggestionsServiceFactory() = default;

KeyedService*
ContextualContentSuggestionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(!profile->IsOffTheRecord());
  if (!IsContextualContentSuggestionsEnabled()) {
    return nullptr;
  }

  PrefService* pref_service = profile->GetPrefs();
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  OAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  scoped_refptr<net::URLRequestContextGetter> request_context =
      profile->GetRequestContext();
  auto contextual_suggestions_fetcher =
      base::MakeUnique<ContextualSuggestionsFetcherImpl>(
          signin_manager, token_service, request_context, pref_service,
          base::Bind(&data_decoder::SafeJsonParser::Parse,
                     content::ServiceManagerConnection::GetForProcess()
                         ->GetConnector()));
  const base::FilePath::CharType kDatabaseFolder[] =
      FILE_PATH_LITERAL("contextualSuggestionsDatabase");
  base::FilePath database_dir(profile->GetPath().Append(kDatabaseFolder));
  auto contextual_suggestions_database =
      base::MakeUnique<RemoteSuggestionsDatabase>(database_dir);
  auto cached_image_fetcher =
      base::MakeUnique<ntp_snippets::CachedImageFetcher>(
          base::MakeUnique<image_fetcher::ImageFetcherImpl>(
              base::MakeUnique<suggestions::ImageDecoderImpl>(),
              request_context.get()),
          pref_service, contextual_suggestions_database.get());
  auto* service = new ContextualContentSuggestionsService(
      std::move(contextual_suggestions_fetcher),
      std::move(cached_image_fetcher),
      std::move(contextual_suggestions_database));

  return service;
}
