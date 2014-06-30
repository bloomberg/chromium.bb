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

#include "base/basictypes.h"
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

// Call a given parameter with the name of each function we use from GNOME
// Keyring. Make sure to adjust the unit test if you change these.
// The list of functions is divided into those we plan to mock in the unittest,
// and those which we use without mocking in the test.
#define GNOME_KEYRING_FOR_EACH_MOCKED_FUNC(F)      \
  F(is_available)                                  \
  F(store_password)                                \
  F(delete_password)                               \
  F(find_items)                                    \
  F(result_to_message)
#define GNOME_KEYRING_FOR_EACH_NON_MOCKED_FUNC(F)  \
  F(attribute_list_free)                           \
  F(attribute_list_new)                            \
  F(attribute_list_append_string)                  \
  F(attribute_list_append_uint32)
#define GNOME_KEYRING_FOR_EACH_FUNC(F)             \
  GNOME_KEYRING_FOR_EACH_NON_MOCKED_FUNC(F)        \
  GNOME_KEYRING_FOR_EACH_MOCKED_FUNC(F)

// Declare the actual function pointers that we'll use in client code.
#define GNOME_KEYRING_DECLARE_POINTER(name) \
    static typeof(&::gnome_keyring_##name) gnome_keyring_##name;
  GNOME_KEYRING_FOR_EACH_FUNC(GNOME_KEYRING_DECLARE_POINTER)
#undef GNOME_KEYRING_DECLARE_POINTER

  // Set to true if LoadGnomeKeyring() has already succeeded.
  static bool keyring_loaded;

 private:
#if defined(DLOPEN_GNOME_KEYRING)
  struct FunctionInfo {
    const char* name;
    void** pointer;
  };

  // Make it easy to initialize the function pointers in LoadGnomeKeyring().
  static const FunctionInfo functions[];
#endif  // defined(DLOPEN_GNOME_KEYRING)
};

// NativeBackend implementation using GNOME Keyring.
class NativeBackendGnome : public PasswordStoreX::NativeBackend,
                           public GnomeKeyringLoader {
 public:
  explicit NativeBackendGnome(LocalProfileId id);

  virtual ~NativeBackendGnome();

  virtual bool Init() OVERRIDE;

  // Implements NativeBackend interface.
  virtual password_manager::PasswordStoreChangeList AddLogin(
      const autofill::PasswordForm& form) OVERRIDE;
  virtual bool UpdateLogin(
      const autofill::PasswordForm& form,
      password_manager::PasswordStoreChangeList* changes) OVERRIDE;
  virtual bool RemoveLogin(const autofill::PasswordForm& form) OVERRIDE;
  virtual bool RemoveLoginsCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) OVERRIDE;
  virtual bool RemoveLoginsSyncedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) OVERRIDE;
  virtual bool GetLogins(const autofill::PasswordForm& form,
                         PasswordFormList* forms) OVERRIDE;
  virtual bool GetAutofillableLogins(PasswordFormList* forms) OVERRIDE;
  virtual bool GetBlacklistLogins(PasswordFormList* forms) OVERRIDE;

 private:
  enum TimestampToCompare {
    CREATION_TIMESTAMP,
    SYNC_TIMESTAMP,
  };

  // Adds a login form without checking for one to replace first.
  bool RawAddLogin(const autofill::PasswordForm& form);

  // Reads PasswordForms from the keyring with the given autofillability state.
  bool GetLoginsList(PasswordFormList* forms, bool autofillable);

  // Helper for GetLoginsCreatedBetween().
  bool GetAllLogins(PasswordFormList* forms);

  // Retrieves password created/synced in the time interval. Returns |true| if
  // the operation succeeded.
  bool GetLoginsBetween(base::Time get_begin,
                        base::Time get_end,
                        TimestampToCompare date_to_compare,
                        PasswordFormList* forms);

  // Removes password created/synced in the time interval. Returns |true| if the
  // operation succeeded. |changes| will contain the changes applied.
  bool RemoveLoginsBetween(base::Time get_begin,
                           base::Time get_end,
                           TimestampToCompare date_to_compare,
                           password_manager::PasswordStoreChangeList* changes);

  // Generates a profile-specific app string based on profile_id_.
  std::string GetProfileSpecificAppString() const;

  // The local profile id, used to generate the app string.
  const LocalProfileId profile_id_;

  // The app string, possibly based on the local profile id.
  std::string app_string_;

  DISALLOW_COPY_AND_ASSIGN(NativeBackendGnome);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_GNOME_X_H_
