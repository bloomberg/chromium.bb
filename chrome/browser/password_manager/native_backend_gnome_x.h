// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_GNOME_X_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_GNOME_X_H_

// libgnome-keyring has been deprecated in favor of libsecret.
// See: https://mail.gnome.org/archives/commits-list/2013-October/msg08876.html
//
// The define below turns off the deprecations, in order to avoid build
// failures with Gnome 3.12. When we move to libsecret, the define can be
// removed, together with the include below it.
//
// The porting is tracked in http://crbug.com/355223
#define GNOME_KEYRING_DEPRECATED
#define GNOME_KEYRING_DEPRECATED_FOR(x)
#include <gnome-keyring.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "chrome/browser/profiles/profile.h"

namespace autofill {
struct PasswordForm;
}

// Many of the gnome_keyring_* functions use variable arguments, which makes
// them difficult if not impossible to truly wrap in C. Therefore, we use
// appropriately-typed function pointers and scoping to make the fact that we
// might be dynamically loading the library almost invisible. As a bonus, we
// also get a simple way to mock the library for testing. Classes that inherit
// from GnomeKeyringLoader will use its versions of the gnome_keyring_*
// functions. Note that it has only static fields.
class GnomeKeyringLoader {
 protected:
  static bool LoadGnomeKeyring();

  // Declare the actual function pointers that we'll use in client code.
  static decltype(&::gnome_keyring_is_available) gnome_keyring_is_available_ptr;
  static decltype(
      &::gnome_keyring_store_password) gnome_keyring_store_password_ptr;
  static decltype(
      &::gnome_keyring_delete_password) gnome_keyring_delete_password_ptr;
  static decltype(&::gnome_keyring_find_items) gnome_keyring_find_items_ptr;
  static decltype(
      &::gnome_keyring_result_to_message) gnome_keyring_result_to_message_ptr;
  static decltype(&::gnome_keyring_attribute_list_free)
      gnome_keyring_attribute_list_free_ptr;
  static decltype(
      &::gnome_keyring_attribute_list_new) gnome_keyring_attribute_list_new_ptr;
  static decltype(&::gnome_keyring_attribute_list_append_string)
      gnome_keyring_attribute_list_append_string_ptr;
  static decltype(&::gnome_keyring_attribute_list_append_uint32)
      gnome_keyring_attribute_list_append_uint32_ptr;
  // We also use gnome_keyring_attribute_list_index(), which is a macro and
  // can't be referenced.

  // Set to true if LoadGnomeKeyring() has already succeeded.
  static bool keyring_loaded;

 private:
  struct FunctionInfo {
    const char* name;
    void** pointer;
  };

  // Make it easy to initialize the function pointers in LoadGnomeKeyring().
  static const FunctionInfo functions[];
};

// NativeBackend implementation using GNOME Keyring.
class NativeBackendGnome : public PasswordStoreX::NativeBackend,
                           public GnomeKeyringLoader {
 public:
  explicit NativeBackendGnome(LocalProfileId id);

  ~NativeBackendGnome() override;

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
  bool DisableAutoSignInForOrigins(
      const base::Callback<bool(const GURL&)>& origin_filter,
      password_manager::PasswordStoreChangeList* changes) override;
  bool GetLogins(const password_manager::PasswordStore::FormDigest& form,
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

  // Adds a login form without checking for one to replace first.
  bool RawAddLogin(const autofill::PasswordForm& form);

  // Retrieves all autofillable or all blacklisted credentials (depending on
  // |autofillable|) from the keyring into |forms|, overwriting the original
  // contents of |forms|. Returns true on success.
  bool GetLoginsList(bool autofillable,
                     ScopedVector<autofill::PasswordForm>* forms)
      WARN_UNUSED_RESULT;

  // Retrieves password created/synced in the time interval. Returns |true| if
  // the operation succeeded.
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

  // The app string, possibly based on the local profile id.
  std::string app_string_;

  DISALLOW_COPY_AND_ASSIGN(NativeBackendGnome);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_GNOME_X_H_
