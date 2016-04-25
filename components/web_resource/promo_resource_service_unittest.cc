// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "components/web_resource/notification_promo.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"

namespace web_resource {

namespace {

version_info::Channel kChannel = version_info::Channel::UNKNOWN;

const char kDateFormat[] = "dd MMM yyyy HH:mm:ss zzzz";

bool YearFromNow(double* date_epoch, std::string* date_string) {
  *date_epoch = (base::Time::Now() + base::TimeDelta::FromDays(365)).ToTimeT();

  UErrorCode status = U_ZERO_ERROR;
  icu::SimpleDateFormat simple_formatter(icu::UnicodeString(kDateFormat),
                                         icu::Locale("en_US"), status);
  if (!U_SUCCESS(status))
    return false;

  icu::UnicodeString date_unicode_string;
  simple_formatter.format(static_cast<UDate>(*date_epoch * 1000),
                          date_unicode_string, status);
  if (!U_SUCCESS(status))
    return false;

  return base::UTF16ToUTF8(date_unicode_string.getBuffer(),
                           static_cast<size_t>(date_unicode_string.length()),
                           date_string);
}

}  // namespace

class NotificationPromoTest : public testing::Test {
 public:
  NotificationPromoTest()
      : notification_promo_(&local_state_),
        received_notification_(false),
        start_(0.0),
        end_(0.0),
        num_groups_(0),
        initial_segment_(0),
        increment_(1),
        time_slice_(0),
        max_group_(0),
        max_views_(0),
        closed_(false) {
    NotificationPromo::RegisterPrefs(local_state_.registry());
  }

  void Init(const std::string& json,
            const std::string& promo_text,
            double start,
            int num_groups,
            int initial_segment,
            int increment,
            int time_slice,
            int max_group,
            int max_views) {
    double year_from_now_epoch;
    std::string year_from_now_string;
    ASSERT_TRUE(YearFromNow(&year_from_now_epoch, &year_from_now_string));

    std::vector<std::string> replacements;
    replacements.push_back(year_from_now_string);

    std::string json_with_end_date(
        base::ReplaceStringPlaceholders(json, replacements, NULL));
    base::Value* value(base::JSONReader::Read(json_with_end_date).release());
    ASSERT_TRUE(value);

    base::DictionaryValue* dict = NULL;
    value->GetAsDictionary(&dict);
    ASSERT_TRUE(dict);
    test_json_.reset(dict);

    promo_type_ = NotificationPromo::NTP_NOTIFICATION_PROMO;
    promo_text_ = promo_text;

    start_ = start;
    end_ = year_from_now_epoch;

    num_groups_ = num_groups;
    initial_segment_ = initial_segment;
    increment_ = increment;
    time_slice_ = time_slice;
    max_group_ = max_group;

    max_views_ = max_views;

    closed_ = false;
    received_notification_ = false;
  }

  void InitPromoFromJson(bool should_receive_notification) {
    notification_promo_.InitFromJson(*test_json_, promo_type_);
    EXPECT_EQ(should_receive_notification,
              notification_promo_.new_notification());

    // Test the fields.
    TestNotification();
  }

  void TestNotification() {
    // Check values.
    EXPECT_EQ(notification_promo_.promo_text_, promo_text_);

    EXPECT_EQ(notification_promo_.start_, start_);
    EXPECT_EQ(notification_promo_.end_, end_);

    EXPECT_EQ(notification_promo_.num_groups_, num_groups_);
    EXPECT_EQ(notification_promo_.initial_segment_, initial_segment_);
    EXPECT_EQ(notification_promo_.increment_, increment_);
    EXPECT_EQ(notification_promo_.time_slice_, time_slice_);
    EXPECT_EQ(notification_promo_.max_group_, max_group_);

    EXPECT_EQ(notification_promo_.max_views_, max_views_);
    EXPECT_EQ(notification_promo_.closed_, closed_);

    // Check group within bounds.
    EXPECT_GE(notification_promo_.group_, 0);
    EXPECT_LT(notification_promo_.group_, num_groups_);

    // Views should be 0 for now.
    EXPECT_EQ(notification_promo_.views_, 0);
  }

