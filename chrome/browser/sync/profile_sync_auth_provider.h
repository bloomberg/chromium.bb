// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_AUTH_PROVIDER_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_AUTH_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "sync/internal_api/public/sync_auth_provider.h"

class ProfileOAuth2TokenService;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

// ProfileSyncAuthProvider implements function for SyncAuthProvider. It doesn't
// inherit from SyncAuthProvider because it lives on UI thread while requests
// originate from sync thread. SyncThreadProxy implements SyncAuthProvider and
// forwards all requests.
class ProfileSyncAuthProvider : public OAuth2TokenService::Consumer,
                                public base::NonThreadSafe {
 public:
  ProfileSyncAuthProvider(ProfileOAuth2TokenService* token_service,
                          const std::string& account_id,
                          const std::string& scope);
  virtual ~ProfileSyncAuthProvider();

  // OAuth2TokenService::Consumer implementation.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // Request access token from OAuth2TokenService. Once access token is received
  // result should be posted to callback on task_runner thread.
  void RequestAccessToken(
      const syncer::SyncAuthProvider::RequestTokenCallback& callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void InvalidateAccessToken(const std::string& token);

  scoped_ptr<syncer::SyncAuthProvider> CreateProviderForSyncThread();

 private:
  class SyncThreadProxy;

  void RespondToTokenRequest(const GoogleServiceAuthError& error,
                             const std::string& token);

  ProfileOAuth2TokenService* token_service_;
  std::string account_id_;
  OAuth2TokenService::ScopeSet oauth2_scope_;

  // Only one outstanding request is allowed. Previous request is reported
  // cancelled if new one arrives.
  scoped_ptr<OAuth2TokenService::Request> access_token_request_;
  syncer::SyncAuthProvider::RequestTokenCallback request_token_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;

  base::WeakPtrFactory<ProfileSyncAuthProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncAuthProvider);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_AUTH_PROVIDER_H_
