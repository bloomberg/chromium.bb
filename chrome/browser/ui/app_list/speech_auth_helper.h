// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SPEECH_AUTH_HELPER_H_
#define CHROME_BROWSER_UI_APP_LIST_SPEECH_AUTH_HELPER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

class Profile;
class ProfileOAuth2TokenService;

namespace base {
class Clock;
}

namespace app_list {

// SpeechAuthHelper is a helper class to generate oauth tokens for audio
// history. This encalsulates generating and refreshing the auth token for a
// given profile. All functions should be called on the UI thread.
class SpeechAuthHelper : public OAuth2TokenService::Consumer,
                         public OAuth2TokenService::Observer {
 public:
  explicit SpeechAuthHelper(Profile* profile, base::Clock* clock);
  ~SpeechAuthHelper() override;

  // Returns the current auth token. If the auth service is not available, or
  // there was a failure in obtaining a token, return the empty string.
  std::string GetToken();

  // Returns the OAuth scope associated with the auth token.
  std::string GetScope();

 private:
  // Overridden from OAuth2TokenService::Consumer:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // Overridden from OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override;

  void ScheduleTokenFetch(const base::TimeDelta& fetch_delay);
  void FetchAuthToken();

  Profile* profile_;
  base::Clock* clock_;
  ProfileOAuth2TokenService* token_service_;
  std::string authenticated_account_id_;
  std::string auth_token_;
  scoped_ptr<OAuth2TokenService::Request> auth_token_request_;

  base::WeakPtrFactory<SpeechAuthHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SpeechAuthHelper);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SPEECH_AUTH_HELPER_H_
