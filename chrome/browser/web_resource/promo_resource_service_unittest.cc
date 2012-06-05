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
      static_cast<DictionaryValue*>(base::JSONReader::Read(json)));

  // Check that prefs are set correctly.
  web_resource_service_->UnpackLogoSignal(*(test_json.get()));
  PrefService* prefs = profile_.GetPrefs();
  ASSERT_TRUE(prefs != NULL);

  double logo_start =
      prefs->GetDouble(prefs::kNtpCustomLogoStart);
  EXPECT_EQ(logo_start, 1264899600);  // unix epoch for Jan 31 2010 0100 GMT.
  double logo_end =
      prefs->GetDouble(prefs::kNtpCustomLogoEnd);
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
      base::JSONReader::Read(json)));

  // Check that prefs are set correctly.
  web_resource_service_->UnpackLogoSignal(*(test_json.get()));

  logo_start = prefs->GetDouble(prefs::kNtpCustomLogoStart);
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
      base::JSONReader::Read(json)));

  // Check that prefs are set correctly.
  web_resource_service_->UnpackLogoSignal(*(test_json.get()));
  logo_start = prefs->GetDouble(prefs::kNtpCustomLogoStart);
  EXPECT_EQ(logo_start, 0);  // date value reset to 0;
  logo_end = prefs->GetDouble(prefs::kNtpCustomLogoEnd);
  EXPECT_EQ(logo_end, 0);  // date value reset to 0;
}

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
      base::JSONReader::Read(json)));

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
      base::JSONReader::Read(json)));

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
      base::JSONReader::Read(json)));

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
      base::JSONReader::Read(json)));

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
      base::JSONReader::Read(json)));

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
