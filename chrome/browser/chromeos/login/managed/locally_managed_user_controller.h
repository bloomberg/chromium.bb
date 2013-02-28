// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CONTROLLER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/login/managed/cloud_connector.h"

namespace chromeos {

// LocallyManagedUserController is used to locally managed user creation.
// LocallyManagedUserController maintains it's own life cycle and deletes itself
// when user creation is finished or aborted.
class LocallyManagedUserController : public CloudConnector::Delegate {
 public:
  // All UI initialization is deferred till Init() call.
  LocallyManagedUserController();
  virtual ~LocallyManagedUserController();

  // Returns the current locally managed user controller if it has been created.
  static LocallyManagedUserController* current_controller() {
    return current_controller_;
  }

  void StartCreation(string16 display_name, std::string password);
  void FinishCreation();

 private:
  // Contains information necessary for new user creation.
  struct UserCreationContext {
    string16 display_name;
  };

  // CloudConnector::Delegate overrides
  virtual void NewUserIdGenerated(std::string new_id) OVERRIDE;
  virtual void OnCloudError(CloudConnector::CloudError error) OVERRIDE;

  // Pointer to the current instance of the controller to be used by
  // automation tests.
  static LocallyManagedUserController* current_controller_;

  // Cloud connector for this controller.
  scoped_ptr<CloudConnector> connector_;

  // Creation context. Not null while creating new LMU.
  scoped_ptr<UserCreationContext> creation_context_;

  DISALLOW_COPY_AND_ASSIGN(LocallyManagedUserController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CONTROLLER_H_
