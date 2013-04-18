// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_REQUEST_H_
#define CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_REQUEST_H_

#include <set>
#include <string>

#include "base/threading/non_thread_safe.h"
#include "chrome/browser/signin/oauth2_token_service.h"

class Profile;

// ProfileOAuth2TokenServiceRequest represents a request to fetch an
// OAuth2 access token for a given set of |scopes| by calling |profile|'s
// ProfileOAuth2TokenService. A request can be created and started from
// any thread with an object |consumer| that will be called back on the
// same thread when fetching completes.  If the request is destructed
// before |consumer| is called, |consumer| will never be called back. (Note
// the actual network activities are not canceled and the cache in
// ProfileOAuth2TokenService will be populated with the fetched results.)
class ProfileOAuth2TokenServiceRequest : public OAuth2TokenService::Request,
                                         public base::NonThreadSafe {
 public:
  static ProfileOAuth2TokenServiceRequest* CreateAndStart(
      Profile* profile,
      const OAuth2TokenService::ScopeSet& scopes,
      OAuth2TokenService::Consumer* consumer);

  virtual ~ProfileOAuth2TokenServiceRequest();

 private:
  class Core;
  friend class Core;

  ProfileOAuth2TokenServiceRequest(Profile* profile,
                            const OAuth2TokenService::ScopeSet& scopes,
                            OAuth2TokenService::Consumer* consumer);
  OAuth2TokenService::Consumer* const consumer_;
  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenServiceRequest);
};

#endif  // CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_REQUEST_H_
