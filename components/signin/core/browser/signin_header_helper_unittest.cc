// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_header_helper.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/signin/core/browser/chrome_connected_header_helper.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !defined(OS_IOS) && !defined(OS_ANDROID)
#include "components/signin/core/browser/dice_header_helper.h"
#endif

namespace signin {

class SigninHeaderHelperTest : public testing::Test {
 protected:
  void SetUp() override {
    content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());

    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* incognito_profile */, false /* guest_profile */,
        false /* store_last_modified */);
    cookie_settings_ =
        new content_settings::CookieSettings(settings_map_.get(), &prefs_, "");
  }

  void TearDown() override { settings_map_->ShutdownOnUIThread(); }

  void CheckMirrorCookieRequest(const GURL& url,
                                const std::string& account_id,
                                const std::string& expected_request) {
    EXPECT_EQ(
        BuildMirrorRequestCookieIfPossible(
            url, account_id, cookie_settings_.get(), PROFILE_MODE_DEFAULT),
        expected_request);
  }

  std::unique_ptr<net::URLRequest> CreateRequest(const GURL& url,
                                                 const std::string& account_id,
                                                 bool sync_enabled) {
    std::unique_ptr<net::URLRequest> url_request =
        url_request_context_.CreateRequest(url, net::DEFAULT_PRIORITY, nullptr,
                                           TRAFFIC_ANNOTATION_FOR_TESTS);
    AppendOrRemoveAccountConsistentyRequestHeader(
        url_request.get(), GURL(), account_id, sync_enabled,
        cookie_settings_.get(), PROFILE_MODE_DEFAULT);
    return url_request;
  }

  void CheckAccountConsistencyHeaderRequest(
      net::URLRequest* url_request,
      const char* header_name,
      const std::string& expected_request) {
    bool expected_result = !expected_request.empty();
    std::string request;
    EXPECT_EQ(
        url_request->extra_request_headers().GetHeader(header_name, &request),
        expected_result);
    if (expected_result) {
      EXPECT_EQ(expected_request, request);
    }
  }

  void CheckMirrorHeaderRequest(const GURL& url,
                                const std::string& account_id,
                                const std::string& expected_request) {
    std::unique_ptr<net::URLRequest> url_request =
        CreateRequest(url, account_id, false /* sync_enabled */);
    CheckAccountConsistencyHeaderRequest(
        url_request.get(), kChromeConnectedHeader, expected_request);
  }

#if !defined(OS_IOS) && !defined(OS_ANDROID)
  void CheckDiceHeaderRequest(const GURL& url,
                              const std::string& account_id,
                              bool sync_enabled,
                              const std::string& expected_mirror_request,
                              const std::string& expected_dice_request) {
    std::unique_ptr<net::URLRequest> url_request =
        CreateRequest(url, account_id, sync_enabled);
    CheckAccountConsistencyHeaderRequest(
        url_request.get(), kChromeConnectedHeader, expected_mirror_request);
    CheckAccountConsistencyHeaderRequest(url_request.get(), kDiceRequestHeader,
                                         expected_dice_request);
  }
#endif

  base::MessageLoop loop_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  net::TestURLRequestContext url_request_context_;

  scoped_refptr<HostContentSettingsMap> settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
};

// Tests that no Mirror request is returned when the user is not signed in (no
// account id).
TEST_F(SigninHeaderHelperTest, TestNoMirrorRequestNoAccountId) {
  switches::EnableAccountConsistencyMirrorForTesting(
      base::CommandLine::ForCurrentProcess());
  CheckMirrorHeaderRequest(GURL("https://docs.google.com"), "", "");
  CheckMirrorCookieRequest(GURL("https://docs.google.com"), "", "");
}

// Tests that no Mirror request is returned when the cookies aren't allowed to
// be set.
TEST_F(SigninHeaderHelperTest, TestNoMirrorRequestCookieSettingBlocked) {
  switches::EnableAccountConsistencyMirrorForTesting(
      base::CommandLine::ForCurrentProcess());
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CheckMirrorHeaderRequest(GURL("https://docs.google.com"), "0123456789", "");
  CheckMirrorCookieRequest(GURL("https://docs.google.com"), "0123456789", "");
}

