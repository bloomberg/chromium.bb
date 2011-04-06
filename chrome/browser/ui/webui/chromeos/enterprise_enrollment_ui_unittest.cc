// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::HasSubstr;
using testing::Not;

void TestStringStillOkForEnterpriseEnrollment(int resource_id) {
  std::string resource_string = l10n_util::GetStringUTF8(resource_id);
  EXPECT_THAT(resource_string, Not(HasSubstr("Sync")));
  EXPECT_THAT(resource_string, Not(HasSubstr("sync")));
}

// This set of strings to test was generated from
// CloudPrintSetupSource::StartDataRequest.  If any of these trip, notify the
// cloud printing team and we'll split the strings.
TEST(EnterpriseEnrollmentResources, SharedStringsCheck) {
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_LOGIN_SIGNIN_PREFIX);
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_LOGIN_SIGNIN_SUFFIX);
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_CANNOT_BE_BLANK);
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_LOGIN_EMAIL);
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_LOGIN_PASSWORD);
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_INVALID_USER_CREDENTIALS);
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_SIGNIN);
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_LOGIN_COULD_NOT_CONNECT);
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_LOGIN_SETTING_UP);
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_SUCCESS);
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_ERROR_SIGNING_IN);
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_GAIA_CAPTCHA_INSTRUCTIONS);
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_INVALID_ACCESS_CODE_LABEL);
  TestStringStillOkForEnterpriseEnrollment(IDS_SYNC_ENTER_ACCESS_CODE_LABEL);
}
