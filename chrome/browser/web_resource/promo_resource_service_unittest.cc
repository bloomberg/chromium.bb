// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/web_resource/notification_promo.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

class PromoResourceServiceTest : public testing::Test {
 public:
  PromoResourceServiceTest()
      : local_state_(static_cast<TestingBrowserProcess*>(g_browser_process)),
        web_resource_service_(new PromoResourceService(&profile_)) {
  }

 protected:
  TestingProfile profile_;
  ScopedTestingLocalState local_state_;
  scoped_refptr<PromoResourceService> web_resource_service_;
  MessageLoop loop_;
};

class NotificationPromoTest {
 public:
  explicit NotificationPromoTest(Profile* profile)
      : profile_(profile),
        prefs_(profile->GetPrefs()),
        notification_promo_(profile),
        received_notification_(false),
        start_(0.0),
        end_(0.0),
        num_groups_(0),
        initial_segment_(0),
        increment_(1),
        time_slice_(0),
        max_group_(0),
        max_views_(0),
        closed_(false),
        gplus_required_(false) {
  }

  void Init(const std::string& json,
            const std::string& promo_text,
            double start, double end,
            int num_groups, int initial_segment, int increment,
            int time_slice, int max_group, int max_views,
            bool gplus_required) {
    Value* value(base::JSONReader::Read(json));
    ASSERT_TRUE(value);
    DictionaryValue* dict = NULL;
    value->GetAsDictionary(&dict);
    ASSERT_TRUE(dict);
    test_json_.reset(dict);

    promo_text_ = promo_text;

    start_ = start;
    end_ = end;

    num_groups_ = num_groups;
    initial_segment_ = initial_segment;
    increment_ = increment;
    time_slice_ = time_slice;
    max_group_ = max_group;

    max_views_ = max_views;

    gplus_required_ = gplus_required;

    closed_ = false;
    received_notification_ = false;
  }

  void InitPromoFromJson(bool should_receive_notification) {
    notification_promo_.InitFromJson(*test_json_);
    EXPECT_EQ(should_receive_notification,
              notification_promo_.new_notification());

    // Test the fields.
    TestNotification();
    TestPrefs();
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

    EXPECT_EQ(notification_promo_.gplus_required_, gplus_required_);
  }

  void TestPrefs() {
    EXPECT_EQ(prefs_->GetString(prefs::kNtpPromoLine), promo_text_);

    EXPECT_EQ(prefs_->GetDouble(prefs::kNtpPromoStart), start_);
    EXPECT_EQ(prefs_->GetDouble(prefs::kNtpPromoEnd), end_);

    EXPECT_EQ(prefs_->GetInteger(prefs::kNtpPromoNumGroups), num_groups_);
    EXPECT_EQ(prefs_->GetInteger(prefs::kNtpPromoInitialSegment),
                                 initial_segment_);
    EXPECT_EQ(prefs_->GetInteger(prefs::kNtpPromoIncrement), increment_);
    EXPECT_EQ(prefs_->GetInteger(prefs::kNtpPromoGroupTimeSlice), time_slice_);
    EXPECT_EQ(prefs_->GetInteger(prefs::kNtpPromoGroupMax), max_group_);

    EXPECT_EQ(prefs_->GetInteger(prefs::kNtpPromoViewsMax), max_views_);
    EXPECT_EQ(prefs_->GetBoolean(prefs::kNtpPromoClosed), closed_);

    EXPECT_EQ(prefs_->GetInteger(prefs::kNtpPromoGroup),
              notification_promo_.group_);
    EXPECT_EQ(prefs_->GetInteger(prefs::kNtpPromoViews), 0);

    EXPECT_EQ(prefs_->GetBoolean(prefs::kNtpPromoGplusRequired),
              gplus_required_);
  }

  // Create a new NotificationPromo from prefs and compare to current
  // notification.
  void TestInitFromPrefs() {
    NotificationPromo prefs_notification_promo(profile_);
    prefs_notification_promo.InitFromPrefs();

    EXPECT_EQ(notification_promo_.prefs_,
              prefs_notification_promo.prefs_);
    EXPECT_EQ(notification_promo_.promo_text_,
              prefs_notification_promo.promo_text_);
    EXPECT_EQ(notification_promo_.start_,
              prefs_notification_promo.start_);
    EXPECT_EQ(notification_promo_.end_,
              prefs_notification_promo.end_);
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
    EXPECT_EQ(notification_promo_.group_,
              prefs_notification_promo.group_);
    EXPECT_EQ(notification_promo_.views_,
              prefs_notification_promo.views_);
    EXPECT_EQ(notification_promo_.closed_,
              prefs_notification_promo.closed_);
    EXPECT_EQ(notification_promo_.gplus_required_,
              prefs_notification_promo.gplus_required_);
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
  }

