// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/apps_promo.h"
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
#include "content/test/test_url_fetcher_factory.h"
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

class NTPSignInPromoTest : public PromoResourceServiceTest,
                      public content::NotificationObserver {
 public:
  NTPSignInPromoTest() : PromoResourceServiceTest(),
                         notifications_received_(0) {
    web_resource_service_->set_channel(chrome::VersionInfo::CHANNEL_DEV);
    registrar_.Add(this,
                   chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED,
                   content::NotificationService::AllSources());
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    EXPECT_EQ(type, chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED);
    // Remember how many notifications we've received.
    ++notifications_received_;
  }

  void reset_notification_count() {
    notifications_received_ = 0;
  }

  int notifications_received() const {
    return notifications_received_;
  }

 protected:
  void ClearNTPSignInPromoPrefs() {
    PrefService* prefs = profile_.GetPrefs();
    prefs->ClearPref(prefs::kNTPSignInPromoGroup);
    prefs->ClearPref(prefs::kNTPSignInPromoGroupMax);
    ASSERT_FALSE(prefs->HasPrefPath(prefs::kNTPSignInPromoGroup));
    ASSERT_FALSE(prefs->HasPrefPath(prefs::kNTPSignInPromoGroupMax));
  }

  void InvalidTestCase(const std::string& question) {
    PrefService* prefs = profile_.GetPrefs();
    ASSERT_TRUE(prefs != NULL);
    prefs->SetInteger(prefs::kNTPSignInPromoGroup, 50);
    prefs->SetInteger(prefs::kNTPSignInPromoGroupMax, 75);
    UnpackNTPSignInPromo(question);
    EXPECT_FALSE(prefs->HasPrefPath(prefs::kNTPSignInPromoGroup));
    EXPECT_FALSE(prefs->HasPrefPath(prefs::kNTPSignInPromoGroupMax));
  }

  void SetupNTPSignInPromoCase(int build, int max_group) {
    std::string question = base::IntToString(build) + ":" +
                           base::IntToString(max_group);
    UnpackNTPSignInPromo(question);
  }

  void UnpackNTPSignInPromo(const std::string& question) {
    std::string json_header =
      "{ "
      "  \"topic\": {"
      "    \"answers\": ["
      "       {"
      "        \"name\": \"sign_in_promo\","
      "        \"question\": \"";

    std::string json_footer = "\""
      "       }"
      "    ]"
      "  }"
      "}";

    scoped_ptr<DictionaryValue> test_json(static_cast<DictionaryValue*>(
        base::JSONReader::Read(json_header + question + json_footer, false)));
    web_resource_service_->UnpackNTPSignInPromoSignal(*(test_json.get()));
  }

  private:
    int notifications_received_;
    content::NotificationRegistrar registrar_;
};

