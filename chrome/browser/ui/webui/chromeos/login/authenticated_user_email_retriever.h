// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTHENTICATED_USER_EMAIL_RETRIEVER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTHENTICATED_USER_EMAIL_RETRIEVER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context_getter.h"

class GaiaAuthFetcher;

namespace chromeos {

// Helper class that retrieves the authenticated user's e-mail address.
class AuthenticatedUserEmailRetriever : public GaiaAuthConsumer {
 public:
  typedef base::Callback<void(const std::string&)>
      AuthenticatedUserEmailCallback;

  // Extracts the GAIA cookies from |url_request_context_getter|, retrieves the
  // authenticated user's e-mail address from GAIA and passes it to |callback|.
  // If the e-mail address cannot be retrieved, an empty string is passed to
  // the |callback|.
  AuthenticatedUserEmailRetriever(
      const AuthenticatedUserEmailCallback& callback,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);
  virtual ~AuthenticatedUserEmailRetriever();

  // GaiaAuthConsumer:
  virtual void OnGetUserInfoSuccess(const UserInfoMap& data) OVERRIDE;
  virtual void OnGetUserInfoFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

 private:
  void ExtractCookies(scoped_refptr<net::CookieStore> cookie_store);
  void ExtractLSIDFromCookies(const net::CookieList& cookies);

  AuthenticatedUserEmailCallback callback_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  scoped_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;

  base::WeakPtrFactory<AuthenticatedUserEmailRetriever> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatedUserEmailRetriever);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTHENTICATED_USER_EMAIL_RETRIEVER_H_
