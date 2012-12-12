// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_OPERATION_RUNNER_H_
#define CHROME_BROWSER_GOOGLE_APIS_OPERATION_RUNNER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class Profile;

namespace net {
class URLRequestContextGetter;
}

namespace google_apis {

class AuthenticatedOperationInterface;
class AuthService;
class OperationRegistry;

// Helper class that runs AuthenticatedOperationInterface objects, handling
// retries and authentication.
class OperationRunner {
 public:
  // |url_request_context_getter| is used to perform authentication with
  // AuthService.
  //
  // |scopes| specifies OAuth2 scopes.
  //
  // |custom_user_agent| will be used for the User-Agent header in HTTP
  // requests issued through the operation runner if the value is not empty.
  OperationRunner(Profile* profile,
                  net::URLRequestContextGetter* url_request_context_getter,
                  const std::vector<std::string>& scopes,
                  const std::string& custom_user_agent);
  virtual ~OperationRunner();

  AuthService* auth_service() { return auth_service_.get(); }
  OperationRegistry* operation_registry() {
    return operation_registry_.get();
  }

  // Prepares the object for use.
  virtual void Initialize();

  // Cancels all in-flight operations.
  void CancelAll();

  // Starts an operation implementing the AuthenticatedOperationInterface
  // interface, and makes the operation retry upon authentication failures by
  // calling back to RetryOperation.
  void StartOperationWithRetry(AuthenticatedOperationInterface* operation);

 private:
  // Called when the access token is fetched.
  void OnAccessTokenFetched(
      const base::WeakPtr<AuthenticatedOperationInterface>& operation,
      GDataErrorCode error,
      const std::string& access_token);

  // Clears any authentication token and retries the operation, which forces
  // an authentication token refresh.
  void RetryOperation(AuthenticatedOperationInterface* operation);

  Profile* profile_;  // Not owned.

  scoped_ptr<AuthService> auth_service_;
  scoped_ptr<OperationRegistry> operation_registry_;
  const std::string custom_user_agent_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<OperationRunner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OperationRunner);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_OPERATION_RUNNER_H_
