// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/owner_manager.h"

// There are two operations that can be performed on the Chrome OS owner-signed
// settings store: Storing and Retrieving the policy blob.
//
// The pattern of use here is that the caller instantiates some
// subclass of SignedSettings by calling one of the create
// methods. Then, call Execute() on this object from the UI
// thread. It'll go off and do work (on the FILE thread and over DBus),
// and then call the appropriate method of the Delegate you passed in
// -- again, on the UI thread.

namespace base {
class Value;
}  // namespace base

namespace enterprise_management {
class PolicyFetchResponse;
class PolicyData;
}  // namespace enterprise_management
namespace em = enterprise_management;

namespace chromeos {
class OwnershipService;

extern const char kDevicePolicyType[];

class SignedSettings : public base::RefCountedThreadSafe<SignedSettings>,
                       public OwnerManager::Delegate {
 public:
  enum ReturnCode {
    SUCCESS,
    NOT_FOUND,         // Email address or property name not found.
    KEY_UNAVAILABLE,   // Owner key not yet configured.
    OPERATION_FAILED,  // IPC to signed settings daemon failed.
    BAD_SIGNATURE      // Signature verification failed.
  };

  template <class T>
  class Delegate {
   public:
    // This method will be called on the UI thread.
    virtual void OnSettingsOpCompleted(ReturnCode code, T value) {}
  };

  SignedSettings();
  virtual ~SignedSettings();

  // These are both "policy" operations, and only one instance of
  // one type can be in flight at a time.
  static SignedSettings* CreateStorePolicyOp(
      em::PolicyFetchResponse* policy,
      SignedSettings::Delegate<bool>* d);

  static SignedSettings* CreateRetrievePolicyOp(
      SignedSettings::Delegate<const em::PolicyFetchResponse&>* d);

  static ReturnCode MapKeyOpCode(OwnerManager::KeyOpCode code);

  virtual void Execute() = 0;

  virtual void Fail(ReturnCode code) = 0;

  // Implementation of OwnerManager::Delegate
  virtual void OnKeyOpComplete(const OwnerManager::KeyOpCode return_code,
                               const std::vector<uint8>& payload) = 0;

 protected:
  static bool PolicyIsSane(const em::PolicyFetchResponse& value,
                           em::PolicyData* poldata);

  void set_service(OwnershipService* service) { service_ = service; }

  OwnershipService* service_;

 private:
  friend class SignedSettingsTest;
  friend class SignedSettingsHelperTest;

  DISALLOW_COPY_AND_ASSIGN(SignedSettings);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_H_
