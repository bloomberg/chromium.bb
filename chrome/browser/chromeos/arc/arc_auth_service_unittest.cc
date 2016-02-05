// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/auth/arc_auth_fetcher.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

const int kThreadOptions = content::TestBrowserThreadBundle::IO_MAINLOOP;
const char kTestAuthCode[] = "4/Qa3CPIhh-WcMfWSf9HZaYcGUhEeax-F9sQK9CNRhZWs";

}  // namespace

class ArcAuthServiceTest : public testing::Test {
 public:
  ArcAuthServiceTest()
      : thread_bundle_(new content::TestBrowserThreadBundle(kThreadOptions)),
        url_fetcher_factory_(
            nullptr,
            base::Bind(&ArcAuthServiceTest::FakeURLFetcherCreator,
                       base::Unretained(this))) {}
  ~ArcAuthServiceTest() override = default;

  void SetUp() override {
    ArcAuthService::DisableUIForTesting();

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.path().AppendASCII("TestArcProfile"));

    profile_ = profile_builder.Build();
    bridge_service_.reset(new FakeArcBridgeService());
    auth_service_.reset(new ArcAuthService(bridge_service_.get()));

    // Check initial conditions.
    EXPECT_EQ(bridge_service_.get(), ArcBridgeService::Get());
    EXPECT_EQ(true, !ArcBridgeService::Get()->available());
    EXPECT_EQ(ArcBridgeService::State::STOPPED,
              ArcBridgeService::Get()->state());
  }

  void TearDown() override {}

 protected:
  Profile* profile() { return profile_.get(); }
  FakeArcBridgeService* bridge_service() { return bridge_service_.get(); }
  ArcAuthService* auth_service() { return auth_service_.get(); }
  net::FakeURLFetcherFactory& url_fetcher_factory() {
    return url_fetcher_factory_;
  }
  void set_cookie(const std::string& cookie) { rt_cookie_ = cookie; }

 private:
  scoped_ptr<net::FakeURLFetcher> FakeURLFetcherCreator(
      const GURL& url,
      net::URLFetcherDelegate* delegate,
      const std::string& response_data,
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    scoped_ptr<net::FakeURLFetcher> fetcher(new net::FakeURLFetcher(
        url, delegate, response_data, response_code, status));
    // Use cookie only once.
    if (!rt_cookie_.empty()) {
      net::ResponseCookies cookies;
      cookies.push_back(rt_cookie_);
      fetcher->set_cookies(cookies);
      rt_cookie_.clear();
    }
    return fetcher;
  }

  scoped_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  net::FakeURLFetcherFactory url_fetcher_factory_;
  scoped_ptr<arc::FakeArcBridgeService> bridge_service_;
  scoped_ptr<arc::ArcAuthService> auth_service_;
  scoped_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  std::string rt_cookie_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthServiceTest);
};

TEST_F(ArcAuthServiceTest, PrefChangeTriggersService) {
  ASSERT_EQ(ArcAuthService::State::DISABLE, auth_service()->state());

  PrefService* pref = profile()->GetPrefs();
  DCHECK_EQ(false, pref->GetBoolean(prefs::kArcEnabled));

  auth_service()->OnPrimaryUserProfilePrepared(profile());
  ASSERT_EQ(ArcAuthService::State::DISABLE, auth_service()->state());

  // Need to initialize URLFetcher for test framework.
  const GURL gaia_gurl = ArcAuthFetcher::CreateURL();
  url_fetcher_factory().SetFakeResponse(gaia_gurl, std::string(), net::HTTP_OK,
                                        net::URLRequestStatus::SUCCESS);

  pref->SetBoolean(prefs::kArcEnabled, true);
  ASSERT_EQ(ArcAuthService::State::FETCHING_CODE, auth_service()->state());

  pref->SetBoolean(prefs::kArcEnabled, false);
  ASSERT_EQ(ArcAuthService::State::DISABLE, auth_service()->state());

  // Correctly stop service.
  auth_service()->Shutdown();
}

TEST_F(ArcAuthServiceTest, BaseWorkflow) {
  ASSERT_EQ(ArcBridgeService::State::STOPPED, bridge_service()->state());
  ASSERT_EQ(ArcAuthService::State::DISABLE, auth_service()->state());
  ASSERT_EQ(std::string(), auth_service()->GetAndResetAuthCode());

  const GURL gaia_gurl = ArcAuthFetcher::CreateURL();
  url_fetcher_factory().SetFakeResponse(gaia_gurl, std::string(), net::HTTP_OK,
                                        net::URLRequestStatus::SUCCESS);
  std::string cookie = "oauth_code=";
  cookie += kTestAuthCode;
  cookie += "; Path=/o/oauth2/programmatic_auth; Secure; HttpOnly";
  set_cookie(cookie);
  auth_service()->OnPrimaryUserProfilePrepared(profile());

  // By default ARC is not enabled.
  ASSERT_EQ(ArcAuthService::State::DISABLE, auth_service()->state());

  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);

  // Setting profile and pref initiates a code fetching process.
  ASSERT_EQ(ArcAuthService::State::FETCHING_CODE, auth_service()->state());

  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(ArcAuthService::State::ENABLE, auth_service()->state());
  ASSERT_EQ(ArcBridgeService::State::READY, bridge_service()->state());
  // Auth code valid only for one call.
  ASSERT_EQ(kTestAuthCode, auth_service()->GetAndResetAuthCode());
  ASSERT_EQ(std::string(), auth_service()->GetAndResetAuthCode());

  auth_service()->Shutdown();
  ASSERT_EQ(ArcAuthService::State::DISABLE, auth_service()->state());
  ASSERT_EQ(ArcBridgeService::State::STOPPED, bridge_service()->state());
  ASSERT_EQ(std::string(), auth_service()->GetAndResetAuthCode());

  // Send profile and don't provide a code.
  auth_service()->OnPrimaryUserProfilePrepared(profile());

  // Setting profile initiates a code fetching process.
  ASSERT_EQ(ArcAuthService::State::FETCHING_CODE, auth_service()->state());

  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  // UI is disabled in unit tests and this code is unchanged.
  ASSERT_EQ(ArcAuthService::State::FETCHING_CODE, auth_service()->state());

  // Send error response.
  url_fetcher_factory().SetFakeResponse(gaia_gurl, std::string(),
                                        net::HTTP_BAD_REQUEST,
                                        net::URLRequestStatus::SUCCESS);
  auth_service()->Shutdown();
  ASSERT_EQ(ArcAuthService::State::DISABLE, auth_service()->state());
  auth_service()->OnPrimaryUserProfilePrepared(profile());

  ASSERT_EQ(ArcAuthService::State::FETCHING_CODE, auth_service()->state());
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(ArcAuthService::State::NO_CODE, auth_service()->state());

  // Correctly stop service.
  auth_service()->Shutdown();
}

}  // namespace arc