  // Create a new NotificationPromo from prefs and compare to current
  // notification.
  void TestInitFromPrefs() {
    NotificationPromo prefs_notification_promo(&local_state_);
    prefs_notification_promo.InitFromPrefs(promo_type_);

    EXPECT_EQ(notification_promo_.local_state_,
              prefs_notification_promo.local_state_);
    EXPECT_EQ(notification_promo_.promo_text_,
              prefs_notification_promo.promo_text_);
    EXPECT_EQ(notification_promo_.start_, prefs_notification_promo.start_);
    EXPECT_EQ(notification_promo_.end_, prefs_notification_promo.end_);
    EXPECT_EQ(notification_promo_.num_groups_,
              prefs_notification_promo.num_groups_);
    EXPECT_EQ(notification_promo_.initial_segment_,
              prefs_notification_promo.initial_segment_);
    EXPECT_EQ(notification_promo_.increment_,
              prefs_notification_promo.increment_);
    EXPECT_EQ(notification_promo_.time_slice_,
              prefs_notification_promo.time_slice_);
    EXPECT_EQ(notification_promo_.max_group_,
              prefs_notification_promo.max_group_);
    EXPECT_EQ(notification_promo_.max_views_,
              prefs_notification_promo.max_views_);
    EXPECT_EQ(notification_promo_.group_, prefs_notification_promo.group_);
    EXPECT_EQ(notification_promo_.views_, prefs_notification_promo.views_);
    EXPECT_EQ(notification_promo_.closed_, prefs_notification_promo.closed_);
  }

  void TestGroup() {
    // Test out of range groups.
    const int incr = num_groups_ / 20;
    for (int i = max_group_; i < num_groups_; i += incr) {
      notification_promo_.group_ = i;
      EXPECT_FALSE(notification_promo_.CanShow());
    }

    // Test in-range groups.
    for (int i = 0; i < max_group_; i += incr) {
      notification_promo_.group_ = i;
      EXPECT_TRUE(notification_promo_.CanShow());
    }

    // When max_group_ is 0, all groups pass.
    notification_promo_.max_group_ = 0;
    for (int i = 0; i < num_groups_; i += incr) {
      notification_promo_.group_ = i;
      EXPECT_TRUE(notification_promo_.CanShow());
    }
    notification_promo_.WritePrefs();
  }

  void TestViews() {
    notification_promo_.views_ = notification_promo_.max_views_ - 2;
    notification_promo_.WritePrefs();

    NotificationPromo::HandleViewed(promo_type_, &local_state_);
    NotificationPromo new_promo(&local_state_);
    new_promo.InitFromPrefs(promo_type_);
    EXPECT_EQ(new_promo.max_views_ - 1, new_promo.views_);
    EXPECT_TRUE(new_promo.CanShow());
    NotificationPromo::HandleViewed(promo_type_, &local_state_);
    new_promo.InitFromPrefs(promo_type_);
    EXPECT_EQ(new_promo.max_views_, new_promo.views_);
    EXPECT_FALSE(new_promo.CanShow());

    // Test out of range views.
    for (int i = max_views_; i < max_views_ * 2; ++i) {
      new_promo.views_ = i;
      EXPECT_FALSE(new_promo.CanShow());
    }

    // Test in range views.
    for (int i = 0; i < max_views_; ++i) {
      new_promo.views_ = i;
      EXPECT_TRUE(new_promo.CanShow());
    }
    new_promo.WritePrefs();
  }

  void TestClosed() {
    NotificationPromo new_promo(&local_state_);
    new_promo.InitFromPrefs(promo_type_);
    EXPECT_FALSE(new_promo.closed_);
    EXPECT_TRUE(new_promo.CanShow());

    NotificationPromo::HandleClosed(promo_type_, &local_state_);
    new_promo.InitFromPrefs(promo_type_);
    EXPECT_TRUE(new_promo.closed_);
    EXPECT_FALSE(new_promo.CanShow());

    new_promo.closed_ = false;
    EXPECT_TRUE(new_promo.CanShow());
    new_promo.WritePrefs();
  }

  void TestPromoText() {
    notification_promo_.promo_text_.clear();
    EXPECT_FALSE(notification_promo_.CanShow());

    notification_promo_.promo_text_ = promo_text_;
    EXPECT_TRUE(notification_promo_.CanShow());
  }

