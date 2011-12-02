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

  MOCK_METHOD2(StartStorePolicyOp,
               void(const em::PolicyFetchResponse&,
                    SignedSettingsHelper::StorePolicyCallback));
  MOCK_METHOD1(StartRetrievePolicyOp,
               void(SignedSettingsHelper::RetrievePolicyCallback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSignedSettingsHelper);
};

ACTION_P(MockSignedSettingsHelperStorePolicy, status_code) {
  arg1.Run(status_code);
}

ACTION_P2(MockSignedSettingsHelperRetrievePolicy, status_code, policy) {
  arg0.Run(status_code, policy);
}

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_SIGNED_SETTINGS_HELPER_H_
