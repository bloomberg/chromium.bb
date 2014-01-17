// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTHENTICATED_USER_EMAIL_RETRIEVER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTHENTICATED_USER_EMAIL_RETRIEVER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

class GaiaAuthFetcher;

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {

// Helper class that retrieves the authenticated user's e-mail address.
class AuthenticatedUserEmailRetriever : public GaiaAuthConsumer {
 public:
  typedef base::Callback<void(const std::string&)>
      AuthenticatedUserEmailCallback;

  // Retrieves the authenticated user's e-mail address from GAIA and passes it
  // to |callback|. Requires that |url_request_context_getter| contain the GAIA
  // cookies for exactly one user. If the e-mail address cannot be retrieved, an
  // empty string is passed to the |callback|.
  AuthenticatedUserEmailRetriever(
      const AuthenticatedUserEmailCallback& callback,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);
  virtual ~AuthenticatedUserEmailRetriever();

  // GaiaAuthConsumer:
  virtual void OnListAccountsSuccess(const std::string& data) OVERRIDE;
  virtual void OnListAccountsFailure(const GoogleServiceAuthError& error)
      OVERRIDE;

 private:
  const AuthenticatedUserEmailCallback callback_;
  GaiaAuthFetcher gaia_auth_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatedUserEmailRetriever);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTHENTICATED_USER_EMAIL_RETRIEVER_H_
