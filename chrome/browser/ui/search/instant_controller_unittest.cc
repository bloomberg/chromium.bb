// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/search/instant_controller.h"
#include "chrome/browser/ui/search/instant_ntp.h"
#include "chrome/browser/ui/search/instant_overlay.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::HistogramBase;
using base::HistogramSamples;
using base::StatisticsRecorder;

class TestableInstantOverlay : public InstantOverlay {
 public:
  TestableInstantOverlay(InstantController* controller,
                         const std::string& instant_url)
      : InstantOverlay(controller, instant_url, false) {
  }

  // Overrides from InstantPage
  virtual bool supports_instant() const OVERRIDE {
    return test_supports_instant_;
  }

  virtual bool IsLocal() const OVERRIDE {
    return test_is_local_;
  };

  void set_supports_instant(bool supports_instant) {
    test_supports_instant_ = supports_instant;
  }

  void set_is_local(bool is_local) {
    test_is_local_ = is_local;
  }

 private:
  std::string test_instant_url_;
  bool test_supports_instant_;
  bool test_is_local_;
};

class TestableInstantNTP : public InstantNTP {
 public:
  TestableInstantNTP(InstantController* controller,
                     const std::string& instant_url)
      : InstantNTP(controller, instant_url, false) {
  }

  // Overrides from InstantPage
  virtual bool supports_instant() const OVERRIDE {
    return test_supports_instant_;
  }

  virtual bool IsLocal() const OVERRIDE {
    return test_is_local_;
  };

  virtual const std::string& instant_url() const OVERRIDE {
    return test_instant_url_;
  }

  void set_instant_url(const std::string& instant_url) {
    test_instant_url_ = instant_url;
  }

  void set_supports_instant(bool supports_instant) {
    test_supports_instant_ = supports_instant;
  }

  void set_is_local(bool is_local) {
    test_is_local_ = is_local;
  }

 private:
  std::string test_instant_url_;
  bool test_supports_instant_;
  bool test_is_local_;
};

class TestableInstantController : public InstantController {
 public:
  TestableInstantController()
      : InstantController(NULL, true),
        test_instant_url_("http://test_url"),
        test_extended_enabled_(true),
        override_javascript_enabled_(true),
        test_javascript_enabled_(true),
        test_in_startup_(false),
        test_overlay_(NULL),
        test_ntp_(NULL) {}

  // Overrides from InstantController
  virtual std::string GetInstantURL() const OVERRIDE {
    return test_instant_url_;
  }

  virtual std::string GetLocalInstantURL() const OVERRIDE {
    return "http://local_instant_url";
  }

  virtual bool extended_enabled() const OVERRIDE {
    return test_extended_enabled_;
  }

  virtual InstantOverlay* overlay() const OVERRIDE {
    return test_overlay_;
  }

  virtual InstantNTP* ntp() const OVERRIDE {
    return test_ntp_;
  }

  void set_instant_url(std::string instant_url) {
    test_instant_url_ = instant_url;
  }

  void set_extended_enabled(bool extended_enabled) {
    test_extended_enabled_ = extended_enabled;
  }

  void set_overlay(InstantOverlay* overlay) {
    test_overlay_ = overlay;
  }

  void set_ntp(InstantNTP* ntp) {
    test_ntp_ = ntp;
  }

  void set_javascript_enabled(bool javascript_enabled) {
    override_javascript_enabled_ = true;
    test_javascript_enabled_ = javascript_enabled;
  }

  void set_override_javascript_enabled(bool override_javascript_enabled) {
    override_javascript_enabled_ = override_javascript_enabled;
  }

  void set_in_startup(bool in_startup) {
    test_in_startup_ = in_startup;
  }

  virtual bool IsJavascriptEnabled() const OVERRIDE {
    if (override_javascript_enabled_)
      return test_javascript_enabled_;
    else
      return InstantController::IsJavascriptEnabled();
  }

  virtual bool InStartup() const OVERRIDE {
    return test_in_startup_;
  }

  virtual Profile* profile() const OVERRIDE {
    return &profile_;
  }

private:
  std::string test_instant_url_;
  bool test_extended_enabled_;
  bool override_javascript_enabled_;
  bool test_javascript_enabled_;
  bool test_in_startup_;
  InstantOverlay* test_overlay_;
  InstantNTP* test_ntp_;
  mutable TestingProfile profile_;
};

