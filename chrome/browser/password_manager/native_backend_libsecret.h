// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_LIBSECRET_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_LIBSECRET_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "chrome/browser/profiles/profile.h"
#include "components/os_crypt/libsecret_util_posix.h"

namespace autofill {
struct PasswordForm;
}

class NativeBackendLibsecret : public PasswordStoreX::NativeBackend {
 public:
  explicit NativeBackendLibsecret(LocalProfileId id);

  ~NativeBackendLibsecret() override;

  bool Init() override;

  // Implements NativeBackend interface.
  password_manager::PasswordStoreChangeList AddLogin(
      const autofill::PasswordForm& form) override;
  bool UpdateLogin(const autofill::PasswordForm& form,
                   password_manager::PasswordStoreChangeList* changes) override;
  bool RemoveLogin(const autofill::PasswordForm& form,
                   password_manager::PasswordStoreChangeList* changes) override;
  bool RemoveLoginsCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) override;
  bool RemoveLoginsSyncedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) override;
  bool DisableAutoSignInForAllLogins(
      password_manager::PasswordStoreChangeList* changes) override;
  bool GetLogins(const autofill::PasswordForm& form,
                 ScopedVector<autofill::PasswordForm>* forms) override;
  bool GetAutofillableLogins(
      ScopedVector<autofill::PasswordForm>* forms) override;
  bool GetBlacklistLogins(ScopedVector<autofill::PasswordForm>* forms) override;
  bool GetAllLogins(ScopedVector<autofill::PasswordForm>* forms) override;

 private:
  enum TimestampToCompare {
    CREATION_TIMESTAMP,
    SYNC_TIMESTAMP,
  };

  // Returns credentials matching |lookup_form| via |forms|.
  bool AddUpdateLoginSearch(
      const autofill::PasswordForm& lookup_form,
      ScopedVector<autofill::PasswordForm>* forms);

  // Adds a login form without checking for one to replace first.
  bool RawAddLogin(const autofill::PasswordForm& form);

  enum GetLoginsListOptions {
    ALL_LOGINS,
    AUTOFILLABLE_LOGINS,
    BLACKLISTED_LOGINS,
  };

  // Retrieves credentials matching |options| from the keyring into |forms|,
  // overwriting the original contents of |forms|. If |lookup_form| is not NULL,
  // only retrieves credentials PSL-matching it. Returns true on success.
  bool GetLoginsList(const autofill::PasswordForm* lookup_form,
                     GetLoginsListOptions options,
                     ScopedVector<autofill::PasswordForm>* forms)
      WARN_UNUSED_RESULT;

  // Retrieves password created/synced in the time interval into |forms|,
  // overwriting the original contents of |forms|. Returns true on success.
  bool GetLoginsBetween(base::Time get_begin,
                        base::Time get_end,
                        TimestampToCompare date_to_compare,
                        ScopedVector<autofill::PasswordForm>* forms)
      WARN_UNUSED_RESULT;

  // Removes password created/synced in the time interval. Returns |true| if the
  // operation succeeded. |changes| will contain the changes applied.
  bool RemoveLoginsBetween(base::Time get_begin,
                           base::Time get_end,
                           TimestampToCompare date_to_compare,
                           password_manager::PasswordStoreChangeList* changes);

  // convert data get from Libsecret to Passwordform
  ScopedVector<autofill::PasswordForm> ConvertFormList(
      GList* found,
      const autofill::PasswordForm* lookup_form);

  // The app string, possibly based on the local profile id.
  std::string app_string_;

  DISALLOW_COPY_AND_ASSIGN(NativeBackendLibsecret);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_LIBSECRET_H_
