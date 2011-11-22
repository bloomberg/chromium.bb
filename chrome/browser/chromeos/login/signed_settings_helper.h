// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_HELPER_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/signed_settings.h"

namespace base {
class Value;
}  // namespace base

namespace enterprise_management {
class PolicyFetchResponse;
}  // namespace enterprise_management
namespace em = enterprise_management;
namespace chromeos {

class SignedSettings;

// Helper to serialize signed settings ops, provide unified callback interface,
// and handle callbacks destruction before ops completion.
class SignedSettingsHelper {
 public:
  class Callback {
   public:
    // Callback of CheckWhitelistOp. |success| indicates whether the op succeeds
    // or not. |email| is the email that is checked against.
    virtual void OnCheckWhitelistCompleted(
        SignedSettings::ReturnCode code,
        const std::string& email) {}

    // Callback of WhitelistOp that adds |email| to the whitelist.
    virtual void OnWhitelistCompleted(
        SignedSettings::ReturnCode code, const std::string& email) {}

    // Callback of WhitelistOp that removes |email| to the whitelist.
    virtual void OnUnwhitelistCompleted(
        SignedSettings::ReturnCode code, const std::string& email) {}

    // Callback of StorePropertyOp.
    virtual void OnStorePropertyCompleted(
        SignedSettings::ReturnCode code,
        const std::string& name,
        const base::Value& value) {}

    // Callback of RetrievePropertyOp.
    virtual void OnRetrievePropertyCompleted(
        SignedSettings::ReturnCode code,
        const std::string& name,
        const base::Value* value) {}

    // Callback of StorePolicyOp.
    virtual void OnStorePolicyCompleted(
        SignedSettings::ReturnCode code) {}

    // Callback of RetrievePolicyOp.
    virtual void OnRetrievePolicyCompleted(
        SignedSettings::ReturnCode code,
        const em::PolicyFetchResponse& policy) {}
  };

  // Class factory
  static SignedSettingsHelper* Get();

  // Functions to start signed settings ops.
  virtual void StartCheckWhitelistOp(const std::string& email,
                                     Callback* callback) = 0;
  virtual void StartWhitelistOp(const std::string& email,
                                bool add_to_whitelist,
                                Callback* callback) = 0;
  virtual void StartStorePropertyOp(const std::string& name,
                                    const base::Value& value,
                                    Callback* callback) = 0;
  virtual void StartRetrieveProperty(const std::string& name,
                                     Callback* callback) = 0;
  virtual void StartStorePolicyOp(const em::PolicyFetchResponse& policy,
                                  Callback* callback) = 0;
  virtual void StartRetrievePolicyOp(Callback* callback) = 0;

  // Cancels all pending calls of given callback.
  virtual void CancelCallback(Callback* callback) = 0;

  class TestDelegate {
   public:
    virtual void OnOpCreated(SignedSettings* op) = 0;
    virtual void OnOpStarted(SignedSettings* op) = 0;
    virtual void OnOpCompleted(SignedSettings* op) = 0;
  };

#if defined(UNIT_TEST)
  void set_test_delegate(TestDelegate* test_delegate) {
    test_delegate_ = test_delegate;
  }
#endif  // defined(UNIT_TEST)

 protected:
  SignedSettingsHelper() : test_delegate_(NULL) {
  }

  TestDelegate* test_delegate_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_HELPER_H_
