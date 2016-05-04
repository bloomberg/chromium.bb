// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/json/json_reader.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_entropy_provider.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "ios/chrome/browser/notification_promo.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"

namespace ios {

namespace {

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
        field_trial_list_(
            new base::FieldTrialList(new base::MockEntropyProvider())),
        received_notification_(false),
        start_(0.0),
        end_(0.0),
        promo_id_(-1),
        max_views_(0),
        max_seconds_(0),
        closed_(false) {
    NotificationPromo::RegisterPrefs(local_state_.registry());
  }
  ~NotificationPromoTest() override {
    variations::testing::ClearAllVariationParams();
  }

  void Init(const std::string& json,
            const std::string& promo_text,
            double start,
            int promo_id,
            int max_views,
            int max_seconds) {
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

    std::map<std::string, std::string> field_trial_params;
    std::string start_param;
    test_json_->GetString("start", &start_param);
    field_trial_params["start"] = start_param;
    field_trial_params["end"] = year_from_now_string;
    field_trial_params["promo_text"] = promo_text;
    field_trial_params["max_views"] = base::IntToString(max_views);
    field_trial_params["max_seconds"] = base::IntToString(max_seconds);
    field_trial_params["promo_id"] = base::IntToString(promo_id);
    // Payload parameters.
    base::DictionaryValue* payload;
    test_json_->GetDictionary("payload", &payload);
    for (base::DictionaryValue::Iterator it(*payload); !it.IsAtEnd();
         it.Advance()) {
      std::string value;
      it.value().GetAsString(&value);
      field_trial_params[it.key()] = value;
    }

    variations::AssociateVariationParams("IOSNTPPromotion", "Group1",
                                         field_trial_params);
    base::FieldTrialList::CreateFieldTrial("IOSNTPPromotion", "Group1");

    promo_type_ = NotificationPromo::MOBILE_NTP_WHATS_NEW_PROMO;
    promo_text_ = promo_text;

    start_ = start;
    end_ = year_from_now_epoch;

    promo_id_ = promo_id;
    max_views_ = max_views;
    max_seconds_ = max_seconds;

    closed_ = false;
    received_notification_ = false;
  }

  void InitPromoFromVariations(bool should_receive_notification) {
    notification_promo_.InitFromVariations();
    EXPECT_EQ(should_receive_notification,
              notification_promo_.new_notification_);

    // Test the fields.
    TestNotification();
  }

  void InitPromoFromJson(bool should_receive_notification) {
    notification_promo_.InitFromJson(*test_json_, promo_type_);
    EXPECT_EQ(should_receive_notification,
              notification_promo_.new_notification_);

    // Test the fields.
    TestNotification();
  }

  void TestNotification() {
    // Check values.
    EXPECT_EQ(notification_promo_.promo_text_, promo_text_);

    EXPECT_EQ(notification_promo_.start_, start_);
    EXPECT_EQ(notification_promo_.end_, end_);

    EXPECT_EQ(notification_promo_.promo_id_, promo_id_);
    EXPECT_EQ(notification_promo_.max_views_, max_views_);
    EXPECT_EQ(notification_promo_.max_seconds_, max_seconds_);
    EXPECT_EQ(notification_promo_.closed_, closed_);

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
    EXPECT_EQ(notification_promo_.promo_id_,
              prefs_notification_promo.promo_id_);
    EXPECT_EQ(notification_promo_.max_views_,
              prefs_notification_promo.max_views_);
    EXPECT_EQ(notification_promo_.max_seconds_,
              prefs_notification_promo.max_seconds_);
    EXPECT_EQ(notification_promo_.views_, prefs_notification_promo.views_);
    EXPECT_EQ(notification_promo_.closed_, prefs_notification_promo.closed_);
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

  void TestMaxTime() {
    const double now = base::Time::Now().ToDoubleT();
    const double margin = 60;

    // Current time is before the |first_view_time_| + |max_seconds_|.
    notification_promo_.first_view_time_ = now - margin;
    notification_promo_.max_seconds_ = margin + 1;
    EXPECT_TRUE(notification_promo_.CanShow());

    // Current time as after the |first_view_time_| + |max_seconds_|.
    notification_promo_.first_view_time_ = now - margin;
    notification_promo_.max_seconds_ = margin - 1;
    EXPECT_FALSE(notification_promo_.CanShow());

    notification_promo_.first_view_time_ = 0;
    notification_promo_.max_seconds_ = max_seconds_;
    EXPECT_TRUE(notification_promo_.CanShow());
  }

  const NotificationPromo& promo() const { return notification_promo_; }

 protected:
  TestingPrefServiceSimple local_state_;

 private:
  NotificationPromo notification_promo_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  bool received_notification_;
  std::unique_ptr<base::DictionaryValue> test_json_;

  NotificationPromo::PromoType promo_type_;
  std::string promo_text_;

  double start_;
  double end_;
  int promo_id_;
  int max_views_;
  int max_seconds_;

  bool closed_;
};

// Test that everything gets parsed correctly, notifications are sent,
// and CanShow() is handled correctly under variety of conditions.
TEST_F(NotificationPromoTest, NotificationPromoJSONTest) {
  Init(
      "{"
      "  \"start\":\"3 Aug 1999 9:26:06 GMT\","
      "  \"end\":\"$1\","
      "  \"promo_text\":\"What do you think of Chrome?\","
      "  \"payload\":"
      "    {"
      "      \"days_active\":7,"
      "      \"install_age_days\":21"
      "    },"
      "  \"max_views\":30,"
      "  \"max_seconds\":30,"
      "  \"promo_id\":0"
      "}",
      "What do you think of Chrome?",
      933672366,  // unix epoch for 3 Aug 1999 9:26:06 GMT.
      0, 30, 30);

  InitPromoFromJson(true);

  // Second time should not trigger a notification.
  InitPromoFromJson(false);

  TestInitFromPrefs();

  // Test various conditions of CanShow.
  TestViews();
  TestClosed();
  TestPromoText();
  TestTime();
  TestMaxTime();
}

TEST_F(NotificationPromoTest, NotificationPromoFinchTest) {
  Init(
      "{"
      "  \"start\":\"3 Aug 1999 9:26:06 GMT\","
      "  \"end\":\"$1\","
      "  \"promo_text\":\"What do you think of Chrome?\","
      "  \"payload\":"
      "    {"
      "      \"days_active\":7,"
      "      \"install_age_days\":21"
      "    },"
      "  \"max_views\":30,"
      "  \"max_seconds\":30,"
      "  \"promo_id\":0"
      "}",
      "What do you think of Chrome?",
      933672366,  // unix epoch for 3 Aug 1999 9:26:06 GMT.
      0, 30, 30);

  InitPromoFromVariations(true);

  // Second time should not trigger a notification.
  InitPromoFromVariations(false);

  TestInitFromPrefs();

  // Test various conditions of CanShow.
  TestViews();
  TestClosed();
  TestPromoText();
  TestTime();
  TestMaxTime();
}

}  // namespace ios