  void TestTime() {
    const double now = base::Time::Now().ToDoubleT();
    const double qhour = 15 * 60;

    notification_promo_.group_ = 0;  // For simplicity.

    notification_promo_.start_ = now - qhour;
    notification_promo_.end_ = now + qhour;
    EXPECT_TRUE(notification_promo_.CanShow());

    // Start time has not arrived.
    notification_promo_.start_ = now + qhour;
    notification_promo_.end_ = now + qhour;
    EXPECT_FALSE(notification_promo_.CanShow());

    // End time has past.
    notification_promo_.start_ = now - qhour;
    notification_promo_.end_ = now - qhour;
    EXPECT_FALSE(notification_promo_.CanShow());

    notification_promo_.start_ = start_;
    notification_promo_.end_ = end_;
    EXPECT_TRUE(notification_promo_.CanShow());
  }

  void TestIncrement() {
    const double now = base::Time::Now().ToDoubleT();
    const double slice = 60;

    notification_promo_.num_groups_ = 18;
    notification_promo_.initial_segment_ = 5;
    notification_promo_.increment_ = 3;
    notification_promo_.time_slice_ = slice;

    notification_promo_.start_ = now - 1;
    notification_promo_.end_ = now + slice;

    // Test initial segment.
    notification_promo_.group_ = 4;
    EXPECT_TRUE(notification_promo_.CanShow());
    notification_promo_.group_ = 5;
    EXPECT_FALSE(notification_promo_.CanShow());

    // Test first increment.
    notification_promo_.start_ -= slice;
    notification_promo_.group_ = 7;
    EXPECT_TRUE(notification_promo_.CanShow());
    notification_promo_.group_ = 8;
    EXPECT_FALSE(notification_promo_.CanShow());

    // Test second increment.
    notification_promo_.start_ -= slice;
    notification_promo_.group_ = 10;
    EXPECT_TRUE(notification_promo_.CanShow());
    notification_promo_.group_ = 11;
    EXPECT_FALSE(notification_promo_.CanShow());

    // Test penultimate increment.
    notification_promo_.start_ -= 2 * slice;
    notification_promo_.group_ = 16;
    EXPECT_TRUE(notification_promo_.CanShow());
    notification_promo_.group_ = 17;
    EXPECT_FALSE(notification_promo_.CanShow());

    // Test last increment.
    notification_promo_.start_ -= slice;
    EXPECT_TRUE(notification_promo_.CanShow());
  }

  const NotificationPromo& promo() const { return notification_promo_; }

 protected:
  TestingPrefServiceSimple local_state_;

 private:
  base::MessageLoop loop_;
  NotificationPromo notification_promo_;
  bool received_notification_;
  std::unique_ptr<base::DictionaryValue> test_json_;

  NotificationPromo::PromoType promo_type_;
  std::string promo_text_;

  double start_;
  double end_;

  int num_groups_;
  int initial_segment_;
  int increment_;
  int time_slice_;
  int max_group_;

  int max_views_;