// Verifies that custom dates read from a web resource server are written to
// the preferences file.
TEST_F(PromoResourceServiceTest, UnpackLogoSignal) {
  // Set up start and end dates in a Dictionary as if parsed from the service.
  std::string json = "{ "
                     "  \"topic\": {"
                     "    \"answers\": ["
                     "       {"
                     "        \"name\": \"custom_logo_start\","
                     "        \"inproduct\": \"31/01/10 01:00 GMT\""
                     "       },"
                     "       {"
                     "        \"name\": \"custom_logo_end\","
                     "        \"inproduct\": \"31/01/12 01:00 GMT\""
                     "       }"
                     "    ]"
                     "  }"
                     "}";
  scoped_ptr<DictionaryValue> test_json(
      static_cast<DictionaryValue*>(base::JSONReader::Read(json, false)));

  // Check that prefs are set correctly.
  web_resource_service_->UnpackLogoSignal(*(test_json.get()));
  PrefService* prefs = profile_.GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  double logo_start =
      prefs->GetDouble(prefs::kNTPCustomLogoStart);
  EXPECT_EQ(logo_start, 1264899600);  // unix epoch for Jan 31 2010 0100 GMT.
  double logo_end =
      prefs->GetDouble(prefs::kNTPCustomLogoEnd);
  EXPECT_EQ(logo_end, 1327971600);  // unix epoch for Jan 31 2012 0100 GMT.

  // Change the start only and recheck.
  json = "{ "
         "  \"topic\": {"
         "    \"answers\": ["
         "       {"
         "        \"name\": \"custom_logo_start\","
         "        \"inproduct\": \"28/02/10 14:00 GMT\""
         "       },"
         "       {"
         "        \"name\": \"custom_logo_end\","
         "        \"inproduct\": \"31/01/12 01:00 GMT\""
         "       }"
         "    ]"
         "  }"
         "}";
  test_json->Clear();
  test_json.reset(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  // Check that prefs are set correctly.
  web_resource_service_->UnpackLogoSignal(*(test_json.get()));

  logo_start = prefs->GetDouble(prefs::kNTPCustomLogoStart);
  EXPECT_EQ(logo_start, 1267365600);  // date changes to Feb 28 2010 1400 GMT.

  // If no date is included in the prefs, reset custom logo dates to 0.
  json = "{ "
         "  \"topic\": {"
         "    \"answers\": ["
         "       {"
         "       }"
         "    ]"
         "  }"
         "}";
  test_json->Clear();
  test_json.reset(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  // Check that prefs are set correctly.
  web_resource_service_->UnpackLogoSignal(*(test_json.get()));
  logo_start = prefs->GetDouble(prefs::kNTPCustomLogoStart);
  EXPECT_EQ(logo_start, 0);  // date value reset to 0;
  logo_end = prefs->GetDouble(prefs::kNTPCustomLogoEnd);
  EXPECT_EQ(logo_end, 0);  // date value reset to 0;
}

class NotificationPromoTestDelegate : public NotificationPromo::Delegate {
 public:
  explicit NotificationPromoTestDelegate(Profile* profile)
      : profile_(profile),
        prefs_(profile->GetPrefs()),
        notification_promo_(NULL),
        received_notification_(false),
        should_receive_notification_(false),
        build_targeted_(true),
        start_(0.0),
        end_(0.0),
        build_(PromoResourceService::NO_BUILD),
        time_slice_(0),
        max_group_(0),
        max_views_(0),
        platform_(NotificationPromo::PLATFORM_NONE),
        feature_mask_(0),
        text_(),
        closed_(false),
        gplus_(false),
        current_platform_(NotificationPromo::CurrentPlatform()) {
  }

  void Init(NotificationPromo* notification_promo,
            const std::string& json,
            double start, double end,
            int build, int time_slice,
            int max_group, int max_views, int platform,
            int feature_mask, const std::string& text, bool closed,
            bool gplus) {
    notification_promo_ = notification_promo;

    test_json_.reset(static_cast<DictionaryValue*>(
        base::JSONReader::Read(json, false)));

    start_ = start;
    end_ = end;

    build_ = build;
    time_slice_ = time_slice;
    max_group_ = max_group;
    max_views_ = max_views;
    platform_ = platform;

    text_ = text;
    closed_ = closed;
    gplus_ = gplus;
    feature_mask_ = feature_mask;

    received_notification_ = false;
  }

  // NotificationPromo::Delegate implementation.
  virtual void OnNotificationParsed(double start, double end,
                                    bool new_notification) {
    if (should_receive_notification_) {
      EXPECT_EQ(CalcStart(), start);
      EXPECT_EQ(notification_promo_->StartTimeWithOffset(), start);
      EXPECT_EQ(notification_promo_->end_, end);
    }

    received_notification_ = new_notification;
    EXPECT_TRUE(received_notification_ == should_receive_notification_);
  }

  virtual bool IsBuildAllowed(int builds_targeted) const {
    EXPECT_EQ(builds_targeted, build_);
    return build_targeted_;
  }

  virtual int CurrentPlatform() const {
    return current_platform_;
  }

  const base::DictionaryValue& TestJson() const {
    return *test_json_;
  }

  double CalcStart() const {
    return start_ + notification_promo_->group_ * time_slice_ * 60.0 * 60.0;
  }

  void InitPromoFromJson(bool should_receive_notification) {
    should_receive_notification_ = should_receive_notification;
    received_notification_ = false;
    notification_promo_->InitFromJson(TestJson(), false);
    EXPECT_TRUE(received_notification_ == should_receive_notification);

    // Test the fields.
    TestNotification();
    TestPrefs();
  }

  void TestCookie(const std::string& cookies,
      bool should_receive_notification, bool should_find_cookie) {
    gplus_ = should_find_cookie;
    should_receive_notification_ = should_receive_notification;
    received_notification_ = false;

    bool found_cookie = NotificationPromo::CheckForGPlusCookie(cookies);
    EXPECT_TRUE(found_cookie == should_find_cookie);

    notification_promo_->CheckForNewNotification(found_cookie);
    EXPECT_TRUE(received_notification_ == should_receive_notification);

    // Test the fields.
    EXPECT_EQ(notification_promo_->gplus_, gplus_);
    EXPECT_EQ(notification_promo_->feature_mask_, feature_mask_);
    // Test the prefs.
    EXPECT_EQ(prefs_->GetBoolean(prefs::kNTPPromoIsLoggedInToPlus), gplus_);
    EXPECT_EQ(prefs_->GetInteger(prefs::kNTPPromoFeatureMask), feature_mask_);

    // Set group_ manually to a passing group.
    notification_promo_->group_ = max_group_ - 1;
    // Assumes feature_mask = GPLUS_FEATURE, so whether or not the promo is
    // is shown depends only on the value of gplus_.
    EXPECT_TRUE(notification_promo_->CanShow() == gplus_);
  }

  void TestGPlus() {
    feature_mask_ = NotificationPromo::FEATURE_GPLUS;
    notification_promo_->feature_mask_ = NotificationPromo::FEATURE_GPLUS;
    // Force a notification when gplus_ is found to be false.
    notification_promo_->prefs_->
        SetBoolean(prefs::kNTPPromoIsLoggedInToPlus, true);

    TestCookie("WRONG=123456;", true, false);
    // Should not trigger notification on second call.
    TestCookie("WRONG=123456;", false, false);

    TestCookie("SID=123456;", true, true);
    // Should not trigger notification on second call.
    TestCookie("SID=123456;", false, true);

    // Reset the notification_promo to its original state.
    feature_mask_ = NotificationPromo::NO_FEATURE;
    notification_promo_->feature_mask_ = NotificationPromo::NO_FEATURE;
    gplus_ = false;
    notification_promo_->prefs_->
        SetBoolean(prefs::kNTPPromoIsLoggedInToPlus, false);
    notification_promo_->gplus_ = false;
  }

  void TestNotification() {
    // Check values.
    EXPECT_EQ(notification_promo_->start_, start_);
    EXPECT_EQ(notification_promo_->end_, end_);
    EXPECT_EQ(notification_promo_->build_, build_);
    EXPECT_EQ(notification_promo_->time_slice_, time_slice_);
    EXPECT_EQ(notification_promo_->max_group_, max_group_);
    EXPECT_EQ(notification_promo_->max_views_, max_views_);
    EXPECT_EQ(notification_promo_->platform_, platform_);
    EXPECT_EQ(notification_promo_->text_, text_);
    EXPECT_EQ(notification_promo_->closed_, closed_);
    EXPECT_EQ(notification_promo_->gplus_, gplus_);
    EXPECT_EQ(notification_promo_->feature_mask_, feature_mask_);

    // Check group within bounds.
    EXPECT_GE(notification_promo_->group_, 0);
    EXPECT_LT(notification_promo_->group_, 100);

    // Views should be 0 for now.
    EXPECT_EQ(notification_promo_->views_, 0);

    // Check calculated time.
    EXPECT_EQ(notification_promo_->StartTimeWithOffset(), CalcStart());
  }

  void TestPrefs() {
    EXPECT_EQ(prefs_->GetDouble(prefs::kNTPPromoStart), start_);
    EXPECT_EQ(prefs_->GetDouble(prefs::kNTPPromoEnd), end_);

    EXPECT_EQ(prefs_->GetInteger(prefs::kNTPPromoBuild), build_);
    EXPECT_EQ(prefs_->GetInteger(prefs::kNTPPromoGroupTimeSlice), time_slice_);
    EXPECT_EQ(prefs_->GetInteger(prefs::kNTPPromoGroupMax), max_group_);
    EXPECT_EQ(prefs_->GetInteger(prefs::kNTPPromoViewsMax), max_views_);
    EXPECT_EQ(prefs_->GetInteger(prefs::kNTPPromoPlatform), platform_);

    EXPECT_EQ(prefs_->GetInteger(prefs::kNTPPromoGroup),
        notification_promo_ ? notification_promo_->group_: 0);
    EXPECT_EQ(prefs_->GetInteger(prefs::kNTPPromoViews), 0);
    EXPECT_EQ(prefs_->GetString(prefs::kNTPPromoLine), text_);
    EXPECT_EQ(prefs_->GetBoolean(prefs::kNTPPromoClosed), closed_);
    EXPECT_EQ(prefs_->GetBoolean(prefs::kNTPPromoIsLoggedInToPlus), gplus_);
    EXPECT_EQ(prefs_->GetInteger(prefs::kNTPPromoFeatureMask), feature_mask_);
  }

  // Create a new NotificationPromo from prefs and compare to current
  // notification.
  void TestInitFromPrefs() {
    scoped_refptr<NotificationPromo> prefs_notification_promo =
    NotificationPromo::Create(profile_, this);
    prefs_notification_promo->InitFromPrefs();
    const bool is_equal = *notification_promo_ == *prefs_notification_promo;
    EXPECT_TRUE(is_equal);
  }

  void TestGroup() {
    // Test out of range groups.
    for (int i = max_group_; i <= NotificationPromo::kMaxGroupSize; ++i) {
      notification_promo_->group_ = i;
      EXPECT_FALSE(notification_promo_->CanShow());
    }

    // Test in-range groups.
    for (int i = 0; i < max_group_; ++i) {
      notification_promo_->group_ = i;
      EXPECT_TRUE(notification_promo_->CanShow());
    }
  }

  void TestViews() {
    // Test out of range views.
    for (int i = max_views_; i < 100; ++i) {
      notification_promo_->views_ = i;
      EXPECT_FALSE(notification_promo_->CanShow());
    }

    // Test in range views.
    for (int i = 0; i < max_views_; ++i) {
      notification_promo_->views_ = i;
      EXPECT_TRUE(notification_promo_->CanShow());
    }
  }

  void TestBuild() {
    build_targeted_ = false;
    EXPECT_FALSE(notification_promo_->CanShow());

    build_targeted_ = true;
    EXPECT_TRUE(notification_promo_->CanShow());
  }

  void TestOnePlatform(int current_platform, bool expected) {
    current_platform_ = current_platform;
    EXPECT_EQ(expected, notification_promo_->CanShow());
  }

  void TestPlatformSet(int target_platform, bool expected_win,
      bool expected_mac, bool expected_linux, bool expected_chromeos) {
    notification_promo_->platform_ = target_platform;
    TestOnePlatform(NotificationPromo::PLATFORM_WIN, expected_win);
    TestOnePlatform(NotificationPromo::PLATFORM_MAC, expected_mac);
    TestOnePlatform(NotificationPromo::PLATFORM_LINUX, expected_linux);
    TestOnePlatform(NotificationPromo::PLATFORM_CHROMEOS, expected_chromeos);
  }

  void TestPlatforms() {
    int target_platform;

    // Windows and Mac only - test real current platform.
    target_platform = 3;
    notification_promo_->platform_ = target_platform;
    bool expected = false;
#if defined(OS_WIN) || defined(OS_MACOSX)
    expected = true;
#endif
    EXPECT_EQ(expected, notification_promo_->CanShow());

    // Windows only.
    target_platform = 1;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_WIN);
    TestPlatformSet(target_platform, true, false, false, false);

    // Mac only.
    target_platform = 2;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_MAC);
    TestPlatformSet(target_platform, false, true, false, false);

    // Linux only.
    target_platform = 4;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_LINUX);
    TestPlatformSet(target_platform, false, false, true, false);

    // ChromeOS only.
    target_platform = 8;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_CHROMEOS);
    TestPlatformSet(target_platform, false, false, false, true);

    // Non-Windows.
    target_platform = 14;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_ALL
                               & ~NotificationPromo::PLATFORM_WIN);
    TestPlatformSet(target_platform, false, true, true, true);

    // Non-Mac.
    target_platform = 13;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_ALL
                               & ~NotificationPromo::PLATFORM_MAC);
    TestPlatformSet(target_platform, true, false, true, true);

    // Non-Linux.
    target_platform = 11;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_ALL
                               & ~NotificationPromo::PLATFORM_LINUX);
    TestPlatformSet(target_platform, true, true, false, true);

    // Non-ChromeOS.
    target_platform = 7;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_ALL
                               & ~NotificationPromo::PLATFORM_CHROMEOS);
    TestPlatformSet(target_platform, true, true, true, false);

    // Windows and Mac.
    target_platform = 3;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_WIN
                               | NotificationPromo::PLATFORM_MAC);
    TestPlatformSet(target_platform, true, true, false, false);

    // Windows and Linux.
    target_platform = 5;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_WIN
                               | NotificationPromo::PLATFORM_LINUX);
    TestPlatformSet(target_platform, true, false, true, false);

    // Windows and ChromeOS.
    target_platform = 9;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_WIN
                               | NotificationPromo::PLATFORM_CHROMEOS);
    TestPlatformSet(target_platform, true, false, false, true);

    // Mac and Linux.
    target_platform = 6;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_MAC
                               | NotificationPromo::PLATFORM_LINUX);
    TestPlatformSet(target_platform, false, true, true, false);

    // Mac and ChromeOS.
    target_platform = 10;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_MAC
                               | NotificationPromo::PLATFORM_CHROMEOS);
    TestPlatformSet(target_platform, false, true, false, true);

    // Linux and ChromeOS.
    target_platform = 12;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_LINUX
                               | NotificationPromo::PLATFORM_CHROMEOS);
    TestPlatformSet(target_platform, false, false, true, true);

    // Disabled.
    target_platform = 0;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_NONE);
    TestPlatformSet(target_platform, false, false, false, false);

    // All platforms.
    target_platform = 15;
    EXPECT_EQ(target_platform, NotificationPromo::PLATFORM_ALL);
    TestPlatformSet(target_platform, true, true, true, true);
  }

  void TestClosed() {
    notification_promo_->closed_ = true;
    EXPECT_FALSE(notification_promo_->CanShow());

    notification_promo_->closed_ = false;
    EXPECT_TRUE(notification_promo_->CanShow());
  }

  void TestText() {
    notification_promo_->text_.clear();
    EXPECT_FALSE(notification_promo_->CanShow());

    notification_promo_->text_ = text_;
    EXPECT_TRUE(notification_promo_->CanShow());
  }

  void TestTime() {
    const double now = base::Time::Now().ToDoubleT();
    const double qhour = 15 * 60;

    notification_promo_->group_ = 0;  // For simplicity.

    notification_promo_->start_ = now - qhour;
    notification_promo_->end_ = now + qhour;
    EXPECT_TRUE(notification_promo_->CanShow());

    // Start time has not arrived.
    notification_promo_->start_ = now + qhour;
    notification_promo_->end_ = now + qhour;
    EXPECT_FALSE(notification_promo_->CanShow());

    // End time has past.
    notification_promo_->start_ = now - qhour;
    notification_promo_->end_ = now - qhour;
    EXPECT_FALSE(notification_promo_->CanShow());

    notification_promo_->start_ = start_;
    notification_promo_->end_ = end_;
    EXPECT_TRUE(notification_promo_->CanShow());
  }

  void TestFeatureMask() {
    // No gplus cookie, feature mask in use.
    notification_promo_->gplus_ = false;
    notification_promo_->feature_mask_ = NotificationPromo::FEATURE_GPLUS;
    EXPECT_FALSE(notification_promo_->CanShow());

    // Gplus cookie, feature mask in use.
    notification_promo_->gplus_ = true;
    notification_promo_->feature_mask_ = NotificationPromo::FEATURE_GPLUS;
    EXPECT_TRUE(notification_promo_->CanShow());

    // If no feature mask, gplus_ value is ignored.
    notification_promo_->gplus_ = true;
    notification_promo_->feature_mask_ = NotificationPromo::NO_FEATURE;
    EXPECT_TRUE(notification_promo_->CanShow());

    notification_promo_->gplus_ = false;
    notification_promo_->feature_mask_ = NotificationPromo::NO_FEATURE;
    EXPECT_TRUE(notification_promo_->CanShow());
  }

 private:
  Profile* profile_;
  PrefService* prefs_;
  NotificationPromo* notification_promo_;
  bool received_notification_;
  bool should_receive_notification_;
  bool build_targeted_;
  scoped_ptr<DictionaryValue> test_json_;

  double start_;
  double end_;

  int build_;
  int time_slice_;
  int max_group_;
  int max_views_;
  int platform_;
  int feature_mask_;

  std::string text_;
  bool closed_;

  bool gplus_;
  int current_platform_;
};

