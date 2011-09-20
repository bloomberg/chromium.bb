// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_KWALLET_X_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_KWALLET_X_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "chrome/browser/profiles/profile.h"

class Pickle;
class PrefService;

namespace webkit_glue {
struct PasswordForm;
}

namespace base {
class WaitableEvent;
}

namespace dbus {
class Bus;
class ObjectProxy;
}

// NativeBackend implementation using KWallet.
class NativeBackendKWallet : public PasswordStoreX::NativeBackend {
 public:
  NativeBackendKWallet(LocalProfileId id, PrefService* prefs);

  virtual ~NativeBackendKWallet();

  virtual bool Init() OVERRIDE;

  // Implements NativeBackend interface.
  virtual bool AddLogin(const webkit_glue::PasswordForm& form) OVERRIDE;
  virtual bool UpdateLogin(const webkit_glue::PasswordForm& form) OVERRIDE;
  virtual bool RemoveLogin(const webkit_glue::PasswordForm& form) OVERRIDE;
  virtual bool RemoveLoginsCreatedBetween(
      const base::Time& delete_begin, const base::Time& delete_end) OVERRIDE;
  virtual bool GetLogins(const webkit_glue::PasswordForm& form,
                         PasswordFormList* forms) OVERRIDE;
  virtual bool GetLoginsCreatedBetween(const base::Time& get_begin,
                                       const base::Time& get_end,
                                       PasswordFormList* forms) OVERRIDE;
  virtual bool GetAutofillableLogins(PasswordFormList* forms) OVERRIDE;
  virtual bool GetBlacklistLogins(PasswordFormList* forms) OVERRIDE;

 protected:
  // Invalid handle returned by WalletHandle().
  static const int kInvalidKWalletHandle = -1;

  // Internally used by Init(), but also for testing to provide a mock bus.
  bool InitWithBus(scoped_refptr<dbus::Bus> optional_bus);

  // Deserializes a list of PasswordForms from the wallet.
  static void DeserializeValue(const std::string& signon_realm,
                               const Pickle& pickle,
                               PasswordFormList* forms);

 private:
  enum InitResult {
    INIT_SUCCESS,    // Init succeeded.
    TEMPORARY_FAIL,  // Init failed, but might succeed after StartKWalletd().
    PERMANENT_FAIL   // Init failed, and is not likely to work later either.
  };

  // Initialization.
  bool StartKWalletd();
  InitResult InitWallet();
  void InitOnDBThread(scoped_refptr<dbus::Bus> optional_bus,
                      base::WaitableEvent* event,
                      bool* success);

  // Reads PasswordForms from the wallet that match the given signon_realm.
  bool GetLoginsList(PasswordFormList* forms,
                     const std::string& signon_realm,
                     int wallet_handle);

  // Reads PasswordForms from the wallet with the given autofillability state.
  bool GetLoginsList(PasswordFormList* forms,
                     bool autofillable,
                     int wallet_handle);

  // Reads PasswordForms from the wallet created in the given time range.
  bool GetLoginsList(PasswordFormList* forms,
                     const base::Time& begin,
                     const base::Time& end,
                     int wallet_handle);

  // Helper for some of the above GetLoginsList() methods.
  bool GetAllLogins(PasswordFormList* forms, int wallet_handle);

  // Writes a list of PasswordForms to the wallet with the given signon_realm.
  // Overwrites any existing list for this signon_realm. Removes the entry if
  // |forms| is empty. Returns true on success.
  bool SetLoginsList(const PasswordFormList& forms,
                     const std::string& signon_realm,
                     int wallet_handle);

  // Opens the wallet and ensures that the "Chrome Form Data" folder exists.
  // Returns kInvalidWalletHandle on error.
  int WalletHandle();

  // Compares two PasswordForms and returns true if they are the same.
  // If |update_check| is false, we only check the fields that are checked by
  // LoginDatabase::UpdateLogin() when updating logins; otherwise, we check the
  // fields that are checked by LoginDatabase::RemoveLogin() for removing them.
  static bool CompareForms(const webkit_glue::PasswordForm& a,
                           const webkit_glue::PasswordForm& b,
                           bool update_check);

  // Serializes a list of PasswordForms to be stored in the wallet.
  static void SerializeValue(const PasswordFormList& forms, Pickle* pickle);

  // Checks a serialized list of PasswordForms for sanity. Returns true if OK.
  // Note that |realm| is only used for generating a useful warning message.
  static bool CheckSerializedValue(const uint8_t* byte_array, size_t length,
                                   const std::string& realm);

  // Convenience function to read a GURL from a Pickle. Assumes the URL has
  // been written as a std::string. Returns true on success.
  static bool ReadGURL(const Pickle& pickle, void** iter, GURL* url);

  // In case the fields in the pickle ever change, version them so we can try to
  // read old pickles. (Note: do not eat old pickles past the expiration date.)
  static const int kPickleVersion = 0;

  // Name of the folder to store passwords in.
  static const char kKWalletFolder[];

  // DBus service, path, and interface names for klauncher and kwalletd.
  static const char kKWalletServiceName[];
  static const char kKWalletPath[];
  static const char kKWalletInterface[];
  static const char kKLauncherServiceName[];
  static const char kKLauncherPath[];
  static const char kKLauncherInterface[];

  // Generates a profile-specific folder name based on profile_id_.
  std::string GetProfileSpecificFolderName() const;

  // Migrates non-profile-specific logins to be profile-specific.
  void MigrateToProfileSpecificLogins();

  // The local profile id, used to generate the folder name.
  const LocalProfileId profile_id_;

  // The pref service to use for persistent migration settings.
  PrefService* prefs_;

  // The KWallet folder name, possibly based on the local profile id.
  std::string folder_name_;

  // True once MigrateToProfileSpecificLogins() has been attempted.
  bool migrate_tried_;

  // DBus handle for communication with klauncher and kwalletd.
  scoped_refptr<dbus::Bus> session_bus_;
  // Object proxy for kwalletd. We do not own this.
  dbus::ObjectProxy* kwallet_proxy_;

  // The name of the wallet we've opened. Set during Init().
  std::string wallet_name_;
  // The application name (e.g. "Chromium"), shown in KWallet auth dialogs.
  const std::string app_name_;

  DISALLOW_COPY_AND_ASSIGN(NativeBackendKWallet);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_KWALLET_X_H_
