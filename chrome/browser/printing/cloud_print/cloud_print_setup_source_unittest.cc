// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/ui_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_LOGIN_SIGNIN_PREFIX);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_LOGIN_SIGNIN_SUFFIX);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_CANNOT_BE_BLANK);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_LOGIN_EMAIL_SAME_LINE);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_LOGIN_PASSWORD_SAME_LINE);
  TestStringStillOkForCloudPrint(IDS_SYNC_INVALID_USER_CREDENTIALS);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_SIGNIN);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_LOGIN_COULD_NOT_CONNECT);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_CANNOT_ACCESS_ACCOUNT);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_CREATE_ACCOUNT);
  TestStringStillOkForCloudPrint(IDS_SYNC_LOGIN_SETTING_UP);
  TestStringStillOkForCloudPrint(IDS_SYNC_SUCCESS);
  TestStringStillOkForCloudPrint(IDS_SYNC_ERROR_SIGNING_IN);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_GAIA_CAPTCHA_INSTRUCTIONS);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_INVALID_ACCESS_CODE_LABEL);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_ENTER_ACCESS_CODE_LABEL);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_ACCESS_CODE_HELP_LABEL);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_GET_ACCESS_CODE_URL);
  TestStringStillOkForCloudPrint(IDS_SYNC_SUCCESS);
  TestStringStillOkForCloudPrint(IDS_CLOUD_PRINT_SETUP_OK_BUTTON_LABEL);
}