TEST_F(PromoResourceServiceTest, NotificationPromoTest) {
  // Check that prefs are set correctly.
  PrefService* prefs = profile_.GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  NotificationPromoTestDelegate delegate(&profile_);
  scoped_refptr<NotificationPromo> notification_promo =
      NotificationPromo::Create(&profile_, &delegate);

  // Make sure prefs are unset.
  delegate.TestPrefs();

  // Set up start and end dates and promo line in a Dictionary as if parsed
  // from the service.
  delegate.Init(notification_promo,
                "{ "
                "  \"topic\": {"
                "    \"answers\": ["
                "       {"
                "        \"name\": \"promo_start\","
                "        \"question\": \"3:2:5:10:15:0\","
                "        \"tooltip\": \"Eat more pie!\","
                "        \"inproduct\": \"31/01/10 01:00 GMT\""
                "       },"
                "       {"
                "        \"name\": \"promo_end\","
                "        \"inproduct\": \"31/01/14 01:00 GMT\""
                "       }"
                "    ]"
                "  }"
                "}",
                1264899600,  // unix epoch for Jan 31 2010 0100 GMT.
                1391130000,  // unix epoch for Jan 31 2012 0100 GMT.
                3, 2, 5, 10, 15, 0,
                "Eat more pie!", false, false);

  delegate.InitPromoFromJson(true);

  // Second time should not trigger a notification.
  delegate.InitPromoFromJson(false);

  delegate.TestInitFromPrefs();

  // Test various conditions of CanShow.
  // TestGroup Has the side effect of setting us to a passing group.
  delegate.TestGroup();
  delegate.TestViews();
  delegate.TestBuild();
  delegate.TestClosed();
  delegate.TestText();
  delegate.TestTime();
  delegate.TestFeatureMask();
  delegate.TestPlatforms();
  delegate.TestGPlus();
}

