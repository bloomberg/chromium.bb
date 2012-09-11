// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_OPERATION_RUNNER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_OPERATION_RUNNER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/operations_base.h"

class Profile;

namespace gdata {

class AuthenticatedOperationInterface;
class AuthService;
class OperationRegistry;

// Helper class that runs AuthenticatedOperationInterface objects, handling
// retries and authentication.
class OperationRunner {
 public:
  OperationRunner(Profile* profile, const std::vector<std::string>& scopes);
  virtual ~OperationRunner();

  AuthService* auth_service() { return auth_service_.get(); }
  OperationRegistry* operation_registry() {
    return operation_registry_.get();
  }

  // Prepares the object for use.
  virtual void Initialize();

  // Cancels all in-flight operations.
  void CancelAll();

  // Authenticates the user by fetching the auth token as needed. |callback|
  // will be run with the error code and the auth token, on the thread this
  // function is run.
  void Authenticate(const AuthStatusCallback& callback);

  // Starts an operation implementing the AuthenticatedOperationInterface
  // interface, and makes the operation retry upon authentication failures by
  // calling back to RetryOperation.
  void StartOperationWithRetry(AuthenticatedOperationInterface* operation);

  // Starts an operation implementing the AuthenticatedOperationInterface
  // interface.
  void StartOperation(AuthenticatedOperationInterface* operation);

  // Called when the authentication token is refreshed.
  void OnOperationAuthRefresh(AuthenticatedOperationInterface* operation,
                              GDataErrorCode error,
                              const std::string& auth_token);

  // Clears any authentication token and retries the operation, which
  // forces an authentication token refresh.
  void RetryOperation(AuthenticatedOperationInterface* operation);

 private:
  Profile* profile_;  // not owned

  scoped_ptr<AuthService> auth_service_;
  scoped_ptr<OperationRegistry> operation_registry_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<OperationRunner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OperationRunner);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_OPERATION_RUNNER_H_
