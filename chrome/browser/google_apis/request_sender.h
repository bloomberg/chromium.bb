// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_REQUEST_SENDER_H_
#define CHROME_BROWSER_GOOGLE_APIS_REQUEST_SENDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class Profile;

namespace net {
class URLRequestContextGetter;
}

namespace google_apis {

class AuthenticatedRequestInterface;
class AuthService;

// Helper class that sends requests implementing
// AuthenticatedRequestInterface and handles retries and authentication.
class RequestSender {
 public:
  // |url_request_context_getter| is used to perform authentication with
  // AuthService.
  //
  // |scopes| specifies OAuth2 scopes.
  //
  // |custom_user_agent| will be used for the User-Agent header in HTTP
  // requests issued through the request sender if the value is not empty.
  RequestSender(Profile* profile,
                net::URLRequestContextGetter* url_request_context_getter,
                const std::vector<std::string>& scopes,
                const std::string& custom_user_agent);
  virtual ~RequestSender();

  AuthService* auth_service() { return auth_service_.get(); }

  // Prepares the object for use.
  virtual void Initialize();

  // Starts a request implementing the AuthenticatedRequestInterface
  // interface, and makes the request retry upon authentication failures by
  // calling back to RetryRequest. The |request| object is owned by this
  // RequestSender. It will be deleted in RequestSender's destructor or
  // in RequestFinished().
  //
  // Returns a closure to cancel the request. The closure cancels the request
  // if it is in-flight, and does nothing if it is already terminated.
  base::Closure StartRequestWithRetry(AuthenticatedRequestInterface* request);

  // Notifies to this RequestSender that |request| has finished.
  // TODO(kinaba): refactor the life time management and make this at private.
  void RequestFinished(AuthenticatedRequestInterface* request);

 private:
  // Called when the access token is fetched.
  void OnAccessTokenFetched(
      const base::WeakPtr<AuthenticatedRequestInterface>& request,
      GDataErrorCode error,
      const std::string& access_token);

  // Clears any authentication token and retries the request, which forces
  // an authentication token refresh.
  void RetryRequest(AuthenticatedRequestInterface* request);

  // Cancels the request. Used for implementing the returned closure of
  // StartRequestWithRetry.
  void CancelRequest(
      const base::WeakPtr<AuthenticatedRequestInterface>& request);

  Profile* profile_;  // Not owned.

  scoped_ptr<AuthService> auth_service_;
  std::set<AuthenticatedRequestInterface*> in_flight_requests_;
  const std::string custom_user_agent_;

  base::ThreadChecker thread_checker_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RequestSender> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RequestSender);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_REQUEST_SENDER_H_