TEST_F(PromoResourceServiceTest, NotificationPromoTestFail) {
  // Check that prefs are set correctly.
  PrefService* prefs = profile_.GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  NotificationPromoTestDelegate delegate(&profile_);
  scoped_refptr<NotificationPromo> notification_promo =
      NotificationPromo::Create(&profile_, &delegate);

  // Set up start and end dates and promo line in a Dictionary as if parsed
  // from the service.
  delegate.Init(notification_promo,
                "{ "
                "  \"topic\": {"
                "    \"answers\": ["
                "       {"
                "        \"name\": \"promo_start\","
                "        \"question\": \"12:8:10:20:15:0\","
                "        \"tooltip\": \"Happy 3rd Birthday!\","
                "        \"inproduct\": \"09/15/10 05:00 PDT\""
                "       },"
                "       {"
                "        \"name\": \"promo_end\","
                "        \"inproduct\": \"09/30/10 05:00 PDT\""
                "       }"
                "    ]"
                "  }"
                "}",
                1284552000,  // unix epoch for Sep 15 2010 0500 PDT.
                1285848000,  // unix epoch for Sep 30 2010 0500 PDT.
                12, 8, 10, 20, 15, 0,
                "Happy 3rd Birthday!", false, false);

  delegate.InitPromoFromJson(true);

  // Second time should not trigger a notification.
  delegate.InitPromoFromJson(false);

  delegate.TestInitFromPrefs();

  // Should fail because out of time bounds.
  EXPECT_FALSE(notification_promo->CanShow());
}