  bool closed_;
};

// Test that everything gets parsed correctly, notifications are sent,
// and CanShow() is handled correctly under variety of conditions.
// Additionally, test that the first string in |strings| is used if
// no payload.promo_short_message is specified in the JSON response.
TEST_F(NotificationPromoTest, NotificationPromoTest) {
  // Set up start date and promo line in a Dictionary as if parsed from the
  // service. date[0].end is replaced with a date 1 year in the future.
  Init(
      "{"
      "  \"ntp_notification_promo\": ["
      "    {"
      "      \"date\":"
      "        ["
      "          {"
      "            \"start\":\"3 Aug 1999 9:26:06 GMT\","
      "            \"end\":\"$1\""
      "          }"
      "        ],"
      "      \"strings\":"
      "        {"
      "          \"NTP4_HOW_DO_YOU_FEEL_ABOUT_CHROME\":"
      "              \"What do you think of Chrome?\""
      "        },"
      "      \"grouping\":"
      "        {"
      "          \"buckets\":1000,"
      "          \"segment\":200,"
      "          \"increment\":100,"
      "          \"increment_frequency\":3600,"
      "          \"increment_max\":400"
      "        },"
      "      \"payload\":"
      "        {"
      "          \"days_active\":7,"
      "          \"install_age_days\":21"
      "        },"
      "      \"max_views\":30"
      "    }"
      "  ]"
      "}",
      "What do you think of Chrome?",
      // The starting date is in 1999 to make tests pass
      // on Android devices with incorrect or unset date/time.
      933672366,  // unix epoch for 3 Aug 1999 9:26:06 GMT.
      1000, 200, 100, 3600, 400, 30);

  InitPromoFromJson(true);

  // Second time should not trigger a notification.
  InitPromoFromJson(false);

  TestInitFromPrefs();

  // Test various conditions of CanShow.
  // TestGroup Has the side effect of setting us to a passing group.
  TestGroup();
  TestViews();
  TestClosed();
  TestPromoText();
  TestTime();
  TestIncrement();
}

// Test that payload.promo_message_short is used if present.
TEST_F(NotificationPromoTest, NotificationPromoCompatNoStringsTest) {
  // Set up start date and promo line in a Dictionary as if parsed from the
  // service. date[0].end is replaced with a date 1 year in the future.
  Init(
      "{"
      "  \"ntp_notification_promo\": ["
      "    {"
      "      \"date\":"
      "        ["
      "          {"
      "            \"start\":\"3 Aug 1999 9:26:06 GMT\","
      "            \"end\":\"$1\""
      "          }"
      "        ],"
      "      \"grouping\":"
      "        {"
      "          \"buckets\":1000,"
      "          \"segment\":200,"
      "          \"increment\":100,"
      "          \"increment_frequency\":3600,"
      "          \"increment_max\":400"
      "        },"
      "      \"payload\":"
      "        {"
      "          \"promo_message_short\":"
      "              \"What do you think of Chrome?\","
      "          \"days_active\":7,"
      "          \"install_age_days\":21"
      "        },"
      "      \"max_views\":30"
      "    }"
      "  ]"
      "}",
      "What do you think of Chrome?",
      // The starting date is in 1999 to make tests pass
      // on Android devices with incorrect or unset date/time.
      933672366,  // unix epoch for 3 Aug 1999 9:26:06 GMT.
      1000, 200, 100, 3600, 400, 30);

  InitPromoFromJson(true);
  // Second time should not trigger a notification.
  InitPromoFromJson(false);
  TestInitFromPrefs();
}

// Test that strings.|payload.promo_message_short| is used if present.
TEST_F(NotificationPromoTest, NotificationPromoCompatPayloadStringsTest) {
  // Set up start date and promo line in a Dictionary as if parsed from the
  // service. date[0].end is replaced with a date 1 year in the future.
  Init(
      "{"
      "  \"ntp_notification_promo\": ["
      "    {"
      "      \"date\":"
      "        ["
      "          {"
      "            \"start\":\"3 Aug 1999 9:26:06 GMT\","
      "            \"end\":\"$1\""
      "          }"
      "        ],"
      "      \"grouping\":"
      "        {"
      "          \"buckets\":1000,"
      "          \"segment\":200,"
      "          \"increment\":100,"
      "          \"increment_frequency\":3600,"
      "          \"increment_max\":400"
      "        },"
      "      \"strings\":"
      "        {"
      "          \"bogus\":\"string\","
      "          \"GOOD_STRING\":"
      "              \"What do you think of Chrome?\""
      "        },"
      "      \"payload\":"
      "        {"
      "          \"promo_message_short\":"
      "              \"GOOD_STRING\","
      "          \"days_active\":7,"
      "          \"install_age_days\":21"
      "        },"
      "      \"max_views\":30"
      "    }"
      "  ]"
      "}",
      "What do you think of Chrome?",
      // The starting date is in 1999 to make tests pass
      // on Android devices with incorrect or unset date/time.
      933672366,  // unix epoch for 3 Aug 1999 9:26:06 GMT.
      1000, 200, 100, 3600, 400, 30);

  InitPromoFromJson(true);
  // Second time should not trigger a notification.
  InitPromoFromJson(false);
  TestInitFromPrefs();
}

TEST_F(NotificationPromoTest, PromoServerURLTest) {
  GURL promo_server_url = NotificationPromo::PromoServerURL(kChannel);
  EXPECT_FALSE(promo_server_url.is_empty());
  EXPECT_TRUE(promo_server_url.is_valid());
  EXPECT_TRUE(promo_server_url.SchemeIs(url::kHttpsScheme));
  // TODO(achuith): Test this better.
}

}  // namespace web_resource
