// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

using testing::HasSubstr;
using testing::Not;

void TestStringStillOkForCloudPrint(int resource_id) {
  std::string resource_string = l10n_util::GetStringUTF8(resource_id);
  EXPECT_THAT(resource_string, Not(HasSubstr("Sync")));
  EXPECT_THAT(resource_string, Not(HasSubstr("sync")));
}

// This set of strings to test was generated from
// CloudPrintSetupSource::StartDataRequest.  If any of these trip, notify the
// cloud printing team and we'll split the strings.
TEST(CloudPrintResources, SharedStringsCheck) {
  TestStringStillOkForCloudPrint(IDS_SYNC_LOGIN_SIGNIN_PREFIX);
  TestStringStillOkForCloudPrint(IDS_SYNC_LOGIN_SIGNIN_SUFFIX);
  TestStringStillOkForCloudPrint(IDS_SYNC_CANNOT_BE_BLANK);
  TestStringStillOkForCloudPrint(IDS_SYNC_LOGIN_EMAIL);
  TestStringStillOkForCloudPrint(IDS_SYNC_LOGIN_PASSWORD);
  TestStringStillOkForCloudPrint(IDS_SYNC_INVALID_USER_CREDENTIALS);
  TestStringStillOkForCloudPrint(IDS_SYNC_SIGNIN);
  TestStringStillOkForCloudPrint(IDS_SYNC_LOGIN_COULD_NOT_CONNECT);
  TestStringStillOkForCloudPrint(IDS_SYNC_CANNOT_ACCESS_ACCOUNT);
  TestStringStillOkForCloudPrint(IDS_SYNC_CREATE_ACCOUNT);
  TestStringStillOkForCloudPrint(IDS_SYNC_LOGIN_SETTING_UP);
  TestStringStillOkForCloudPrint(IDS_SYNC_SUCCESS);
  TestStringStillOkForCloudPrint(IDS_SYNC_ERROR_SIGNING_IN);
  TestStringStillOkForCloudPrint(IDS_SYNC_GAIA_CAPTCHA_INSTRUCTIONS);
  TestStringStillOkForCloudPrint(IDS_SYNC_INVALID_ACCESS_CODE_LABEL);
  TestStringStillOkForCloudPrint(IDS_SYNC_ENTER_ACCESS_CODE_LABEL);
  TestStringStillOkForCloudPrint(IDS_SYNC_ACCESS_CODE_HELP_LABEL);
  TestStringStillOkForCloudPrint(IDS_SYNC_GET_ACCESS_CODE_URL);
  TestStringStillOkForCloudPrint(IDS_SYNC_SUCCESS);
  TestStringStillOkForCloudPrint(IDS_SYNC_SETUP_OK_BUTTON_LABEL);
}