TEST_F(PromoResourceServiceTest, GetNextQuestionValueTest) {
  const std::string question("0:-100:2048:0:0:0:2a");
  const int question_vec[] = { 0, -100, 2048, 0, 0, 0};
  size_t index = 0;
  bool err = false;

  for (size_t i = 0; i < arraysize(question_vec); ++i) {
    EXPECT_EQ(question_vec[i],
        NotificationPromo::GetNextQuestionValue(question, &index, &err));
    EXPECT_FALSE(err);
  }
  EXPECT_EQ(NotificationPromo::GetNextQuestionValue(question, &index, &err), 0);
  EXPECT_TRUE(err);
}

TEST_F(PromoResourceServiceTest, NewGroupTest) {
  for (size_t i = 0; i < 1000; ++i) {
    const int group = NotificationPromo::NewGroup();
    EXPECT_GE(group, 0);
    EXPECT_LT(group, 100);
  }
}

TEST_F(PromoResourceServiceTest, UnpackWebStoreSignal) {
  web_resource_service_->set_channel(chrome::VersionInfo::CHANNEL_DEV);

  std::string json = "{ "
                     "  \"topic\": {"
                     "    \"answers\": ["
                     "       {"
                     "        \"answer_id\": \"341252\","
                     "        \"name\": \"webstore_promo:15:1:\","
                     "        \"question\": \"The header!\","
                     "        \"inproduct_target\": \"The button label!\","
                     "        \"inproduct\": \"http://link.com\","
                     "        \"tooltip\": \"No thanks, hide this.\""
                     "       }"
                     "    ]"
                     "  }"
                     "}";
  scoped_ptr<DictionaryValue> test_json(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  // Set the source logo URL to verify that it gets cleared.
  AppsPromo::SetSourcePromoLogoURL(GURL("https://www.google.com/test.png"));

  // Check that prefs are set correctly.
  web_resource_service_->UnpackWebStoreSignal(*(test_json.get()));

  AppsPromo::PromoData actual_data = AppsPromo::GetPromo();
  EXPECT_EQ("341252", actual_data.id);
  EXPECT_EQ("The header!", actual_data.header);
  EXPECT_EQ("The button label!", actual_data.button);
  EXPECT_EQ(GURL("http://link.com"), actual_data.link);
  EXPECT_EQ("No thanks, hide this.", actual_data.expire);
  EXPECT_EQ(AppsPromo::USERS_NEW, actual_data.user_group);

  // When we don't download a logo, we revert to the default and clear the
  // source pref.
  EXPECT_EQ(GURL("chrome://theme/IDR_WEBSTORE_ICON"), actual_data.logo);
  EXPECT_EQ(GURL(""), AppsPromo::GetSourcePromoLogoURL());
}

// Tests that the "web store active" flag is set even when the web store promo
// fails parsing.
TEST_F(PromoResourceServiceTest, UnpackPartialWebStoreSignal) {
  std::string json = "{ "
                     "  \"topic\": {"
                     "    \"answers\": ["
                     "       {"
                     "        \"answer_id\": \"sdlfj32\","
                     "        \"name\": \"webstore_promo:#klsdjlfSD\""
                     "       }"
                     "    ]"
                     "  }"
                     "}";
  scoped_ptr<DictionaryValue> test_json(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  // Check that prefs are set correctly.
  web_resource_service_->UnpackWebStoreSignal(*(test_json.get()));
  EXPECT_FALSE(AppsPromo::IsPromoSupportedForLocale());
  EXPECT_TRUE(AppsPromo::IsWebStoreSupportedForLocale());
}

// Tests that we can successfully unpack web store signals with HTTPS
// logos.
TEST_F(PromoResourceServiceTest, UnpackWebStoreSignalHttpsLogo) {
  web_resource_service_->set_channel(chrome::VersionInfo::CHANNEL_DEV);

  std::string logo_url = "https://www.google.com/image/test.png";
  std::string png_data = "!$#%,./nvl;iadh9oh82";
  std::string png_base64 = "data:image/png;base64,ISQjJSwuL252bDtpYWRoOW9oODI=";

  FakeURLFetcherFactory factory;
  factory.SetFakeResponse(logo_url, png_data, true);

  std::string json =
      "{ "
      "  \"topic\": {"
      "    \"answers\": ["
      "       {"
      "        \"answer_id\": \"340252\","
      "        \"name\": \"webstore_promo:15:1:" + logo_url + "\","
      "        \"question\": \"Header!\","
      "        \"inproduct_target\": \"The button label!\","
      "        \"inproduct\": \"http://link.com\","
      "        \"tooltip\": \"No thanks, hide this.\""
      "       }"
      "    ]"
      "  }"
      "}";

  scoped_ptr<DictionaryValue> test_json(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  // Update the promo multiple times to verify the logo is cached correctly.
  for (size_t i = 0; i < 2; ++i) {
    web_resource_service_->UnpackWebStoreSignal(*(test_json.get()));

    // We should only need to run the message loop the first time since the
    // image is then cached.
    if (i == 0)
      loop_.RunAllPending();

    // Reset this scoped_ptr to prevent a DCHECK.
    web_resource_service_->apps_promo_logo_fetcher_.reset();

    AppsPromo::PromoData actual_data = AppsPromo::GetPromo();
    EXPECT_EQ("340252", actual_data.id);
    EXPECT_EQ("Header!", actual_data.header);
    EXPECT_EQ("The button label!", actual_data.button);
    EXPECT_EQ(GURL("http://link.com"), actual_data.link);
    EXPECT_EQ("No thanks, hide this.", actual_data.expire);
    EXPECT_EQ(AppsPromo::USERS_NEW, actual_data.user_group);

    // The logo should now be a base64 DATA URL.
    EXPECT_EQ(GURL(png_base64), actual_data.logo);

    // And the source pref should hold the source HTTPS URL.
    EXPECT_EQ(GURL(logo_url), AppsPromo::GetSourcePromoLogoURL());
  }
}

// Tests that we revert to the default logo when the fetch fails.
TEST_F(PromoResourceServiceTest, UnpackWebStoreSignalHttpsLogoError) {
  web_resource_service_->set_channel(chrome::VersionInfo::CHANNEL_DEV);

  std::string logo_url = "https://www.google.com/image/test.png";
  std::string png_data = "!$#%,./nvl;iadh9oh82";
  std::string png_base64 = "ISQjJSwuL252bDtpYWRoOW9oODI=";

  FakeURLFetcherFactory factory;

  // Have URLFetcher return a 500 error.
  factory.SetFakeResponse(logo_url, png_data, false);

  std::string json =
      "{ "
      "  \"topic\": {"
      "    \"answers\": ["
      "       {"
      "        \"answer_id\": \"340252\","
      "        \"name\": \"webstore_promo:15:1:" + logo_url + "\","
      "        \"question\": \"Header!\","
      "        \"inproduct_target\": \"The button label!\","
      "        \"inproduct\": \"http://link.com\","
      "        \"tooltip\": \"No thanks, hide this.\""
      "       }"
      "    ]"
      "  }"
      "}";

  scoped_ptr<DictionaryValue> test_json(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  web_resource_service_->UnpackWebStoreSignal(*(test_json.get()));

  loop_.RunAllPending();

  // Reset this scoped_ptr to prevent a DCHECK.
  web_resource_service_->apps_promo_logo_fetcher_.reset();

  AppsPromo::PromoData actual_data = AppsPromo::GetPromo();
  EXPECT_EQ("340252", actual_data.id);
  EXPECT_EQ("Header!", actual_data.header);
  EXPECT_EQ("The button label!", actual_data.button);
  EXPECT_EQ(GURL("http://link.com"), actual_data.link);
  EXPECT_EQ("No thanks, hide this.", actual_data.expire);
  EXPECT_EQ(AppsPromo::USERS_NEW, actual_data.user_group);

  // Logos are the default values.
  EXPECT_EQ(GURL("chrome://theme/IDR_WEBSTORE_ICON"), actual_data.logo);
  EXPECT_EQ(GURL(""), AppsPromo::GetSourcePromoLogoURL());
}

// Tests that we don't download images over HTTP.
TEST_F(PromoResourceServiceTest, UnpackWebStoreSignalHttpLogo) {
  web_resource_service_->set_channel(chrome::VersionInfo::CHANNEL_DEV);

  // Use an HTTP URL.
  std::string logo_url = "http://www.google.com/image/test.png";
  std::string png_data = "!$#%,./nvl;iadh9oh82";
  std::string png_base64 = "ISQjJSwuL252bDtpYWRoOW9oODI=";

  FakeURLFetcherFactory factory;
  factory.SetFakeResponse(logo_url, png_data, true);

  std::string json =
      "{ "
      "  \"topic\": {"
      "    \"answers\": ["
      "       {"
      "        \"answer_id\": \"340252\","
      "        \"name\": \"webstore_promo:15:1:" + logo_url + "\","
      "        \"question\": \"Header!\","
      "        \"inproduct_target\": \"The button label!\","
      "        \"inproduct\": \"http://link.com\","
      "        \"tooltip\": \"No thanks, hide this.\""
      "       }"
      "    ]"
      "  }"
      "}";

  scoped_ptr<DictionaryValue> test_json(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json, false)));

  web_resource_service_->UnpackWebStoreSignal(*(test_json.get()));

  loop_.RunAllPending();

  // Reset this scoped_ptr to prevent a DCHECK.
  web_resource_service_->apps_promo_logo_fetcher_.reset();

  AppsPromo::PromoData actual_data = AppsPromo::GetPromo();
  EXPECT_EQ("340252", actual_data.id);
  EXPECT_EQ("Header!", actual_data.header);
  EXPECT_EQ("The button label!", actual_data.button);
  EXPECT_EQ(GURL("http://link.com"), actual_data.link);
  EXPECT_EQ("No thanks, hide this.", actual_data.expire);
  EXPECT_EQ(AppsPromo::USERS_NEW, actual_data.user_group);

  // Logos should be the default values because HTTP URLs are not valid.
  EXPECT_EQ(GURL("chrome://theme/IDR_WEBSTORE_ICON"), actual_data.logo);
  EXPECT_EQ(GURL(""), AppsPromo::GetSourcePromoLogoURL());
}

// Don't run sync promo unpacking tests on ChromeOS as on that plaform
// PromoResourceService::UnpackNTPSignInPromoSignal() basically just no-ops.
#if !defined(OS_CHROMEOS)
TEST_F(NTPSignInPromoTest, UnpackNTPSignInPromoSignal) {
  PrefService* prefs = profile_.GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  // Test on by default (currently should be false).
  EXPECT_FALSE(PromoResourceService::CanShowNTPSignInPromo(&profile_));
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kNTPSignInPromoGroup));
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kNTPSignInPromoGroupMax));

  // Non-targeted build.
  ClearNTPSignInPromoPrefs();
  SetupNTPSignInPromoCase(2, 50);
  EXPECT_FALSE(PromoResourceService::CanShowNTPSignInPromo(&profile_));
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kNTPSignInPromoGroup));
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kNTPSignInPromoGroupMax));

  // Targeted build, doesn't create bucket and doesn't show promo because
  // groupMax < group.
  ClearNTPSignInPromoPrefs();
  SetupNTPSignInPromoCase(1, 0);
  EXPECT_FALSE(PromoResourceService::CanShowNTPSignInPromo(&profile_));
  EXPECT_EQ(prefs->GetInteger(prefs::kNTPSignInPromoGroupMax), 0);
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kNTPSignInPromoGroup));

  // Targeted build, max_group = 50, ensure group pref created and within the
  // group bounds.
  ClearNTPSignInPromoPrefs();
  SetupNTPSignInPromoCase(1, 50);
  PromoResourceService::CanShowNTPSignInPromo(&profile_);
  EXPECT_EQ(prefs->GetInteger(prefs::kNTPSignInPromoGroupMax), 50);
  EXPECT_TRUE(prefs->HasPrefPath(prefs::kNTPSignInPromoGroup));
  EXPECT_GT(prefs->GetInteger(prefs::kNTPSignInPromoGroup), 0);
  EXPECT_LE(prefs->GetInteger(prefs::kNTPSignInPromoGroup), 100);

  // Set user group = 50, now shows promo.
  prefs->SetInteger(prefs::kNTPSignInPromoGroup, 50);
  EXPECT_TRUE(PromoResourceService::CanShowNTPSignInPromo(&profile_));
  EXPECT_EQ(prefs->GetInteger(prefs::kNTPSignInPromoGroup), 50);
  EXPECT_EQ(prefs->GetInteger(prefs::kNTPSignInPromoGroupMax), 50);

  // Bump user group, ensure that we should not show promo.
  prefs->SetInteger(prefs::kNTPSignInPromoGroup, 51);
  EXPECT_FALSE(PromoResourceService::CanShowNTPSignInPromo(&profile_));
  EXPECT_EQ(prefs->GetInteger(prefs::kNTPSignInPromoGroup), 51);
  EXPECT_EQ(prefs->GetInteger(prefs::kNTPSignInPromoGroupMax), 50);

  // If the max group gets bumped to the user's group (or above), it should
  // show.
  prefs->SetInteger(prefs::kNTPSignInPromoGroupMax, 51);
  EXPECT_TRUE(PromoResourceService::CanShowNTPSignInPromo(&profile_));
  EXPECT_EQ(prefs->GetInteger(prefs::kNTPSignInPromoGroup), 51);
  EXPECT_EQ(prefs->GetInteger(prefs::kNTPSignInPromoGroupMax), 51);

  // Reduce max group.
  prefs->SetInteger(prefs::kNTPSignInPromoGroupMax, 49);
  EXPECT_FALSE(PromoResourceService::CanShowNTPSignInPromo(&profile_));
  EXPECT_EQ(prefs->GetInteger(prefs::kNTPSignInPromoGroup), 51);
  EXPECT_EQ(prefs->GetInteger(prefs::kNTPSignInPromoGroupMax), 49);

  // Ignore non-targeted builds.
  prefs->SetInteger(prefs::kNTPSignInPromoGroup, 50);
  prefs->SetInteger(prefs::kNTPSignInPromoGroupMax, 75);
  EXPECT_TRUE(PromoResourceService::CanShowNTPSignInPromo(&profile_));
  SetupNTPSignInPromoCase(2, 25);
  // Make sure the prefs are deleted.
  EXPECT_FALSE(PromoResourceService::CanShowNTPSignInPromo(&profile_));
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kNTPSignInPromoGroup));
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kNTPSignInPromoGroupMax));
}

