// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_GCM_NETWORK_CHANNEL_DELEGATE_IMPL_H_
#define CHROME_BROWSER_INVALIDATION_GCM_NETWORK_CHANNEL_DELEGATE_IMPL_H_

#include "google_apis/gaia/oauth2_token_service.h"
#include "sync/notifier/gcm_network_channel_delegate.h"

class Profile;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace invalidation {

// Implementation of GCMNetworkChannelDelegate.
// GCMNetworkChannel lives in sync/notifier and therefore doesn't have access to
// ProfileOAuth2TokenService and GCMPRofileService that it needs.
// GCMNetworkChannelDelegate declares abstract interface for these functions
// which GCMNetworkChannelDelegateImpl implements in chrome/browser/invalidation
// where it has access to BrowserContext keyed services.
class GCMNetworkChannelDelegateImpl : public syncer::GCMNetworkChannelDelegate,
                                      public OAuth2TokenService::Consumer {
 public:
  explicit GCMNetworkChannelDelegateImpl(Profile* profile);
  virtual ~GCMNetworkChannelDelegateImpl();

  // GCMNetworkChannelDelegate implementation.
  virtual void Register(RegisterCallback callback) OVERRIDE;
  virtual void RequestToken(RequestTokenCallback callback) OVERRIDE;
  virtual void InvalidateToken(const std::string& token) OVERRIDE;

  // OAuth2TokenService::Consumer implementation.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

 private:
  static void InvalidateTokenOnUIThread(Profile* profile,
                                        const std::string& token);

  Profile* profile_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;

  // Fields related to RequestToken function.
  std::string account_id_;
  scoped_ptr<OAuth2TokenService::Request> access_token_request_;
  RequestTokenCallback request_token_callback_;
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_GCM_NETWORK_CHANNEL_DELEGATE_IMPL_H_
