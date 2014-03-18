// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_SUPERVISED_USER_AUTHENTICATION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_SUPERVISED_USER_AUTHENTICATION_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/managed/supervised_user_login_flow.h"

namespace chromeos {

class SupervisedUserManager;

// This is a class that encapsulates all details of password handling for
// supervised users.
// Main property is the schema used to handle password. For now it can be either
// plain password schema, when plain text password is passed to standard
// cryprohome authentication algorithm without modification, or hashed password
// schema, when password is additionally hashed with user-specific salt.
// Second schema is required to allow password syncing across devices for
// supervised users.
class SupervisedUserAuthentication {
 public:
  enum Schema {
    SCHEMA_PLAIN = 1,
    SCHEMA_SALT_HASHED = 2
  };

  explicit SupervisedUserAuthentication(SupervisedUserManager* owner);
  virtual ~SupervisedUserAuthentication();

  // Returns current schema for whole ChromeOS. It defines if users with older
  // schema should be migrated somehow.
  Schema GetStableSchema();

  // Transforms password according to schema specified in Local State.
  std::string TransformPassword(const std::string& supervised_user_id,
                                const std::string& password);

  // Schedules password migration for |user_id| with |password| as a plain text
  // password. Migration should happen during |user_login_flow|.
  void SchedulePasswordMigration(const std::string& user_id,
                                 const std::string& password,
                                 SupervisedUserLoginFlow* user_login_flow);

  // Fills |password_data| with |password|-specific data for |user_id|,
  // depending on target schema. Does not affect Local State.
  bool FillDataForNewUser(const std::string& user_id,
                          const std::string& password,
                          base::DictionaryValue* password_data,
                          base::DictionaryValue* extra_data);

  // Stores |password_data| for |user_id| in Local State. Only public parts
  // of |password_data| will be stored.
  void StorePasswordData(const std::string& user_id,
                         const base::DictionaryValue& password_data);

  bool NeedPasswordChange(const std::string& user_id,
                          const base::DictionaryValue* password_data);

  // Called by manager.
  void ChangeSupervisedUserPassword(const std::string& manager_id,
                                    const std::string& master_key,
                                    const std::string& supervised_user_id,
                                    const base::DictionaryValue* password_data);

  // Creates a random string that can be used as a master key for managed
  // user's homedir.
  std::string GenerateMasterKey();

  // Called by supervised user
  void ScheduleSupervisedPasswordChange(
      const std::string& supervised_user_id,
      const base::DictionaryValue* password_data);

  // Utility method that gets schema version for |user_id| from Local State.
  Schema GetPasswordSchema(const std::string& user_id);

 private:
  SupervisedUserManager* owner_;

  // Controls if migration is enabled.
  bool migration_enabled_;

  // Target schema version. Affects migration process and new user creation.
  Schema stable_schema_;


  DISALLOW_COPY_AND_ASSIGN(SupervisedUserAuthentication);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_SUPERVISED_USER_AUTHENTICATION_H_