class InstantControllerTest : public testing::Test {
 public:
  InstantControllerTest()
      : ui_thread_(content::BrowserThread::UI),
        instant_controller_(new TestableInstantController()) {
  }

  virtual void SetUp() OVERRIDE {
    base::StatisticsRecorder::Initialize();
  }

  TestableInstantController* instant_controller() {
    return instant_controller_.get();
  }

 private:
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestableInstantController> instant_controller_;
};

TEST_F(InstantControllerTest, ShouldSwitchToLocalOverlay) {
  InstantController::InstantFallbackReason fallback_reason;

  instant_controller()->set_extended_enabled(false);
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason, InstantController::INSTANT_FALLBACK_NONE);

  instant_controller()->set_extended_enabled(true);
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason, InstantController::INSTANT_FALLBACK_NO_OVERLAY);

  std::string instant_url("http://test_url");
  scoped_ptr<TestableInstantOverlay> test_overlay(
      new TestableInstantOverlay(instant_controller(), instant_url));
  test_overlay->set_is_local(true);
  instant_controller()->set_overlay(test_overlay.get());
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason, InstantController::INSTANT_FALLBACK_NONE);

  instant_controller()->set_javascript_enabled(false);
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason,
            InstantController::INSTANT_FALLBACK_JAVASCRIPT_DISABLED);
  instant_controller()->set_javascript_enabled(true);

  test_overlay->set_is_local(false);
  instant_controller()->set_instant_url("");
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason,
            InstantController::INSTANT_FALLBACK_INSTANT_URL_EMPTY);

  instant_controller()->set_instant_url("http://instant_url");
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason,
            InstantController::INSTANT_FALLBACK_ORIGIN_PATH_MISMATCH);

  instant_controller()->set_instant_url(instant_url);
  test_overlay->set_supports_instant(false);
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason,
            InstantController::INSTANT_FALLBACK_INSTANT_NOT_SUPPORTED);

  test_overlay->set_supports_instant(true);
  fallback_reason = instant_controller()->ShouldSwitchToLocalOverlay();
  ASSERT_EQ(fallback_reason, InstantController::INSTANT_FALLBACK_NONE);
}

