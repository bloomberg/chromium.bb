// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_H_
#pragma once

#include <string>

#include "base/ref_counted.h"
#include "chrome/browser/chromeos/login/owner_manager.h"

// There are two categories of operations that can be performed on the
// Chrome OS owner-signed settings store:
// 1) doing stuff to the whitelist (adding/removing/checking)
// 2) Storing/Retrieving arbitrary name=value pairs
//
// Unfortunately, it is currently a limitation that only one of each
// category can be in-flight at a time.  You can be doing exactly one thing
// to the whitelist, and exactly one thing to the property store at a time.
// I've filed an issue on me to remove that restriction.
// http://code.google.com/p/chromium-os/issues/detail?id=6415

// The pattern of use here is that the caller instantiates some
// subclass of SignedSettings by calling one of the create
// methods. Then, call Execute() on this object from the UI
// thread. It'll go off and do work (on the FILE thread and over DBus),
// and then call the appropriate method of the Delegate you passed in
// -- again, on the UI thread.

namespace chromeos {
class OwnershipService;

class SignedSettings : public base::RefCountedThreadSafe<SignedSettings>,
                       public OwnerManager::Delegate {
 public:
  enum ReturnCode {
    SUCCESS,
    NOT_FOUND,        // Email address or property name not found.
    KEY_UNAVAILABLE,  // Owner key not yet configured.
    OPERATION_FAILED  // Signature op or IPC to signed settings daemon failed.
  };

  template <class T>
  class Delegate {
   public:
    // This method will be called on the UI thread.
    virtual void OnSettingsOpCompleted(ReturnCode code, T value) {}
  };

  SignedSettings();
  virtual ~SignedSettings();

  // These are both "whitelist" operations, and only one instance of
  // one type can be in flight at a time.
  static SignedSettings* CreateCheckWhitelistOp(
      const std::string& email,
      SignedSettings::Delegate<bool>* d);

  static SignedSettings* CreateWhitelistOp(const std::string& email,
                                           bool add_to_whitelist,
                                           SignedSettings::Delegate<bool>* d);

  // These are both "property" operations, and only one instance of
  // one type can be in flight at a time.
  static SignedSettings* CreateStorePropertyOp(
      const std::string& name,
      const std::string& value,
      SignedSettings::Delegate<bool>* d);

  static SignedSettings* CreateRetrievePropertyOp(
      const std::string& name,
      SignedSettings::Delegate<std::string>* d);

  // These are both "policy" operations, and only one instance of
  // one type can be in flight at a time.
  static SignedSettings* CreateStorePolicyOp(
      const std::string& value,
      SignedSettings::Delegate<bool>* d);

  static SignedSettings* CreateRetrievePolicyOp(
      SignedSettings::Delegate<std::string>* d);

  static ReturnCode MapKeyOpCode(OwnerManager::KeyOpCode code);

  virtual void Execute() = 0;

  // Implementation of OwnerManager::Delegate::OnKeyOpComplete()
  void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                       const std::vector<uint8>& payload) = 0;

 protected:
  OwnershipService* service_;

 private:
  friend class SignedSettingsTest;
  friend class SignedSettingsHelperTest;

  void set_service(OwnershipService* service) { service_ = service; }
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_H_
