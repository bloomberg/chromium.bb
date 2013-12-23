// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/web_resource/notification_promo.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/web_resource/notification_promo_mobile_ntp.h"

class PromoResourceServiceMobileNtpTest : public testing::Test {
 public:
  // |promo_resource_service_| must be created after |local_state_|.
  PromoResourceServiceMobileNtpTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        promo_resource_service_(new PromoResourceService) {}

 protected:
  ScopedTestingLocalState local_state_;
  scoped_refptr<PromoResourceService> promo_resource_service_;
  base::MessageLoop loop_;
};

class NotificationPromoMobileNtpTest {
 public:
  NotificationPromoMobileNtpTest() : received_notification_(false) {}

  void Init(const std::string& json,
            const std::string& promo_text,
            const std::string& promo_text_long,
            const std::string& promo_action_type,
            const std::string& promo_action_arg0,
            const std::string& promo_action_arg1) {
    base::Value* value(base::JSONReader::Read(json));
    ASSERT_TRUE(value);
    base::DictionaryValue* dict = NULL;
    value->GetAsDictionary(&dict);
    ASSERT_TRUE(dict);
    test_json_.reset(dict);

    promo_text_ = promo_text;
    promo_text_long_ = promo_text_long;
    promo_action_type_ = promo_action_type;
    promo_action_args_.push_back(promo_action_arg0);
    promo_action_args_.push_back(promo_action_arg1);

    received_notification_ = false;
  }

  void InitPromoFromJson(bool should_receive_notification) {
    EXPECT_TRUE(mobile_promo_.InitFromJson(*test_json_));
    EXPECT_TRUE(mobile_promo_.valid());
    EXPECT_EQ(should_receive_notification,
              mobile_promo_.notification_promo().new_notification());

    // Test the fields.
    TestNotification();
  }

  void TestNotification() {
    // Check values.
    EXPECT_TRUE(mobile_promo_.valid());
    EXPECT_EQ(mobile_promo_.text(), promo_text_);
    EXPECT_EQ(mobile_promo_.text_long(), promo_text_long_);
    EXPECT_EQ(mobile_promo_.action_type(), promo_action_type_);
    EXPECT_TRUE(mobile_promo_.action_args() != NULL);
    EXPECT_EQ(2u, promo_action_args_.size());
    EXPECT_EQ(mobile_promo_.action_args()->GetSize(),
              promo_action_args_.size());
    for (std::size_t i = 0; i < promo_action_args_.size(); ++i) {
      std::string value;
      EXPECT_TRUE(mobile_promo_.action_args()->GetString(i, &value));
      EXPECT_EQ(value, promo_action_args_[i]);
    }
  }

  // Create a new NotificationPromo from prefs and compare to current
  // notification.
  void TestInitFromPrefs() {
    NotificationPromoMobileNtp prefs_mobile_promo;
    EXPECT_TRUE(prefs_mobile_promo.InitFromPrefs());
    EXPECT_TRUE(prefs_mobile_promo.valid());
    EXPECT_TRUE(mobile_promo_.valid());

    EXPECT_EQ(prefs_mobile_promo.text(),
              mobile_promo_.text());
    EXPECT_EQ(prefs_mobile_promo.text_long(),
              mobile_promo_.text_long());
    EXPECT_EQ(prefs_mobile_promo.action_type(),
              mobile_promo_.action_type());
    EXPECT_TRUE(mobile_promo_.action_args() != NULL);
    EXPECT_EQ(prefs_mobile_promo.action_args()->GetSize(),
              mobile_promo_.action_args()->GetSize());
    for (std::size_t i = 0;
         i < prefs_mobile_promo.action_args()->GetSize();
         ++i) {
      std::string promo_value;
      std::string prefs_value;
      EXPECT_TRUE(
          prefs_mobile_promo.action_args()->GetString(i, &prefs_value));
      EXPECT_TRUE(
          mobile_promo_.action_args()->GetString(i, &promo_value));
      EXPECT_EQ(promo_value, prefs_value);
    }
  }

 private:
  NotificationPromoMobileNtp mobile_promo_;
  bool received_notification_;
  scoped_ptr<base::DictionaryValue> test_json_;

  std::string promo_text_;
  std::string promo_text_long_;
  std::string promo_action_type_;
  std::vector<std::string> promo_action_args_;
};

TEST_F(PromoResourceServiceMobileNtpTest, NotificationPromoMobileNtpTest) {
  NotificationPromoMobileNtpTest promo_test;

  // Set up start and end dates and promo line in a Dictionary as if parsed
  // from the service.
  promo_test.Init(
      "{"
      "  \"mobile_ntp_sync_promo\": ["
      "    {"
      "      \"date\":"
      "        ["
      "          {"
      "            \"start\":\"3 Aug 1999 9:26:06 GMT\","
      "            \"end\":\"7 Jan 2013 5:40:75 PST\""
      "          }"
      "        ],"
      "      \"strings\":"
      "        {"
      "          \"MOBILE_PROMO_CHROME_SHORT_TEXT\":"
      "              \"Like Chrome? Go http://www.google.com/chrome/\","
      "          \"MOBILE_PROMO_CHROME_LONG_TEXT\":"
      "              \"It's simple. Go http://www.google.com/chrome/\","
      "          \"MOBILE_PROMO_EMAIL_BODY\":\"This is the body.\","
      "          \"XXX\":\"XXX value\""
      "        },"
      "      \"payload\":"
      "        {"
      "          \"payload_format_version\":3,"
      "          \"promo_message_long\":"
      "              \"MOBILE_PROMO_CHROME_LONG_TEXT\","
      "          \"promo_message_short\":"
      "              \"MOBILE_PROMO_CHROME_SHORT_TEXT\","
      "          \"promo_action_type\":\"ACTION_EMAIL\","
      "          \"promo_action_args\":[\"MOBILE_PROMO_EMAIL_BODY\",\"XXX\"]"
      "        },"
      "      \"max_views\":30"
      "    }"
      "  ]"
      "}",
      "Like Chrome? Go http://www.google.com/chrome/",
      "It\'s simple. Go http://www.google.com/chrome/",
      "ACTION_EMAIL", "This is the body.", "XXX value");

  promo_test.InitPromoFromJson(true);

  // Second time should not trigger a notification.
  promo_test.InitPromoFromJson(false);

  promo_test.TestInitFromPrefs();
}