// Tests that no Mirror request is returned when the target is a non-Google URL.
TEST_F(SigninHeaderHelperTest, TestNoMirrorRequestExternalURL) {
  switches::EnableAccountConsistencyMirrorForTesting(
      base::CommandLine::ForCurrentProcess());
  CheckMirrorHeaderRequest(GURL("https://foo.com"), "0123456789", "");
  CheckMirrorCookieRequest(GURL("https://foo.com"), "0123456789", "");
}

// Tests that the Mirror request is returned without the GAIA Id when the target
// is a google TLD domain.
TEST_F(SigninHeaderHelperTest, TestMirrorRequestGoogleTLD) {
  switches::EnableAccountConsistencyMirrorForTesting(
      base::CommandLine::ForCurrentProcess());
  CheckMirrorHeaderRequest(GURL("https://google.fr"), "0123456789",
                           "mode=0,enable_account_consistency=true");
  CheckMirrorCookieRequest(GURL("https://google.de"), "0123456789",
                           "mode=0:enable_account_consistency=true");
}

// Tests that the Mirror request is returned when the target is the domain
// google.com, and that the GAIA Id is only attached for the cookie.
TEST_F(SigninHeaderHelperTest, TestMirrorRequestGoogleCom) {
  switches::EnableAccountConsistencyMirrorForTesting(
      base::CommandLine::ForCurrentProcess());
  CheckMirrorHeaderRequest(GURL("https://www.google.com"), "0123456789",
                           "mode=0,enable_account_consistency=true");
  CheckMirrorCookieRequest(
      GURL("https://www.google.com"), "0123456789",
      "id=0123456789:mode=0:enable_account_consistency=true");
}

// Mirror is always enabled on Android and iOS, so these tests are only relevant
// on Desktop.
#if !defined(OS_ANDROID) && !defined(OS_IOS)

// Tests that the Mirror request is returned when the target is a Gaia URL, even
// if account consistency is disabled.
TEST_F(SigninHeaderHelperTest, TestMirrorRequestGaiaURL) {
  ASSERT_FALSE(switches::IsAccountConsistencyMirrorEnabled());
  CheckMirrorHeaderRequest(GURL("https://accounts.google.com"), "0123456789",
                           "mode=0,enable_account_consistency=false");
  CheckMirrorCookieRequest(
      GURL("https://accounts.google.com"), "0123456789",
      "id=0123456789:mode=0:enable_account_consistency=false");
}

// Tests Dice requests.
TEST_F(SigninHeaderHelperTest, TestDiceRequest) {
  switches::EnableAccountConsistencyDiceForTesting(
      base::CommandLine::ForCurrentProcess());
  // ChromeConnected but no Dice for Docs URLs.
  CheckDiceHeaderRequest(
      GURL("https://docs.google.com"), "0123456789", false /* sync_enabled */,
      "id=0123456789,mode=0,enable_account_consistency=false", "");

  // ChromeConnected and Dice for Gaia URLs.
  // Sync disabled.
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  ASSERT_FALSE(client_id.empty());
  CheckDiceHeaderRequest(GURL("https://accounts.google.com"), "0123456789",
                         false /* sync_enabled */,
                         "mode=0,enable_account_consistency=false",
                         "client_id=" + client_id);
  // Sync enabled: check that the Dice header has the Sync account ID and that
  // the mirror header is not modified.
  CheckDiceHeaderRequest(
      GURL("https://accounts.google.com"), "0123456789",
      true /* sync_enabled */, "mode=0,enable_account_consistency=false",
      "client_id=" + client_id + ",sync_account_id=0123456789");

  // No ChromeConnected and no Dice for other URLs.
  CheckDiceHeaderRequest(GURL("https://www.google.com"), "0123456789",
                         false /* sync_enabled */, "", "");
}

