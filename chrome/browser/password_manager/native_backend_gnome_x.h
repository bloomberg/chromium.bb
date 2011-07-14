// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_GNOME_X_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_GNOME_X_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "chrome/browser/profiles/profile.h"

class PrefService;

namespace webkit_glue {
struct PasswordForm;
}

// NativeBackend implementation using GNOME Keyring.
class NativeBackendGnome : public PasswordStoreX::NativeBackend {
 public:
  NativeBackendGnome(LocalProfileId id, PrefService* prefs);

  virtual ~NativeBackendGnome();

  virtual bool Init();

  // Implements NativeBackend interface.
  virtual bool AddLogin(const webkit_glue::PasswordForm& form);
  virtual bool UpdateLogin(const webkit_glue::PasswordForm& form);
  virtual bool RemoveLogin(const webkit_glue::PasswordForm& form);
  virtual bool RemoveLoginsCreatedBetween(const base::Time& delete_begin,
                                          const base::Time& delete_end);
  virtual bool GetLogins(const webkit_glue::PasswordForm& form,
                         PasswordFormList* forms);
  virtual bool GetLoginsCreatedBetween(const base::Time& get_begin,
                                       const base::Time& get_end,
                                       PasswordFormList* forms);
  virtual bool GetAutofillableLogins(PasswordFormList* forms);
  virtual bool GetBlacklistLogins(PasswordFormList* forms);

 private:
  // Adds a login form without checking for one to replace first.
  bool RawAddLogin(const webkit_glue::PasswordForm& form);

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
