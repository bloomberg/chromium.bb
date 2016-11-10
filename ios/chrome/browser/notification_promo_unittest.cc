// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
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
        field_trial_list_(base::MakeUnique<base::FieldTrialList>(
            base::MakeUnique<base::MockEntropyProvider>())),
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

  void InitPromoFromVariations() {
    notification_promo_.InitFromVariations();

    // Test the fields.
    TestServerProvidedParameters();
  }

  void InitPromoFromJson() {
    notification_promo_.InitFromJson(*test_json_, promo_type_);

    // Test the fields.
    TestServerProvidedParameters();
  }

  void TestServerProvidedParameters() {
    // Check values.
    EXPECT_EQ(notification_promo_.promo_text_, promo_text_);

    EXPECT_DOUBLE_EQ(notification_promo_.start_, start_);
    EXPECT_DOUBLE_EQ(notification_promo_.end_, end_);

    EXPECT_EQ(notification_promo_.promo_id_, promo_id_);
    EXPECT_EQ(notification_promo_.max_views_, max_views_);
    EXPECT_EQ(notification_promo_.max_seconds_, max_seconds_);
  }

  void TestViews() {
    notification_promo_.views_ = notification_promo_.max_views_ - 2;
    notification_promo_.WritePrefs();

    // Initialize promo from saved prefs and server params.
    NotificationPromo first_promo(&local_state_);
    first_promo.InitFromVariations();
    first_promo.InitFromPrefs(promo_type_);
    EXPECT_EQ(first_promo.max_views_ - 2, first_promo.views_);
    EXPECT_TRUE(first_promo.CanShow());
    first_promo.HandleViewed();

    // Initialize another promo to test that the new views were recorded
    // correctly in prefs.
    NotificationPromo second_promo(&local_state_);
    second_promo.InitFromVariations();
    second_promo.InitFromPrefs(promo_type_);
    EXPECT_EQ(second_promo.max_views_ - 1, second_promo.views_);
    EXPECT_TRUE(second_promo.CanShow());
    second_promo.HandleViewed();

    NotificationPromo third_promo(&local_state_);
    third_promo.InitFromVariations();
    third_promo.InitFromPrefs(promo_type_);
    EXPECT_EQ(third_promo.max_views_, third_promo.views_);
    EXPECT_FALSE(third_promo.CanShow());

    // Test out of range views.
    for (int i = max_views_; i < max_views_ * 2; ++i) {
      third_promo.views_ = i;
      EXPECT_FALSE(third_promo.CanShow());
    }

    // Test in range views.
    for (int i = 0; i < max_views_; ++i) {
      third_promo.views_ = i;
      EXPECT_TRUE(third_promo.CanShow());
    }

    // Reset prefs to default.
    notification_promo_.views_ = 0;
    notification_promo_.WritePrefs();
  }

  void TestClosed() {
    // Initialize promo from saved prefs and server params.
    NotificationPromo first_promo(&local_state_);
    first_promo.InitFromVariations();
    first_promo.InitFromPrefs(promo_type_);
    EXPECT_FALSE(first_promo.closed_);
    EXPECT_TRUE(first_promo.CanShow());
    first_promo.HandleClosed();

    // Initialize another promo to test that the the closing of the promo was
    // recorded correctly in prefs.
    NotificationPromo second_promo(&local_state_);
    second_promo.InitFromVariations();
    second_promo.InitFromPrefs(promo_type_);
    EXPECT_TRUE(second_promo.closed_);
    EXPECT_FALSE(second_promo.CanShow());

    // Reset prefs to default.
    second_promo.closed_ = false;
    EXPECT_TRUE(second_promo.CanShow());
    second_promo.WritePrefs();
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

  // Tests that the first view time is recorded properly in prefs when the
  // first view occurs.
  void TestFirstViewTimeRecorded() {
    EXPECT_DOUBLE_EQ(0, notification_promo_.first_view_time_);
    notification_promo_.HandleViewed();

    NotificationPromo temp_promo(&local_state_);
    temp_promo.InitFromVariations();
    temp_promo.InitFromPrefs(promo_type_);
    EXPECT_NE(0, temp_promo.first_view_time_);

    notification_promo_.views_ = 0;
    notification_promo_.first_view_time_ = 0;
    notification_promo_.WritePrefs();
  }

  // Tests that if data is saved in the old pref structure, it is successfully
  // migrated to the new structure that supports saving multiple promos.
  // TODO(crbug.com/623726) Remove this method when migration is no longer
  // needed as most users have been upgraded to the new pref structure.
  void TestMigrationOfOldPrefs() {
    NotificationPromo promo(&local_state_);
    promo.InitFromVariations();

    // Pick values for each variable that is saved into old prefs structure.
    double first_view_time = 2.0;
    int views = max_views_ + 1;
    bool closed = true;

    // Save data into old prefs structure.
    base::DictionaryValue* ntp_promo = new base::DictionaryValue;
    ntp_promo->SetInteger("id", promo.promo_id_);
    ntp_promo->SetDouble("first_view_time", first_view_time);
    ntp_promo->SetInteger("views", views);
    ntp_promo->SetBoolean("closed", true);

    base::ListValue* promo_list = new base::ListValue;
    promo_list->Set(0, ntp_promo);

    std::string promo_list_key = "mobile_ntp_whats_new_promo";
    std::string promo_dict_key = "ios.ntppromo";

    base::DictionaryValue promo_dict;
    promo_dict.Set(promo_list_key, promo_list);
    local_state_.Set(promo_dict_key, promo_dict);

    // Initialize promo and verify that its instance variables match the data
    // saved in the old structure.
    promo.InitFromPrefs(promo_type_);
    EXPECT_DOUBLE_EQ(first_view_time, promo.first_view_time_);
    EXPECT_EQ(views, promo.views_);
    EXPECT_EQ(closed, promo.closed_);
    EXPECT_FALSE(promo.CanShow());

    // Verify that old pref structure was cleared.
    const base::DictionaryValue* current_promo_dict =
        local_state_.GetDictionary(promo_dict_key);
    // Do not continue further if no dictionary was found at the key in prefs.
    ASSERT_TRUE(current_promo_dict);
    const base::ListValue* current_promo_list = NULL;
    current_promo_dict->GetList(promo_list_key, &current_promo_list);
    EXPECT_FALSE(current_promo_list);
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

  InitPromoFromJson();

  // Test various conditions of CanShow.
  TestViews();
  TestClosed();
  TestPromoText();
  TestTime();
  TestMaxTime();

  TestFirstViewTimeRecorded();
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

  InitPromoFromVariations();

  // Test various conditions of CanShow.
  TestViews();
  TestClosed();
  TestPromoText();
  TestTime();
  TestMaxTime();

  TestFirstViewTimeRecorded();
}

// TODO(crbug.com/623726) Remove this test case when migration is no longer
// needed as most users have been upgraded to the new pref structure.
TEST_F(NotificationPromoTest, NotificationPromoPrefMigrationTest) {
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
  InitPromoFromVariations();
  TestMigrationOfOldPrefs();
}

}  // namespace ios