// Tests that no Dice request is returned when Dice is not enabled.
TEST_F(SigninHeaderHelperTest, TestNoDiceRequestWhenDisabled) {
  switches::EnableAccountConsistencyMirrorForTesting(
      base::CommandLine::ForCurrentProcess());
  CheckDiceHeaderRequest(GURL("https://accounts.google.com"), "0123456789",
                         false /* sync_enabled */,
                         "mode=0,enable_account_consistency=true", "");
}

// Tests that the Mirror request is returned with the GAIA Id on Drive origin,
// even if account consistency is disabled.
TEST_F(SigninHeaderHelperTest, TestMirrorRequestDrive) {
  ASSERT_FALSE(switches::IsAccountConsistencyMirrorEnabled());
  CheckMirrorHeaderRequest(
      GURL("https://docs.google.com/document"), "0123456789",
      "id=0123456789,mode=0,enable_account_consistency=false");
  CheckMirrorCookieRequest(
      GURL("https://drive.google.com/drive"), "0123456789",
      "id=0123456789:mode=0:enable_account_consistency=false");

  // Enable Account Consistency will override the disable.
  switches::EnableAccountConsistencyMirrorForTesting(
      base::CommandLine::ForCurrentProcess());
  CheckMirrorHeaderRequest(
      GURL("https://docs.google.com/document"), "0123456789",
      "id=0123456789,mode=0,enable_account_consistency=true");
  CheckMirrorCookieRequest(
      GURL("https://drive.google.com/drive"), "0123456789",
      "id=0123456789:mode=0:enable_account_consistency=true");
}

TEST_F(SigninHeaderHelperTest, TestDiceInvalidResponseParams) {
  DiceResponseParams params = BuildDiceResponseParams("blah");
  EXPECT_EQ(DiceAction::NONE, params.user_intention);
}

TEST_F(SigninHeaderHelperTest, TestBuildDiceResponseParams) {
  const char kAuthorizationCode[] = "authorization_code";
  const char kEmail[] = "foo@example.com";
  const char kObfuscatedGaiaID[] = "obfuscated_gaia_id";
  const int kSessionIndex = 42;

  {
    // Signin response.
    DiceResponseParams params = BuildDiceResponseParams(base::StringPrintf(
        "action=SIGNIN,id=%s,email=%s,authuser=%i,authorization_code=%s",
        kObfuscatedGaiaID, kEmail, kSessionIndex, kAuthorizationCode));
    EXPECT_EQ(DiceAction::SIGNIN, params.user_intention);
    EXPECT_EQ(kObfuscatedGaiaID, params.obfuscated_gaia_id);
    EXPECT_EQ(kEmail, params.email);
    EXPECT_EQ(kSessionIndex, params.session_index);
    EXPECT_EQ(kAuthorizationCode, params.authorization_code);
  }

  {
    // Signout response.
    DiceResponseParams params = BuildDiceResponseParams(
        base::StringPrintf("action=SIGNOUT,id=%s,email=%s,authuser=%i",
                           kObfuscatedGaiaID, kEmail, kSessionIndex));
    EXPECT_EQ(DiceAction::SIGNOUT, params.user_intention);
    EXPECT_EQ(kObfuscatedGaiaID, params.obfuscated_gaia_id);
    EXPECT_EQ(kEmail, params.email);
    EXPECT_EQ(kSessionIndex, params.session_index);
    EXPECT_EQ("", params.authorization_code);
  }

  {
    // Missing authorization code.
    DiceResponseParams params = BuildDiceResponseParams(
        base::StringPrintf("action=SIGNIN,id=%s,email=%s,authuser=%i",
                           kObfuscatedGaiaID, kEmail, kSessionIndex));
    EXPECT_EQ(DiceAction::NONE, params.user_intention);
  }

  {
    // Missing non-optional field (email).
    DiceResponseParams params = BuildDiceResponseParams(base::StringPrintf(
        "action=SIGNIN,id=%s,authuser=%i,authorization_code=%s",
        kObfuscatedGaiaID, kSessionIndex, kAuthorizationCode));
    EXPECT_EQ(DiceAction::NONE, params.user_intention);
  }
}

#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

