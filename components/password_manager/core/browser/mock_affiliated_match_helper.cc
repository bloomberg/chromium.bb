// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/mock_affiliated_match_helper.h"

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliation_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

MockAffiliatedMatchHelper::MockAffiliatedMatchHelper()
    : AffiliatedMatchHelper(nullptr,
                            make_scoped_ptr<AffiliationService>(nullptr)) {}

MockAffiliatedMatchHelper::~MockAffiliatedMatchHelper() {}

void MockAffiliatedMatchHelper::ExpectCallToGetAffiliatedAndroidRealms(
    const autofill::PasswordForm& expected_observed_form,
    const std::vector<std::string>& results_to_return) {
  EXPECT_CALL(*this, OnGetAffiliatedAndroidRealmsCalled(expected_observed_form))
      .WillOnce(testing::Return(results_to_return));
}

void MockAffiliatedMatchHelper::ExpectCallToGetAffiliatedWebRealms(
    const autofill::PasswordForm& expected_android_form,
    const std::vector<std::string>& results_to_return) {
  EXPECT_CALL(*this, OnGetAffiliatedWebRealmsCalled(expected_android_form))
      .WillOnce(testing::Return(results_to_return));
}

void MockAffiliatedMatchHelper::GetAffiliatedAndroidRealms(
    const autofill::PasswordForm& observed_form,
    const AffiliatedRealmsCallback& result_callback) {
  std::vector<std::string> affiliated_android_realms =
      OnGetAffiliatedAndroidRealmsCalled(observed_form);
  result_callback.Run(affiliated_android_realms);
}

void MockAffiliatedMatchHelper::GetAffiliatedWebRealms(
    const autofill::PasswordForm& android_form,
    const AffiliatedRealmsCallback& result_callback) {
  std::vector<std::string> affiliated_web_realms =
      OnGetAffiliatedWebRealmsCalled(android_form);
  result_callback.Run(affiliated_web_realms);
}

}  // namespace password_manager
