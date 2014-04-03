// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_MANAGED_USER_CREATION_CONTROLLER_OLD_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_MANAGED_USER_CREATION_CONTROLLER_OLD_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/managed/managed_user_authenticator.h"
#include "chrome/browser/chromeos/login/managed/managed_user_creation_controller.h"
#include "chrome/browser/managed_mode/managed_user_registration_utility.h"

class Profile;

namespace chromeos {

// LMU Creation process:
// 0. Manager is logged in
// 1. Generate ID for new LMU
// 2. Start "transaction" in Local State.
// 3. Create local cryptohome (errors could arise)
// 4. Create user in cloud (errors could arise)
// 5. Store cloud token in cryptohome (actually, error could arise).
// 6. Mark "transaction" as completed.
// 7. End manager session.
class ManagedUserCreationControllerOld
    : public ManagedUserCreationController,
      ManagedUserAuthenticator::AuthStatusConsumer {
 public:
  // All UI initialization is deferred till Init() call.
  // |Consumer| is not owned by controller, and it is expected that it wouldn't
  // be deleted before ManagedUserCreationControllerOld.
  ManagedUserCreationControllerOld(StatusConsumer* consumer,
                                   const std::string& manager_id);

  virtual ~ManagedUserCreationControllerOld();

  // Set up controller for creating new supervised user with |display_name|,
  // |password| and avatar indexed by |avatar_index|. StartCreation() have to
  // be called to actually start creating user.
  virtual void StartCreation(const base::string16& display_name,
                             const std::string& password,
                             int avatar_index) OVERRIDE;

  // Configures and initiates importing existing supervised user to this device.
  // Existing user is identified by |sync_id|, has |display_name|, |password|,
  // |avatar_index|. The master key for cryptohome is a |master_key|.
  virtual void StartImport(const base::string16& display_name,
                           const std::string& password,
                           int avatar_index,
                           const std::string& sync_id,
                           const std::string& master_key) OVERRIDE;

  // Not implemented in this class.
  virtual void StartImport(const base::string16& display_name,
                           int avatar_index,
                           const std::string& sync_id,
                           const std::string& master_key,
                           const base::DictionaryValue* password_data,
                           const std::string& encryption_key,
                           const std::string& signature_key) OVERRIDE;

  virtual void SetManagerProfile(Profile* manager_profile) OVERRIDE;
  virtual Profile* GetManagerProfile() OVERRIDE;

  virtual void CancelCreation() OVERRIDE;
  virtual void FinishCreation() OVERRIDE;
  virtual std::string GetManagedUserId() OVERRIDE;

 private:
  // Indicates if we create new user, or import an existing one.
  enum CreationType { NEW_USER, USER_IMPORT, };

  // Contains information necessary for new user creation.
  struct UserCreationContext {
    UserCreationContext();
    ~UserCreationContext();

    base::string16 display_name;
    int avatar_index;
    std::string manager_id;
    std::string local_user_id;  // Used to identify cryptohome.
    std::string sync_user_id;   // Used to identify user in manager's sync data.
    std::string password;
    std::string mount_hash;
    std::string master_key;
    bool token_acquired;
    std::string token;
    bool token_succesfully_written;
    CreationType creation_type;
    base::DictionaryValue password_data;
    Profile* manager_profile;
    scoped_ptr<ManagedUserRegistrationUtility> registration_utility;
  };

  void StartCreation();

  // ManagedUserAuthenticator::StatusConsumer overrides.
  virtual void OnAuthenticationFailure(
      ManagedUserAuthenticator::AuthState error) OVERRIDE;
  virtual void OnMountSuccess(const std::string& mount_hash) OVERRIDE;
  virtual void OnAddKeySuccess() OVERRIDE;

  void CreationTimedOut();
  void RegistrationCallback(const GoogleServiceAuthError& error,
                            const std::string& token);

  void TokenFetched(const std::string& token);

  // Completion callback for StoreManagedUserFiles method.
  // Called on the UI thread.
  void OnManagedUserFilesStored(bool success);

  scoped_refptr<ManagedUserAuthenticator> authenticator_;

  // Creation context. Not null while creating new LMU.
  scoped_ptr<UserCreationContext> creation_context_;

  // Timer for showing warning if creation process takes too long.
  base::OneShotTimer<ManagedUserCreationControllerOld> timeout_timer_;

  // Factory of callbacks.
  base::WeakPtrFactory<ManagedUserCreationControllerOld> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserCreationControllerOld);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_MANAGED_USER_CREATION_CONTROLLER_OLD_H_
