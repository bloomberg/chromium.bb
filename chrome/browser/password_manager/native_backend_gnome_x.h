// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_GNOME_X_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_GNOME_X_H_

#include <string>

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "chrome/browser/profiles/profile.h"

#include "library_loaders/libgnome-keyring.h"

class PrefService;

namespace content {
struct PasswordForm;
}

// NativeBackend implementation using GNOME Keyring.
class NativeBackendGnome : public PasswordStoreX::NativeBackend {
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

#ifdef UNIT_TEST
  LibGnomeKeyringLoader* libgnome_keyring_loader() {
    return &libgnome_keyring_loader_;
  }
#endif  // UNIT_TEST

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

  LibGnomeKeyringLoader libgnome_keyring_loader_;

  DISALLOW_COPY_AND_ASSIGN(NativeBackendGnome);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_GNOME_X_H_
