// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android_profile_oauth2_token_service.h"

#include "base/bind.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"

AndroidProfileOAuth2TokenService::AndroidProfileOAuth2TokenService(
    net::URLRequestContextGetter* getter)
    : ProfileOAuth2TokenService(getter) {
}

AndroidProfileOAuth2TokenService::~AndroidProfileOAuth2TokenService() {
}

scoped_ptr<OAuth2TokenService::Request>
    AndroidProfileOAuth2TokenService::StartRequest(
        const OAuth2TokenService::ScopeSet& scopes,
        OAuth2TokenService::Consumer* consumer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (HasCacheEntry(scopes))
    return StartCacheLookupRequest(scopes, consumer);

  scoped_ptr<RequestImpl> request(new RequestImpl(consumer));
  DCHECK_EQ(scopes.size(), 1U);
  std::vector<std::string> scope_list(scopes.begin(), scopes.end());
  ProfileSyncServiceAndroid* sync_service =
      ProfileSyncServiceAndroid::GetProfileSyncServiceAndroid();
  sync_service->FetchOAuth2Token(
      scope_list.front(),
      base::Bind(&OAuth2TokenService::InformConsumer,
                 request->AsWeakPtr()));
  return request.PassAs<Request>();
}

void AndroidProfileOAuth2TokenService::InvalidateToken(
    const ScopeSet& scopes,
    const std::string& invalid_token) {
  OAuth2TokenService::InvalidateToken(scopes, invalid_token);

  DCHECK_EQ(scopes.size(), 1U);
  std::vector<std::string> scope_list(scopes.begin(), scopes.end());
  ProfileSyncServiceAndroid* sync_service =
      ProfileSyncServiceAndroid::GetProfileSyncServiceAndroid();
  sync_service->InvalidateOAuth2Token(
      scope_list.front(),
      invalid_token);
}

bool AndroidProfileOAuth2TokenService::ShouldCacheForRefreshToken(
    TokenService *token_service,
    const std::string& refresh_token) {
  // The parent class skips caching if the TokenService login token is stale,
  // but on Android the user is always logged in to exactly one profile, so
  // this concept doesn't exist and we can simply always cache.
  return true;
}
