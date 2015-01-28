// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_LIBSECRET_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_LIBSECRET_H_

#include <libsecret/secret.h>

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "chrome/browser/profiles/profile.h"

namespace autofill {
struct PasswordForm;
}

class LibsecretLoader {
 public:
  static decltype(&::secret_password_store_sync) secret_password_store_sync;
  static decltype(&::secret_service_search_sync) secret_service_search_sync;
  static decltype(&::secret_password_clear_sync) secret_password_clear_sync;
  static decltype(&::secret_item_get_secret) secret_item_get_secret;
  static decltype(&::secret_value_get_text) secret_value_get_text;
  static decltype(&::secret_item_get_attributes) secret_item_get_attributes;
  static decltype(&::secret_item_load_secret_sync) secret_item_load_secret_sync;
  static decltype(&::secret_value_unref) secret_value_unref;

 protected:
  static bool LoadLibsecret();
  static bool LibsecretIsAvailable();

  static bool libsecret_loaded;

 private:
  struct FunctionInfo {
    const char* name;
    void** pointer;
  };

  static const FunctionInfo functions[];
};

class NativeBackendLibsecret : public PasswordStoreX::NativeBackend,
                               public LibsecretLoader {
 public:
  explicit NativeBackendLibsecret(LocalProfileId id);

  ~NativeBackendLibsecret() override;

  bool Init() override;

  // Implements NativeBackend interface.
  password_manager::PasswordStoreChangeList AddLogin(
      const autofill::PasswordForm& form) override;
  bool UpdateLogin(const autofill::PasswordForm& form,
                   password_manager::PasswordStoreChangeList* changes) override;
  bool RemoveLogin(const autofill::PasswordForm& form) override;
  bool RemoveLoginsCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) override;
  bool RemoveLoginsSyncedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) override;
  bool GetLogins(const autofill::PasswordForm& form,
                 ScopedVector<autofill::PasswordForm>* forms) override;
  bool GetAutofillableLogins(
      ScopedVector<autofill::PasswordForm>* forms) override;
  bool GetBlacklistLogins(ScopedVector<autofill::PasswordForm>* forms) override;

 private:
  enum TimestampToCompare {
    CREATION_TIMESTAMP,
    SYNC_TIMESTAMP,
  };

  enum AddUpdateLoginSearchOptions {
    SEARCH_USE_SUBMIT,
    SEARCH_IGNORE_SUBMIT,
  };

  // Search that is used in AddLogin and UpdateLogin methods
  void AddUpdateLoginSearch(const autofill::PasswordForm& lookup_form,
                            AddUpdateLoginSearchOptions options,
                            ScopedVector<autofill::PasswordForm>* forms);

  // Adds a login form without checking for one to replace first.
  bool RawAddLogin(const autofill::PasswordForm& form);

  enum GetLoginsListOptions {
    ALL_LOGINS,
    AUTOFILLABLE_LOGINS,
    BLACKLISTED_LOGINS,
  };

  // Reads PasswordForms from the keyring with the given autofillability state.
  bool GetLoginsList(const autofill::PasswordForm* lookup_form,
                     GetLoginsListOptions options,
                     ScopedVector<autofill::PasswordForm>* forms);

  // Helper for GetLoginsCreatedBetween().
  bool GetAllLogins(ScopedVector<autofill::PasswordForm>* forms);

  // Retrieves password created/synced in the time interval. Returns |true| if
  // the operation succeeded.
  bool GetLoginsBetween(base::Time get_begin,
                        base::Time get_end,
                        TimestampToCompare date_to_compare,
                        ScopedVector<autofill::PasswordForm>* forms);

  // Removes password created/synced in the time interval. Returns |true| if the
  // operation succeeded. |changes| will contain the changes applied.
  bool RemoveLoginsBetween(base::Time get_begin,
                           base::Time get_end,
                           TimestampToCompare date_to_compare,
                           password_manager::PasswordStoreChangeList* changes);

  // convert data get from Libsecret to Passwordform
  bool ConvertFormList(GList* found,
                       const autofill::PasswordForm* lookup_form,
                       ScopedVector<autofill::PasswordForm>* forms);

  // Generates a profile-specific app string based on profile_id_.
  static std::string GetProfileSpecificAppString(LocalProfileId id);

  // The app string, possibly based on the local profile id.
  std::string app_string_;

  DISALLOW_COPY_AND_ASSIGN(NativeBackendLibsecret);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_LIBSECRET_H_
