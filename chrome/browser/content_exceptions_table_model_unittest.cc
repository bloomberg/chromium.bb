// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_exceptions_table_model.h"

#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ContentExceptionsTableModelTest : public TestingBrowserProcessTest {
 public:
  ContentExceptionsTableModelTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

 protected:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
};

TEST_F(ContentExceptionsTableModelTest, Incognito) {
  TestingProfile profile;
  TestingProfile* otr_profile = new TestingProfile();
  otr_profile->set_off_the_record(true);
  ContentExceptionsTableModel model(profile.GetHostContentSettingsMap(),
                                    otr_profile->GetHostContentSettingsMap(),
                                    CONTENT_SETTINGS_TYPE_COOKIES);
  delete otr_profile;
  model.AddException(ContentSettingsPattern("example.com"),
                     CONTENT_SETTING_BLOCK, true);
}

}  // namespace