  void TestViews() {
    // Test out of range views.
    for (int i = max_views_; i < max_views_ * 2; ++i) {
      notification_promo_.views_ = i;
      EXPECT_FALSE(notification_promo_.CanShow());
    }

    // Test in range views.
    for (int i = 0; i < max_views_; ++i) {
      notification_promo_.views_ = i;
      EXPECT_TRUE(notification_promo_.CanShow());
    }
  }

  void TestClosed() {
    notification_promo_.closed_ = true;
    EXPECT_FALSE(notification_promo_.CanShow());

    notification_promo_.closed_ = false;
    EXPECT_TRUE(notification_promo_.CanShow());
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

  void TestGplus() {
    notification_promo_.gplus_required_ = true;

    // Test G+ required.
    notification_promo_.prefs_->SetBoolean(prefs::kIsGooglePlusUser, true);
    EXPECT_TRUE(notification_promo_.CanShow());
    notification_promo_.prefs_->SetBoolean(prefs::kIsGooglePlusUser, false);
    EXPECT_FALSE(notification_promo_.CanShow());

    notification_promo_.gplus_required_ = false;

    // Test G+ not required.
    notification_promo_.prefs_->SetBoolean(prefs::kIsGooglePlusUser, true);
    EXPECT_TRUE(notification_promo_.CanShow());
    notification_promo_.prefs_->SetBoolean(prefs::kIsGooglePlusUser, false);
    EXPECT_TRUE(notification_promo_.CanShow());
  }

 private:
  Profile* profile_;
  PrefService* prefs_;
  NotificationPromo notification_promo_;
  bool received_notification_;
  scoped_ptr<DictionaryValue> test_json_;

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

  bool gplus_required_;
};

TEST_F(PromoResourceServiceTest, NotificationPromoTest) {
  // Check that prefs are set correctly.
  PrefService* prefs = profile_.GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  NotificationPromoTest promo_test(&profile_);

  // Make sure prefs are unset.
  promo_test.TestPrefs();

  // Set up start and end dates and promo line in a Dictionary as if parsed
  // from the service.
  promo_test.Init("{"
                  "  \"ntp_notification_promo\":"
                  "    {"
                  "      \"date\":"
                  "        ["
                  "          {"
                  "            \"start\":\"15 Jan 2012 10:50:85 PST\","
                  "            \"end\":\"7 Jan 2013 5:40:75 PST\""
                  "          }"
                  "        ],"
                  "      \"strings\":"
                  "        {"
                  "          \"NTP4_HOW_DO_YOU_FEEL_ABOUT_CHROME\":"
                  "          \"What do you think of Chrome?\""
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
                  "          \"install_age_days\":21,"
                  "          \"gplus_required\":false"
                  "        },"
                  "      \"max_views\":30"
                  "    }"
                  "}",
                  "What do you think of Chrome?",
                  1326653485,  // unix epoch for 15 Jan 2012 10:50:85 PST.
                  1357566075,  // unix epoch for 7 Jan 2013 5:40:75 PST.
                  1000, 200, 100, 3600, 400, 30, false);

  promo_test.InitPromoFromJson(true);

  // Second time should not trigger a notification.
  promo_test.InitPromoFromJson(false);

  promo_test.TestInitFromPrefs();

  // Test various conditions of CanShow.
  // TestGroup Has the side effect of setting us to a passing group.
  promo_test.TestGroup();
  promo_test.TestViews();
  promo_test.TestClosed();
  promo_test.TestPromoText();
  promo_test.TestTime();
  promo_test.TestIncrement();
  promo_test.TestGplus();
}

TEST_F(PromoResourceServiceTest, PromoServerURLTest) {
  GURL promo_server_url = NotificationPromo::PromoServerURL();
  EXPECT_FALSE(promo_server_url.is_empty());
  EXPECT_TRUE(promo_server_url.SchemeIs("https"));
  // TODO(achuith): Test this better.
}
