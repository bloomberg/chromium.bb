// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_GNOME_X_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_GNOME_X_H_

#include <gnome-keyring.h>

#include <string>

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "chrome/browser/profiles/profile.h"

class PrefService;

namespace content {
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
#define GNOME_KEYRING_FOR_EACH_FUNC(F)          \
  F(is_available)                               \
  F(store_password)                             \
  F(delete_password)                            \
  F(find_itemsv)                                \
  F(result_to_message)

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
  NativeBackendGnome(LocalProfileId id, PrefService* prefs);

  virtual ~NativeBackendGnome();

  virtual bool Init() OVERRIDE;

  // Implements NativeBackend interface.
  virtual bool AddLogin(const content::PasswordForm& form) OVERRIDE;
  virtual bool UpdateLogin(const content::PasswordForm& form) OVERRIDE;
  virtual bool RemoveLogin(const content::PasswordForm& form) OVERRIDE;
  virtual bool RemoveLoginsCreatedBetween(
      const base::Time& delete_begin, const base::Time& delete_end) OVERRIDE;
  virtual bool GetLogins(const content::PasswordForm& form,
                         PasswordFormList* forms) OVERRIDE;
  virtual bool GetLoginsCreatedBetween(const base::Time& get_begin,
                                       const base::Time& get_end,
                                       PasswordFormList* forms) OVERRIDE;
  virtual bool GetAutofillableLogins(PasswordFormList* forms) OVERRIDE;
  virtual bool GetBlacklistLogins(PasswordFormList* forms) OVERRIDE;

 private:
  // Adds a login form without checking for one to replace first.
  bool RawAddLogin(const content::PasswordForm& form);

  // Reads PasswordForms from the keyring with the given autofillability state.
  bool GetLoginsList(PasswordFormList* forms, bool autofillable);

  // Helper for GetLoginsCreatedBetween().
  bool GetAllLogins(PasswordFormList* forms);

  // Generates a profile-specific app string based on profile_id_.
  std::string GetProfileSpecificAppString() const;

  // Migrates non-profile-specific logins to be profile-specific.
  void MigrateToProfileSpecificLogins();

  // The local profile id, used to generate the app string.
  const LocalProfileId profile_id_;

  // The pref service to use for persistent migration settings.
  PrefService* prefs_;

  // The app string, possibly based on the local profile id.
  std::string app_string_;

  // True once MigrateToProfileSpecificLogins() has been attempted.
  bool migrate_tried_;

  DISALLOW_COPY_AND_ASSIGN(NativeBackendGnome);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_GNOME_X_H_
