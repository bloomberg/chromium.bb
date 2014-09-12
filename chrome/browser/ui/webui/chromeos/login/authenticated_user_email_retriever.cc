// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/authenticated_user_email_retriever.h"

#include <utility>
#include <vector>

#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

AuthenticatedUserEmailRetriever::AuthenticatedUserEmailRetriever(
    const AuthenticatedUserEmailCallback& callback,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : callback_(callback),
      gaia_auth_fetcher_(this,
                         GaiaConstants::kChromeSource,
                         url_request_context_getter.get()) {
  gaia_auth_fetcher_.StartListAccounts();
}

AuthenticatedUserEmailRetriever::~AuthenticatedUserEmailRetriever() {
}

void AuthenticatedUserEmailRetriever::OnListAccountsSuccess(
    const std::string& data) {
  std::vector<std::pair<std::string, bool> > accounts;
  gaia::ParseListAccountsData(data, &accounts);
  if (accounts.size() != 1)
    callback_.Run(std::string());
  else
    callback_.Run(accounts.front().first);
}

void AuthenticatedUserEmailRetriever::OnListAccountsFailure(
    const GoogleServiceAuthError& error) {
  callback_.Run(std::string());
}

}  // namespace chromeos
