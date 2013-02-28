// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_CLOUD_CONNECTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_CLOUD_CONNECTOR_H_

#include <string>

#include "base/basictypes.h"

namespace chromeos {

// Class that interacts with cloud server for locally managed users.
class CloudConnector {
 public:
  enum CloudError {
    NOT_CONNECTED,
    TIMED_OUT,
    SERVER_ERROR,
  };

  class Delegate {
   public:
    // Called when new id for locally managed user was succesfully generated.
    virtual void NewUserIdGenerated(std::string new_id) = 0;
    // Called when some error happened while interacting with cloud server.
    virtual void OnCloudError(CloudError error) = 0;
   protected:
    virtual ~Delegate();
  };

  // Creates CloudConnector with specific delegate for callbacks.
  explicit CloudConnector(Delegate* delegate);
  virtual ~CloudConnector();

  // Request new user id generation.
  void GenerateNewUserId();

 private:
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CloudConnector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_CLOUD_CONNECTOR_H_
