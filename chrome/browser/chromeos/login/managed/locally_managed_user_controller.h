// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CONTROLLER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/login/managed/cloud_connector.h"
#include "chrome/browser/chromeos/login/managed/managed_user_authenticator.h"

namespace chromeos {

// LocallyManagedUserController is used to locally managed user creation.
// LocallyManagedUserController maintains it's own life cycle and deletes itself
// when user creation is finished or aborted.
class LocallyManagedUserController
    : public ManagedUserAuthenticator::StatusConsumer,
      public CloudConnector::Delegate {
 public:
  enum ErrorCode {
    NO_ERROR,
    CRYPTOHOME_NO_MOUNT,
    CRYPTOHOME_FAILED_MOUNT,
    CRYPTOHOME_FAILED_TPM,
    CLOUD_NOT_CONNECTED,
    CLOUD_TIMED_OUT,
    CLOUD_SERVER_ERROR,
  };

  class StatusConsumer {
   public:
    virtual ~StatusConsumer();

    virtual void OnCreationError(ErrorCode code, bool recoverable) = 0;
    virtual void OnCreationSuccess() = 0;
  };

  // All UI initialization is deferred till Init() call.
  // |Consumer| is not owned by controller, and it is expected that it wouldn't
  // be deleted before LocallyManagedUserController.
  explicit LocallyManagedUserController(StatusConsumer* consumer);
  virtual ~LocallyManagedUserController();

  // Returns the current locally managed user controller if it has been created.
  static LocallyManagedUserController* current_controller() {
    return current_controller_;
  }

  void SetUpCreation(string16 display_name, std::string password);
  void StartCreation();
  void RetryLastStep();
  void FinishCreation();

 private:
  // Contains information necessary for new user creation.
  struct UserCreationContext {
    UserCreationContext();
    ~UserCreationContext();

    string16 display_name;
    bool id_acquired;
    std::string user_id;
    std::string password;
    std::string mount_hash;
    bool token_acquired;
    std::string token;
  };

  // CloudConnector::Delegate overrides.
  virtual void NewUserIdGenerated(std::string& new_id) OVERRIDE;
  virtual void DMTokenFetched(std::string& user_id, std::string& token)
      OVERRIDE;
  virtual void OnCloudError(CloudConnector::CloudError error) OVERRIDE;

  // ManagedUserAuthenticator::StatusConsumer overrides.
  virtual void OnAuthenticationFailure(
      ManagedUserAuthenticator::AuthState error) OVERRIDE;
  virtual void OnMountSuccess(const std::string& mount_hash) OVERRIDE;
  virtual void OnCreationSuccess() OVERRIDE;

  // Stores data files in locally managed user home directory.
  // It is called on one of BlockingPool threads.
  virtual void StoreManagedUserFiles(const base::FilePath& base_path);

  // Completion callback for StoreManagedUserFiles method.
  // Called on the UI thread.
  virtual void OnManagedUserFilesStored();

  // Pointer to the current instance of the controller to be used by
  // automation tests.
  static LocallyManagedUserController* current_controller_;

  StatusConsumer* consumer_;

  // Cloud connector for this controller.
  scoped_ptr<CloudConnector> connector_;
  scoped_refptr<ManagedUserAuthenticator> authenticator_;

  // Creation context. Not null while creating new LMU.
  scoped_ptr<UserCreationContext> creation_context_;

  // Factory of callbacks.
  base::WeakPtrFactory<LocallyManagedUserController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocallyManagedUserController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CONTROLLER_H_
