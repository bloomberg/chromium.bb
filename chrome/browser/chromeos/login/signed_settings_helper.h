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
  typedef base::Callback<void(SignedSettings::ReturnCode)> StorePolicyCallback;
  typedef base::Callback<void(SignedSettings::ReturnCode,
      const em::PolicyFetchResponse&)> RetrievePolicyCallback;

  // Class factory
  static SignedSettingsHelper* Get();

  // Functions to start signed settings ops.
  virtual void StartStorePolicyOp(
      const em::PolicyFetchResponse& policy,
      StorePolicyCallback callback) = 0;
  virtual void StartRetrievePolicyOp(
      RetrievePolicyCallback callback) = 0;

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
