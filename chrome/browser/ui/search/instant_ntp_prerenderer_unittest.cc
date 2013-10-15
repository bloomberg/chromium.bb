// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/search/instant_ntp.h"
#include "chrome/browser/ui/search/instant_ntp_prerenderer.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestableInstantNTP : public InstantNTP {
 public:
  TestableInstantNTP(InstantNTPPrerenderer* ntp_prerenderer,
                     const std::string& instant_url,
                     Profile* profile)
      : InstantNTP(ntp_prerenderer, instant_url, profile) {
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

class TestableInstantNTPPrerenderer : public InstantNTPPrerenderer {
 public:
  explicit TestableInstantNTPPrerenderer(TestingProfile* profile,
      InstantService* instant_service)
      : InstantNTPPrerenderer(profile, instant_service, NULL),
        test_instant_url_("http://test_url"),
        override_javascript_enabled_(true),
        test_javascript_enabled_(true),
        test_in_startup_(false),
        test_ntp_(NULL) {
  }

  // Overrides from InstantNTPPrerenderer
  virtual std::string GetInstantURL() const OVERRIDE {
    return test_instant_url_;
  }

  virtual std::string GetLocalInstantURL() const OVERRIDE {
    return "http://local_instant_url";
  }

  virtual InstantNTP* ntp() const OVERRIDE {
    return test_ntp_;
  }

  virtual bool IsJavascriptEnabled() const OVERRIDE {
    if (override_javascript_enabled_)
      return test_javascript_enabled_;
    else
      return InstantNTPPrerenderer::IsJavascriptEnabled();
  }

  virtual bool InStartup() const OVERRIDE {
    return test_in_startup_;
  }

  void set_instant_url(const std::string& instant_url) {
    test_instant_url_ = instant_url;
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

private:
  std::string test_instant_url_;
  bool override_javascript_enabled_;
  bool test_javascript_enabled_;
  bool test_in_startup_;
  InstantNTP* test_ntp_;
};

class InstantNTPPrerendererTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    instant_service_ = InstantServiceFactory::GetForProfile(&profile_);
    instant_ntp_prerenderer_.reset(
        new TestableInstantNTPPrerenderer(&profile_, instant_service_));
  }

  virtual void TearDown() OVERRIDE {
    instant_ntp_prerenderer_.reset();
  }

  TestableInstantNTPPrerenderer* instant_ntp_prerenderer() {
    return instant_ntp_prerenderer_.get();
  }

  Profile* profile() {
    return instant_ntp_prerenderer()->profile();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestableInstantNTPPrerenderer> instant_ntp_prerenderer_;
  InstantService* instant_service_;
  mutable TestingProfile profile_;
};

TEST_F(InstantNTPPrerendererTest, PrefersRemoteNTPOnStartup) {
  std::string instant_url("http://instant_url");
  scoped_ptr<TestableInstantNTP> ntp(new TestableInstantNTP(
      instant_ntp_prerenderer(), instant_url, profile()));
  ntp->set_is_local(false);
  instant_ntp_prerenderer()->set_ntp(ntp.get());
  instant_ntp_prerenderer()->set_javascript_enabled(true);
  instant_ntp_prerenderer()->set_instant_url(instant_url);
  ntp->set_instant_url(instant_url);
  ntp->set_supports_instant(false);
  instant_ntp_prerenderer()->set_in_startup(true);
  EXPECT_EQ(!chrome::ShouldPreferRemoteNTPOnStartup(),
            instant_ntp_prerenderer()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantNTPPrerendererTest, SwitchesToLocalNTPIfNoInstantSupport) {
  std::string instant_url("http://instant_url");
  scoped_ptr<TestableInstantNTP> ntp(new TestableInstantNTP(
      instant_ntp_prerenderer(), instant_url, profile()));
  ntp.reset(new TestableInstantNTP(instant_ntp_prerenderer(), instant_url,
                                   profile()));
  ntp->set_is_local(false);
  instant_ntp_prerenderer()->set_ntp(ntp.get());
  instant_ntp_prerenderer()->set_javascript_enabled(true);
  instant_ntp_prerenderer()->set_instant_url(instant_url);
  ntp->set_instant_url(instant_url);
  ntp->set_supports_instant(false);
  instant_ntp_prerenderer()->set_in_startup(false);
  EXPECT_TRUE(instant_ntp_prerenderer()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantNTPPrerendererTest, SwitchesToLocalNTPIfPathBad) {
  std::string instant_url("http://instant_url");
  scoped_ptr<TestableInstantNTP> ntp(new TestableInstantNTP(
      instant_ntp_prerenderer(), instant_url, profile()));
  ntp.reset(new TestableInstantNTP(instant_ntp_prerenderer(), instant_url,
                                   profile()));
  ntp->set_is_local(false);
  instant_ntp_prerenderer()->set_ntp(ntp.get());
  instant_ntp_prerenderer()->set_javascript_enabled(true);
  instant_ntp_prerenderer()->set_instant_url("http://bogus_url");
  ntp->set_instant_url(instant_url);
  ntp->set_supports_instant(true);
  instant_ntp_prerenderer()->set_in_startup(false);
  EXPECT_TRUE(instant_ntp_prerenderer()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantNTPPrerendererTest, DoesNotSwitchToLocalNTPIfOnCurrentNTP) {
  std::string instant_url("http://instant_url");
  scoped_ptr<TestableInstantNTP> ntp(new TestableInstantNTP(
      instant_ntp_prerenderer(), instant_url, profile()));
  ntp.reset(new TestableInstantNTP(instant_ntp_prerenderer(), instant_url,
                                   profile()));
  ntp->set_is_local(false);
  instant_ntp_prerenderer()->set_ntp(ntp.get());
  instant_ntp_prerenderer()->set_javascript_enabled(true);
  instant_ntp_prerenderer()->set_instant_url(instant_url);
  ntp->set_instant_url(instant_url);
  ntp->set_supports_instant(true);
  instant_ntp_prerenderer()->set_in_startup(false);
  EXPECT_FALSE(instant_ntp_prerenderer()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantNTPPrerendererTest, DoesNotSwitchToLocalNTPIfOnLocalNTP) {
  std::string instant_url("http://instant_url");
  scoped_ptr<TestableInstantNTP> ntp(new TestableInstantNTP(
      instant_ntp_prerenderer(), instant_url, profile()));
  ntp.reset(new TestableInstantNTP(instant_ntp_prerenderer(), instant_url,
                                   profile()));
  ntp->set_is_local(false);
  instant_ntp_prerenderer()->set_ntp(ntp.get());
  instant_ntp_prerenderer()->set_javascript_enabled(true);
  instant_ntp_prerenderer()->set_instant_url(instant_url);
  ntp->set_instant_url("http://local_instant_url");
  ntp->set_supports_instant(true);
  instant_ntp_prerenderer()->set_in_startup(false);
  EXPECT_FALSE(instant_ntp_prerenderer()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantNTPPrerendererTest, SwitchesToLocalNTPIfJSDisabled) {
  std::string instant_url("http://instant_url");
  scoped_ptr<TestableInstantNTP> ntp(new TestableInstantNTP(
      instant_ntp_prerenderer(), instant_url, profile()));
  ntp.reset(new TestableInstantNTP(instant_ntp_prerenderer(), instant_url,
                                   profile()));
  ntp->set_is_local(false);
  instant_ntp_prerenderer()->set_ntp(ntp.get());
  instant_ntp_prerenderer()->set_javascript_enabled(false);
  instant_ntp_prerenderer()->set_instant_url(instant_url);
  ntp->set_instant_url("http://local_instant_url");
  ntp->set_supports_instant(true);
  instant_ntp_prerenderer()->set_in_startup(false);
  EXPECT_TRUE(instant_ntp_prerenderer()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantNTPPrerendererTest, SwitchesToLocalNTPIfNoNTPReady) {
  EXPECT_TRUE(instant_ntp_prerenderer()->ShouldSwitchToLocalNTP());
}

TEST_F(InstantNTPPrerendererTest, IsJavascriptEnabled) {
  instant_ntp_prerenderer()->set_override_javascript_enabled(false);
  EXPECT_TRUE(instant_ntp_prerenderer()->IsJavascriptEnabled());
}

TEST_F(InstantNTPPrerendererTest, IsJavascriptEnabledChecksContentSettings) {
  instant_ntp_prerenderer()->set_override_javascript_enabled(false);
  instant_ntp_prerenderer()->profile()->GetHostContentSettingsMap()
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 CONTENT_SETTING_DEFAULT);
  EXPECT_TRUE(instant_ntp_prerenderer()->IsJavascriptEnabled());
  instant_ntp_prerenderer()->profile()->GetHostContentSettingsMap()
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(instant_ntp_prerenderer()->IsJavascriptEnabled());
  instant_ntp_prerenderer()->profile()->GetHostContentSettingsMap()
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(instant_ntp_prerenderer()->IsJavascriptEnabled());
}

TEST_F(InstantNTPPrerendererTest, IsJavascriptEnabledChecksPrefs) {
  instant_ntp_prerenderer()->set_override_javascript_enabled(false);
  instant_ntp_prerenderer()->profile()->GetPrefs()->SetBoolean(
      prefs::kWebKitJavascriptEnabled, true);
  EXPECT_TRUE(instant_ntp_prerenderer()->IsJavascriptEnabled());
  instant_ntp_prerenderer()->profile()->GetPrefs()->SetBoolean(
      prefs::kWebKitJavascriptEnabled, false);
  EXPECT_FALSE(instant_ntp_prerenderer()->IsJavascriptEnabled());
}