// Throw random stuff at UnpackNTPSignInPromoSignal and make sure no segfaults
// or other issues and that the prefs were cleared.
TEST_F(NTPSignInPromoTest, UnpackNTPSignInPromoSignalInvalid) {
  // Empty.
  InvalidTestCase("");

  // Negative numbers.
  InvalidTestCase("-5:-6");

  // An extra field.
  InvalidTestCase("1:0:1");

  // A ton of separators.
  InvalidTestCase("::::::");

  // Really big numbers.
  InvalidTestCase("68719476737:68719476737");

  // UTF-8 chars.
  InvalidTestCase("だからって馬鹿に:してるの？怒る友人");
}

TEST_F(NTPSignInPromoTest, UnpackNTPSignInPromoSignalNotify) {
  // Clear prefs and ensure we're not triggering notifications every time we
  // receive not-targeted or valid data (only when we need to turn off the
  // promo).
  ClearNTPSignInPromoPrefs();
  reset_notification_count();

  // Non-targeted build.
  SetupNTPSignInPromoCase(2, 50);

  // Targeted, but under any possible max group.
  SetupNTPSignInPromoCase(1, 0);

  // Targeted for sure, but shouldn't send notification.
  SetupNTPSignInPromoCase(1, 100);

  // Expect this didn't trigger any notifications.
  EXPECT_EQ(notifications_received(), 0);

  // Expect notifications when the promo is disabled, as this is expected
  // behavior.
  SetupNTPSignInPromoCase(2, 0);
  EXPECT_EQ(notifications_received(), 1);
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(PromoResourceServiceTest, IsBuildTargetedTest) {
  // canary
  const chrome::VersionInfo::Channel canary =
      chrome::VersionInfo::CHANNEL_CANARY;
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(canary, 1));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(canary, 3));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(canary, 7));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(canary, 15));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(canary, 8));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(canary, 11));

  // dev
  const chrome::VersionInfo::Channel dev =
      chrome::VersionInfo::CHANNEL_DEV;
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(dev, 1));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(dev, 3));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(dev, 7));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(dev, 15));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(dev, 8));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(dev, 11));

  // beta
  const chrome::VersionInfo::Channel beta =
      chrome::VersionInfo::CHANNEL_BETA;
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(beta, 1));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(beta, 3));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(beta, 7));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(beta, 15));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(beta, 8));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(beta, 11));

  // stable
  const chrome::VersionInfo::Channel stable =
      chrome::VersionInfo::CHANNEL_STABLE;
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(stable, 1));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(stable, 3));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(stable, 7));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(stable, 15));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(stable, 8));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(stable, 11));
  EXPECT_TRUE(PromoResourceService::IsBuildTargeted(stable, 12));

  // invalid
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(stable, -1));
  EXPECT_FALSE(PromoResourceService::IsBuildTargeted(stable, INT_MAX));
}