TEST_F(InstantControllerTest, PrefersRemoteNTPOnStartup) {
  std::string instant_url("http://instant_url");
  scoped_ptr<TestableInstantNTP> ntp(new TestableInstantNTP(
      instant_controller(), instant_url));
  ntp->set_is_local(false);
  instant_controller()->set_ntp(ntp.get());
  instant_controller()->set_javascript_enabled(true);
  instant_controller()->set_extended_enabled(true);
  instant_controller()->set_instant_url(instant_url);
  ntp->set_instant_url(instant_url);
  ntp->set_supports_instant(false);
  instant_controller()->set_in_startup(true);
  EXPECT_EQ(!chrome::ShouldPreferRemoteNTPOnStartup(),
            instant_controller()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantControllerTest, SwitchesToLocalNTPIfNoInstantSupport) {
  std::string instant_url("http://instant_url");
  scoped_ptr<TestableInstantNTP> ntp(new TestableInstantNTP(
      instant_controller(), instant_url));
  ntp.reset(new TestableInstantNTP(instant_controller(), instant_url));
  ntp->set_is_local(false);
  instant_controller()->set_ntp(ntp.get());
  instant_controller()->set_javascript_enabled(true);
  instant_controller()->set_extended_enabled(true);
  instant_controller()->set_instant_url(instant_url);
  ntp->set_instant_url(instant_url);
  ntp->set_supports_instant(false);
  instant_controller()->set_in_startup(false);
  EXPECT_TRUE(instant_controller()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantControllerTest, SwitchesToLocalNTPIfPathBad) {
  std::string instant_url("http://instant_url");
  scoped_ptr<TestableInstantNTP> ntp(new TestableInstantNTP(
      instant_controller(), instant_url));
  ntp.reset(new TestableInstantNTP(instant_controller(), instant_url));
  ntp->set_is_local(false);
  instant_controller()->set_ntp(ntp.get());
  instant_controller()->set_javascript_enabled(true);
  instant_controller()->set_extended_enabled(true);
  instant_controller()->set_instant_url("http://bogus_url");
  ntp->set_instant_url(instant_url);
  ntp->set_supports_instant(true);
  instant_controller()->set_in_startup(false);
  EXPECT_TRUE(instant_controller()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantControllerTest, DoesNotSwitchToLocalNTPIfOnCurrentNTP) {
  std::string instant_url("http://instant_url");
  scoped_ptr<TestableInstantNTP> ntp(new TestableInstantNTP(
      instant_controller(), instant_url));
  ntp.reset(new TestableInstantNTP(instant_controller(), instant_url));
  ntp->set_is_local(false);
  instant_controller()->set_ntp(ntp.get());
  instant_controller()->set_javascript_enabled(true);
  instant_controller()->set_extended_enabled(true);
  instant_controller()->set_instant_url(instant_url);
  ntp->set_instant_url(instant_url);
  ntp->set_supports_instant(true);
  instant_controller()->set_in_startup(false);
  EXPECT_FALSE(instant_controller()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantControllerTest, DoesNotSwitchToLocalNTPIfOnLocalNTP) {
  std::string instant_url("http://instant_url");
  scoped_ptr<TestableInstantNTP> ntp(new TestableInstantNTP(
      instant_controller(), instant_url));
  ntp.reset(new TestableInstantNTP(instant_controller(), instant_url));
  ntp->set_is_local(false);
  instant_controller()->set_ntp(ntp.get());
  instant_controller()->set_javascript_enabled(true);
  instant_controller()->set_extended_enabled(true);
  instant_controller()->set_instant_url(instant_url);
  ntp->set_instant_url("http://local_instant_url");
  ntp->set_supports_instant(true);
  instant_controller()->set_in_startup(false);
  EXPECT_FALSE(instant_controller()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantControllerTest, SwitchesToLocalNTPIfJSDisabled) {
  std::string instant_url("http://instant_url");
  scoped_ptr<TestableInstantNTP> ntp(new TestableInstantNTP(
      instant_controller(), instant_url));
  ntp.reset(new TestableInstantNTP(instant_controller(), instant_url));
  ntp->set_is_local(false);
  instant_controller()->set_ntp(ntp.get());
  instant_controller()->set_javascript_enabled(false);
  instant_controller()->set_extended_enabled(true);
  instant_controller()->set_instant_url(instant_url);
  ntp->set_instant_url("http://local_instant_url");
  ntp->set_supports_instant(true);
  instant_controller()->set_in_startup(false);
  EXPECT_TRUE(instant_controller()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantControllerTest, SwitchesToLocalNTPIfNoNTPReady) {
  EXPECT_TRUE(instant_controller()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantControllerTest, IsJavascriptEnabled) {
  instant_controller()->set_override_javascript_enabled(false);
  EXPECT_TRUE(instant_controller()->IsJavascriptEnabled());
}

TEST_F(InstantControllerTest, IsJavascriptEnabledChecksContentSettings) {
  instant_controller()->set_override_javascript_enabled(false);
  instant_controller()->profile()->GetHostContentSettingsMap()
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 CONTENT_SETTING_DEFAULT);
  EXPECT_TRUE(instant_controller()->IsJavascriptEnabled());
  instant_controller()->profile()->GetHostContentSettingsMap()
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(instant_controller()->IsJavascriptEnabled());
  instant_controller()->profile()->GetHostContentSettingsMap()
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(instant_controller()->IsJavascriptEnabled());
}

TEST_F(InstantControllerTest, IsJavascriptEnabledChecksPrefs) {
  instant_controller()->set_override_javascript_enabled(false);
  instant_controller()->profile()->GetPrefs()->SetBoolean(
      prefs::kWebKitJavascriptEnabled, true);
  EXPECT_TRUE(instant_controller()->IsJavascriptEnabled());
  instant_controller()->profile()->GetPrefs()->SetBoolean(
      prefs::kWebKitJavascriptEnabled, false);
  EXPECT_FALSE(instant_controller()->IsJavascriptEnabled());
}
