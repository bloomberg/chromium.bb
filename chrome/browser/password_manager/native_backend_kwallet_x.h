// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_KWALLET_X_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_KWALLET_X_H_
#pragma once

#include <dbus/dbus-glib.h>
#include <glib.h>

#include <string>

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "webkit/glue/password_form.h"

class Pickle;

// NativeBackend implementation using KWallet.
class NativeBackendKWallet : public PasswordStoreX::NativeBackend {
 public:
  NativeBackendKWallet();

  virtual ~NativeBackendKWallet();

  virtual bool Init();

  // Implements NativeBackend interface.
  virtual bool AddLogin(const webkit_glue::PasswordForm& form);
  virtual bool UpdateLogin(const webkit_glue::PasswordForm& form);
  virtual bool RemoveLogin(const webkit_glue::PasswordForm& form);
  virtual bool RemoveLoginsCreatedBetween(const base::Time& delete_begin,
                                          const base::Time& delete_end);
  virtual bool GetLogins(const webkit_glue::PasswordForm& form,
                         PasswordFormList* forms);
  virtual bool GetLoginsCreatedBetween(const base::Time& delete_begin,
                                       const base::Time& delete_end,
                                       PasswordFormList* forms);
  virtual bool GetAutofillableLogins(PasswordFormList* forms);
  virtual bool GetBlacklistLogins(PasswordFormList* forms);

 private:
  // Initialization.
  bool StartKWalletd();
  bool InitWallet();

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

  // Checks if the last DBus call returned an error. If it did, logs the error
  // message, frees it and returns true.
  // This must be called after every DBus call.
  bool CheckError();

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
  static bool CheckSerializedValue(const GArray* byte_array, const char* realm);

  // Deserializes a list of PasswordForms from the wallet.
  static void DeserializeValue(const std::string& signon_realm,
                               const Pickle& pickle,
                               PasswordFormList* forms);

  // Convenience function to read a GURL from a Pickle. Assumes the URL has
  // been written as a std::string. Returns true on success.
  static bool ReadGURL(const Pickle& pickle, void** iter, GURL* url);

  // In case the fields in the pickle ever change, version them so we can try to
  // read old pickles. (Note: do not eat old pickles past the expiration date.)
  static const int kPickleVersion = 0;

  // Name of the application - will appear in kwallet's dialogs.
  static const char* kAppId;
  // Name of the folder to store passwords in.
  static const char* kKWalletFolder;

  // DBus stuff.
  static const char* kKWalletServiceName;
  static const char* kKWalletPath;
  static const char* kKWalletInterface;
  static const char* kKLauncherServiceName;
  static const char* kKLauncherPath;
  static const char* kKLauncherInterface;

  // Invalid handle returned by WalletHandle().
  static const int kInvalidKWalletHandle = -1;

  // Error from the last DBus call. NULL when there's no error. Freed and
  // cleared by CheckError().
  GError* error_;
  // Connection to the DBus session bus.
  DBusGConnection* connection_;
  // Proxy to the kwallet DBus service.
  DBusGProxy* proxy_;

  // The name of the wallet we've opened. Set during Init().
  std::string wallet_name_;

  DISALLOW_COPY_AND_ASSIGN(NativeBackendKWallet);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_KWALLET_X_H_
