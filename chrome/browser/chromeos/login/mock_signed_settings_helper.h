// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_SIGNED_SETTINGS_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_SIGNED_SETTINGS_HELPER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockSignedSettingsHelper : public chromeos::SignedSettingsHelper {
 public:
  MockSignedSettingsHelper();
  virtual ~MockSignedSettingsHelper();

  MOCK_METHOD2(StartCheckWhitelistOp,
               void(const std::string& email, Callback*));
  MOCK_METHOD3(StartWhitelistOp,
               void(const std::string&, bool, Callback*));
  MOCK_METHOD3(StartStorePropertyOp,
               void(const std::string&, const base::Value&, Callback*));
  MOCK_METHOD2(StartRetrieveProperty,
               void(const std::string&, Callback*));
  MOCK_METHOD2(StartStorePolicyOp,
               void(const em::PolicyFetchResponse&, Callback*));
  MOCK_METHOD1(StartRetrievePolicyOp, void(Callback* callback));
  MOCK_METHOD1(CancelCallback, void(Callback*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSignedSettingsHelper);
};

ACTION_P(MockSignedSettingsHelperStorePolicy, status_code) {
  arg1->OnStorePolicyCompleted(status_code);
}

ACTION_P2(MockSignedSettingsHelperRetrievePolicy, status_code, policy) {
  arg0->OnRetrievePolicyCompleted(status_code, policy);
}

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_SIGNED_SETTINGS_HELPER_H_
