// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_KWALLET_X_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_NATIVE_BACKEND_KWALLET_X_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "chrome/browser/profiles/profile.h"

class Pickle;
class PickleIterator;
class PrefService;

namespace content {
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

  // Serializes a list of PasswordForms to be stored in the wallet.
  static void SerializeValue(const PasswordFormList& forms, Pickle* pickle);

  // Deserializes a list of PasswordForms from the wallet.
  // |size_32| controls reading the size field within the pickle as 32 bits.
  // We used to use Pickle::WriteSize() to write the number of password forms,
  // but that has a different size on 32- and 64-bit systems. So, now we always
  // write a 64-bit quantity, but we support trying to read it as either size
  // when reading old pickles that fail to deserialize using the native size.
  static bool DeserializeValueSize(const std::string& signon_realm,
                                   const PickleIterator& iter,
                                   bool size_32, bool warn_only,
                                   PasswordFormList* forms);

  // In case the fields in the pickle ever change, version them so we can try to
  // read old pickles. (Note: do not eat old pickles past the expiration date.)
  static const int kPickleVersion = 1;

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
