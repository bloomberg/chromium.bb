// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_KWALLET_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_KWALLET_H_

#include <dbus/dbus-glib.h>
#include <glib.h>

#include <string>
#include <vector>

#include "base/lock.h"
#include "chrome/browser/password_manager/login_database.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "webkit/glue/password_form.h"

class Pickle;
class Profile;
class Task;

class PasswordStoreKWallet : public PasswordStore {
 public:
  PasswordStoreKWallet(LoginDatabase* login_db,
                       Profile* profile,
                       WebDataService* web_data_service);

  bool Init();

 private:
  typedef std::vector<webkit_glue::PasswordForm*> PasswordFormList;

  virtual ~PasswordStoreKWallet();

  // Implements PasswordStore interface.
  virtual void AddLoginImpl(const webkit_glue::PasswordForm& form);
  virtual void UpdateLoginImpl(const webkit_glue::PasswordForm& form);
  virtual void RemoveLoginImpl(const webkit_glue::PasswordForm& form);
  virtual void RemoveLoginsCreatedBetweenImpl(const base::Time& delete_begin,
                                              const base::Time& delete_end);
  virtual void GetLoginsImpl(GetLoginsRequest* request,
                             const webkit_glue::PasswordForm& form);
  virtual void GetAutofillableLoginsImpl(GetLoginsRequest* request);
  virtual void GetBlacklistLoginsImpl(GetLoginsRequest* request);
  virtual bool FillAutofillableLogins(
      std::vector<webkit_glue::PasswordForm*>* forms);
  virtual bool FillBlacklistLogins(
      std::vector<webkit_glue::PasswordForm*>* forms);

  // Initialization.
  bool StartKWalletd();
  bool InitWallet();

  // Reads a list of PasswordForms from the wallet that match the signon_realm.
  void GetLoginsList(PasswordFormList* forms,
                     const std::string& signon_realm,
                     int wallet_handle);

  // Writes a list of PasswordForms to the wallet with the given signon_realm.
  // Overwrites any existing list for this signon_realm. Removes the entry if
  // |forms| is empty.
  void SetLoginsList(const PasswordFormList& forms,
                     const std::string& signon_realm,
                     int wallet_handle);

  // Helper for FillAutofillableLogins() and FillBlacklistLogins().
  bool FillSomeLogins(bool autofillable, PasswordFormList* forms);

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

  // Deserializes a list of PasswordForms from the wallet.
  static void DeserializeValue(const std::string& signon_realm,
                               const Pickle& pickle,
                               PasswordFormList* forms);

  // Convenience function to read a GURL from a Pickle. Assumes the URL has
  // been written as a std::string.
  static void ReadGURL(const Pickle& pickle, void** iter, GURL* url);

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

  // Controls all access to kwallet DBus calls.
  Lock kwallet_lock_;

  // Error from the last DBus call. NULL when there's no error. Freed and
  // cleared by CheckError().
  GError* error_;
  // Connection to the DBus session bus.
  DBusGConnection* connection_;
  // Proxy to the kwallet DBus service.
  DBusGProxy* proxy_;

  // The name of the wallet we've opened. Set during Init().
  std::string wallet_name_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreKWallet);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_KWALLET_H_
