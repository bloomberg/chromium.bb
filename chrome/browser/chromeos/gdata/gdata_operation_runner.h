// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATION_RUNNER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATION_RUNNER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/gdata/gdata_auth_service.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "chrome/browser/chromeos/gdata/gdata_params.h"

class Profile;

namespace gdata {

class GDataOperationInterface;
class GDataOperationRegistry;

// Helper class that runs GDataOperationInterface objects, handling retries and
// authentication.
class GDataOperationRunner : public GDataAuthService::Observer {
 public:
  explicit GDataOperationRunner(Profile* profile);
  virtual ~GDataOperationRunner();

  GDataAuthService* auth_service() { return auth_service_.get(); }
  GDataOperationRegistry* operation_registry() {
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

  // Starts an operation implementing the GDataOperationInterface interface, and
  // makes the operation retry upon authentication failures by calling back to
  // RetryOperation.
  void StartOperationWithRetry(GDataOperationInterface* operation);

  // Starts an operation implementing the GDataOperationInterface interface.
  void StartOperation(GDataOperationInterface* operation);

  // Called when the authentication token is refreshed.
  void OnOperationAuthRefresh(GDataOperationInterface* operation,
                              GDataErrorCode error,
                              const std::string& auth_token);

  // Clears any authentication token and retries the operation, which
  // forces an authentication token refresh.
  void RetryOperation(GDataOperationInterface* operation);

 private:
  // GDataAuthService::Observer override.
  virtual void OnOAuth2RefreshTokenChanged() OVERRIDE;

  Profile* profile_;  // not owned

  scoped_ptr<GDataAuthService> auth_service_;
  scoped_ptr<GDataOperationRegistry> operation_registry_;
  base::WeakPtrFactory<GDataOperationRunner> weak_ptr_factory_;
  base::WeakPtr<GDataOperationRunner> weak_ptr_bound_to_ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(GDataOperationRunner);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATION_RUNNER_H_