// Tests that the Mirror header request is returned normally when the redirect
// URL is eligible.
TEST_F(SigninHeaderHelperTest, TestMirrorHeaderEligibleRedirectURL) {
  switches::EnableAccountConsistencyMirrorForTesting(
      base::CommandLine::ForCurrentProcess());
  const GURL url("https://docs.google.com/document");
  const GURL redirect_url("https://www.google.com");
  const std::string account_id = "0123456789";
  std::unique_ptr<net::URLRequest> url_request =
      url_request_context_.CreateRequest(url, net::DEFAULT_PRIORITY, nullptr,
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
  AppendOrRemoveAccountConsistentyRequestHeader(
      url_request.get(), redirect_url, account_id, false /* sync_enabled */,
      cookie_settings_.get(), PROFILE_MODE_DEFAULT);
  EXPECT_TRUE(
      url_request->extra_request_headers().HasHeader(kChromeConnectedHeader));
}

// Tests that the Mirror header request is stripped when the redirect URL is not
// eligible.
TEST_F(SigninHeaderHelperTest, TestMirrorHeaderNonEligibleRedirectURL) {
  switches::EnableAccountConsistencyMirrorForTesting(
      base::CommandLine::ForCurrentProcess());
  const GURL url("https://docs.google.com/document");
  const GURL redirect_url("http://www.foo.com");
  const std::string account_id = "0123456789";
  std::unique_ptr<net::URLRequest> url_request =
      url_request_context_.CreateRequest(url, net::DEFAULT_PRIORITY, nullptr,
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
  AppendOrRemoveAccountConsistentyRequestHeader(
      url_request.get(), redirect_url, account_id, false /* sync_enabled */,
      cookie_settings_.get(), PROFILE_MODE_DEFAULT);
  EXPECT_FALSE(
      url_request->extra_request_headers().HasHeader(kChromeConnectedHeader));
}

// Tests that the Mirror header, whatever its value is, is untouched when both
// the current and the redirect URL are non-eligible.
TEST_F(SigninHeaderHelperTest, TestIgnoreMirrorHeaderNonEligibleURLs) {
  switches::EnableAccountConsistencyMirrorForTesting(
      base::CommandLine::ForCurrentProcess());
  const GURL url("https://www.bar.com");
  const GURL redirect_url("http://www.foo.com");
  const std::string account_id = "0123456789";
  const std::string fake_header = "foo,bar";
  std::unique_ptr<net::URLRequest> url_request =
      url_request_context_.CreateRequest(url, net::DEFAULT_PRIORITY, nullptr,
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
  url_request->SetExtraRequestHeaderByName(kChromeConnectedHeader, fake_header,
                                           false);
  AppendOrRemoveAccountConsistentyRequestHeader(
      url_request.get(), redirect_url, account_id, false /* sync_enabled */,
      cookie_settings_.get(), PROFILE_MODE_DEFAULT);
  std::string header;
  EXPECT_TRUE(url_request->extra_request_headers().GetHeader(
      kChromeConnectedHeader, &header));
  EXPECT_EQ(fake_header, header);
}

TEST_F(SigninHeaderHelperTest, TestInvalidManageAccountsParams) {
  ManageAccountsParams params = BuildManageAccountsParams("blah");
  EXPECT_EQ(GAIA_SERVICE_TYPE_NONE, params.service_type);
}

TEST_F(SigninHeaderHelperTest, TestBuildManageAccountsParams) {
  const char kContinueURL[] = "https://www.example.com/continue";
  const char kEmail[] = "foo@example.com";

  ManageAccountsParams params = BuildManageAccountsParams(base::StringPrintf(
      "action=REAUTH,email=%s,is_saml=true,is_same_tab=true,continue_url=%s",
      kEmail, kContinueURL));
  EXPECT_EQ(GAIA_SERVICE_TYPE_REAUTH, params.service_type);
  EXPECT_EQ(kEmail, params.email);
  EXPECT_EQ(true, params.is_saml);
  EXPECT_EQ(true, params.is_same_tab);
  EXPECT_EQ(GURL(kContinueURL), params.continue_url);
}

}  // namespace signin
