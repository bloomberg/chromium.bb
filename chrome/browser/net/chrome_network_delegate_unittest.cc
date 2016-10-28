// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/net/safe_search_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/data_usage/core/data_use_aggregator.h"
#include "components/data_usage/core/data_use_amortizer.h"
#include "components/data_usage/core/data_use_annotator.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/prefs/pref_member.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/event_router_forwarder.h"
#endif

namespace {

// This function requests a URL, and makes it return a known response. If
// |from_user| is true, it attaches a ResourceRequestInfo to the URLRequest,
// because requests from users have this info. If |from_user| is false, the
// request is presumed to be from a service, and the service name is set in the
// request's user data. (As an example suggestions service tag is attached). if
// |redirect| is true, it adds necessary socket data to have it follow redirect
// before getting the final response.
std::unique_ptr<net::URLRequest> RequestURL(
    net::URLRequestContext* context,
    net::MockClientSocketFactory* socket_factory,
    bool from_user,
    bool redirect) {
  net::MockRead redirect_mock_reads[] = {
      net::MockRead("HTTP/1.1 302 Found\r\n"
                    "Location: http://bar.com/\r\n\r\n"),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider redirect_socket_data_provider(
      redirect_mock_reads, arraysize(redirect_mock_reads), nullptr, 0);

  if (redirect)
    socket_factory->AddSocketDataProvider(&redirect_socket_data_provider);
  net::MockRead response_mock_reads[] = {
      net::MockRead("HTTP/1.1 200 OK\r\n\r\n"), net::MockRead("response body"),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider response_socket_data_provider(
      response_mock_reads, arraysize(response_mock_reads), nullptr, 0);
  socket_factory->AddSocketDataProvider(&response_socket_data_provider);
  net::TestDelegate test_delegate;
  test_delegate.set_quit_on_complete(true);
  std::unique_ptr<net::URLRequest> request(context->CreateRequest(
      GURL("http://example.com"), net::DEFAULT_PRIORITY, &test_delegate));

  if (from_user) {
    content::ResourceRequestInfo::AllocateForTesting(
        request.get(), content::RESOURCE_TYPE_MAIN_FRAME, nullptr, -2, -2, -2,
        true, false, true, true, false);
  } else {
    request->SetUserData(
        data_use_measurement::DataUseUserData::kUserDataKey,
        new data_use_measurement::DataUseUserData(
            data_use_measurement::DataUseUserData::SUGGESTIONS,
            data_use_measurement::DataUseUserData::FOREGROUND));
  }
  request->Start();
  base::RunLoop().RunUntilIdle();
  return request;
}

// A fake DataUseAggregator for testing that only counts how many times its
// respective methods have been called.
class FakeDataUseAggregator : public data_usage::DataUseAggregator {
 public:
  FakeDataUseAggregator()
      : data_usage::DataUseAggregator(
            std::unique_ptr<data_usage::DataUseAnnotator>(),
            std::unique_ptr<data_usage::DataUseAmortizer>()),
        on_the_record_tx_bytes_(0),
        on_the_record_rx_bytes_(0),
        off_the_record_tx_bytes_(0),
        off_the_record_rx_bytes_(0) {}
  ~FakeDataUseAggregator() override {}

  void ReportDataUse(net::URLRequest* request,
                     int64_t tx_bytes,
                     int64_t rx_bytes) override {
    on_the_record_tx_bytes_ += tx_bytes;
    on_the_record_rx_bytes_ += rx_bytes;
  }

  void ReportOffTheRecordDataUse(int64_t tx_bytes, int64_t rx_bytes) override {
    off_the_record_tx_bytes_ += tx_bytes;
    off_the_record_rx_bytes_ += rx_bytes;
  }

  int64_t on_the_record_tx_bytes() const { return on_the_record_tx_bytes_; }
  int64_t on_the_record_rx_bytes() const { return on_the_record_rx_bytes_; }
  int64_t off_the_record_tx_bytes() const { return off_the_record_tx_bytes_; }
  int64_t off_the_record_rx_bytes() const { return off_the_record_rx_bytes_; }

 private:
  int64_t on_the_record_tx_bytes_;
  int64_t on_the_record_rx_bytes_;
  int64_t off_the_record_tx_bytes_;
  int64_t off_the_record_rx_bytes_;
};

}  // namespace

class ChromeNetworkDelegateTest : public testing::Test {
 public:
  ChromeNetworkDelegateTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        context_(new net::TestURLRequestContext(true)) {
#if defined(ENABLE_EXTENSIONS)
    forwarder_ = new extensions::EventRouterForwarder();
#endif
  }

  void SetUp() override {
    ChromeNetworkDelegate::InitializePrefsOnUIThread(
        &enable_referrers_, nullptr, nullptr, nullptr, nullptr,
        profile_.GetTestingPrefService());
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
  }

  void Initialize() {
    network_delegate_.reset(
        new ChromeNetworkDelegate(forwarder(), &enable_referrers_,
                                  metrics::UpdateUsagePrefCallbackType()));
    context_->set_client_socket_factory(&socket_factory_);
    context_->set_network_delegate(network_delegate_.get());
    context_->Init();
  }

  net::TestURLRequestContext* context() { return context_.get(); }
  net::NetworkDelegate* network_delegate() { return network_delegate_.get(); }
  net::MockClientSocketFactory* socket_factory() { return &socket_factory_; }

  ChromeNetworkDelegate* chrome_network_delegate() {
    return network_delegate_.get();
  }

  extensions::EventRouterForwarder* forwarder() {
#if defined(ENABLE_EXTENSIONS)
    return forwarder_.get();
#else
    return nullptr;
#endif
  }

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  content::TestBrowserThreadBundle thread_bundle_;
#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> forwarder_;
#endif
  TestingProfile profile_;
  BooleanPrefMember enable_referrers_;
  std::unique_ptr<ChromeNetworkDelegate> network_delegate_;
  net::MockClientSocketFactory socket_factory_;
  std::unique_ptr<net::TestURLRequestContext> context_;
};

// Test that the total data use consumed by Chrome is recorded correctly.
TEST_F(ChromeNetworkDelegateTest, TotalDataUseMeasurementTest) {
  Initialize();
  base::HistogramTester histogram_tester;

  // A query from a user without redirection.
  RequestURL(context(), socket_factory(), true, false);
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("DataUse.BytesSent.Delegate");
  EXPECT_FALSE(buckets.empty());

  buckets = histogram_tester.GetAllSamples("DataUse.BytesReceived.Delegate");
  EXPECT_FALSE(buckets.empty());
}

// This function tests data use measurement for requests by services. it makes a
// query which is similar to a query of a service, so it should affect
// DataUse.TrafficSize.System.Dimensions and DataUse.MessageSize.ServiceName
// histograms. AppState and ConnectionType dimensions are always Foreground and
// NotCellular respectively.
TEST_F(ChromeNetworkDelegateTest, DataUseMeasurementServiceTest) {
  Initialize();
  base::HistogramTester histogram_tester;

  // A query from a service without redirection.
  RequestURL(context(), socket_factory(), false, false);
  EXPECT_FALSE(
      histogram_tester
          .GetTotalCountsForPrefix(
              "DataUse.TrafficSize.System.Downstream.Foreground.NotCellular")
          .empty());
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.System.Upstream.Foreground.NotCellular", 1);
  EXPECT_FALSE(histogram_tester
                   .GetTotalCountsForPrefix("DataUse.MessageSize.Suggestions")
                   .empty());
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.User.Downstream.Foreground.NotCellular", 0);
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.User.Upstream.Foreground.NotCellular", 0);
}

// This function tests data use measurement for requests by user.The query from
// a user should affect DataUse.TrafficSize.User.Dimensions histogram. AppState
// and ConnectionType dimensions are always Foreground and NotCellular
// respectively.
TEST_F(ChromeNetworkDelegateTest, DataUseMeasurementUserTest) {
  Initialize();
  base::HistogramTester histogram_tester;

  // A query from user without redirection.
  RequestURL(context(), socket_factory(), true, false);
  EXPECT_FALSE(
      histogram_tester
          .GetTotalCountsForPrefix(
              "DataUse.TrafficSize.User.Downstream.Foreground.NotCellular")
          .empty());
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.User.Upstream.Foreground.NotCellular", 1);
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.System.Downstream.Foreground.NotCellular", 0);
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.System.Upstream.Foreground.NotCellular", 0);
  histogram_tester.ExpectTotalCount("DataUse.MessageSize.Suggestions", 0);
}

// This function tests data use measurement for requests by services in case the
// request is redirected once. it makes a query which is similar to a query of a
// service, so it should affect DataUse.TrafficSize.System.Dimensions and
// DataUse.MessageSize.ServiceName histograms. AppState and ConnectionType
// dimensions are always Foreground and NotCellular respectively.
TEST_F(ChromeNetworkDelegateTest, DataUseMeasurementServiceTestWithRedirect) {
  Initialize();
  base::HistogramTester histogram_tester;

  // A query from user with one redirection.
  RequestURL(context(), socket_factory(), false, true);
  EXPECT_FALSE(
      histogram_tester
          .GetTotalCountsForPrefix(
              "DataUse.TrafficSize.System.Downstream.Foreground.NotCellular")
          .empty());
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.System.Upstream.Foreground.NotCellular", 2);
  // Two uploads and two downloads message, so totalCount should be 4.
  EXPECT_FALSE(histogram_tester
                   .GetTotalCountsForPrefix("DataUse.MessageSize.Suggestions")
                   .empty());
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.User.Downstream.Foreground.NotCellular", 0);
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.User.Upstream.Foreground.NotCellular", 0);
}

// This function tests data use measurement for requests by user in case the
// request is redirected once.The query from a user should affect
// DataUse.TrafficSize.User.Dimensions histogram. AppState and ConnectionType
// dimensions are always Foreground and NotCellular respectively.
TEST_F(ChromeNetworkDelegateTest, DataUseMeasurementUserTestWithRedirect) {
  Initialize();
  base::HistogramTester histogram_tester;

  // A query from user with one redirection.
  RequestURL(context(), socket_factory(), true, true);

  EXPECT_FALSE(
      histogram_tester
          .GetTotalCountsForPrefix(
              "DataUse.TrafficSize.User.Downstream.Foreground.NotCellular")
          .empty());
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.User.Upstream.Foreground.NotCellular", 2);
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.System.Downstream.Foreground.NotCellular", 0);
  histogram_tester.ExpectTotalCount(
      "DataUse.TrafficSize.System.Upstream.Foreground.NotCellular", 0);
  histogram_tester.ExpectTotalCount("DataUse.MessageSize.Suggestions", 0);
}

TEST_F(ChromeNetworkDelegateTest, DisableSameSiteCookiesIffFlagDisabled) {
  Initialize();
  EXPECT_FALSE(network_delegate()->AreExperimentalCookieFeaturesEnabled());
}

TEST_F(ChromeNetworkDelegateTest, EnableSameSiteCookiesIffFlagEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  Initialize();
  EXPECT_TRUE(network_delegate()->AreExperimentalCookieFeaturesEnabled());
}

TEST_F(ChromeNetworkDelegateTest, ReportDataUseToAggregator) {
  FakeDataUseAggregator fake_aggregator;
  Initialize();

  chrome_network_delegate()->set_data_use_aggregator(
      &fake_aggregator, false /* is_data_usage_off_the_record */);

  std::unique_ptr<net::URLRequest> request =
      RequestURL(context(), socket_factory(), true, false);
  EXPECT_EQ(request->GetTotalSentBytes(),
            fake_aggregator.on_the_record_tx_bytes());
  EXPECT_EQ(request->GetTotalReceivedBytes(),
            fake_aggregator.on_the_record_rx_bytes());
  EXPECT_EQ(0, fake_aggregator.off_the_record_tx_bytes());
  EXPECT_EQ(0, fake_aggregator.off_the_record_rx_bytes());
}

TEST_F(ChromeNetworkDelegateTest, ReportOffTheRecordDataUseToAggregator) {
  FakeDataUseAggregator fake_aggregator;
  Initialize();

  chrome_network_delegate()->set_data_use_aggregator(
      &fake_aggregator, true /* is_data_usage_off_the_record */);
  std::unique_ptr<net::URLRequest> request =
      RequestURL(context(), socket_factory(), true, false);

  EXPECT_EQ(0, fake_aggregator.on_the_record_tx_bytes());
  EXPECT_EQ(0, fake_aggregator.on_the_record_rx_bytes());
  EXPECT_EQ(request->GetTotalSentBytes(),
            fake_aggregator.off_the_record_tx_bytes());
  EXPECT_EQ(request->GetTotalReceivedBytes(),
            fake_aggregator.off_the_record_rx_bytes());
}

class ChromeNetworkDelegatePolicyTest : public testing::Test {
 public:
  ChromeNetworkDelegatePolicyTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
#if defined(ENABLE_EXTENSIONS)
    forwarder_ = new extensions::EventRouterForwarder();
#endif
  }

 protected:
   void SetDelegate(net::NetworkDelegate* delegate) {
     context_.set_network_delegate(delegate);
   }

  extensions::EventRouterForwarder* forwarder() {
#if defined(ENABLE_EXTENSIONS)
    return forwarder_.get();
#else
    return nullptr;
#endif
  }

  content::TestBrowserThreadBundle thread_bundle_;
#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> forwarder_;
#endif
  TestingProfile profile_;
  BooleanPrefMember enable_referrers_;
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  DISALLOW_COPY_AND_ASSIGN(ChromeNetworkDelegatePolicyTest);
};

class ChromeNetworkDelegateSafeSearchTest :
    public ChromeNetworkDelegatePolicyTest {
 public:
  void SetUp() override {
    ChromeNetworkDelegate::InitializePrefsOnUIThread(
        &enable_referrers_,
        nullptr,
        &force_google_safe_search_,
        &force_youtube_restrict_,
        nullptr,
        profile_.GetTestingPrefService());
  }

 protected:
  std::unique_ptr<net::NetworkDelegate> CreateNetworkDelegate() {
    std::unique_ptr<ChromeNetworkDelegate> network_delegate(
        new ChromeNetworkDelegate(forwarder(), &enable_referrers_,
                                  metrics::UpdateUsagePrefCallbackType()));
    network_delegate->set_force_google_safe_search(&force_google_safe_search_);
    network_delegate->set_force_youtube_restrict(&force_youtube_restrict_);
    return std::move(network_delegate);
  }

  void SetSafeSearch(bool google_safe_search, int youtube_restrict) {
    force_google_safe_search_.SetValue(google_safe_search);
    force_youtube_restrict_.SetValue(youtube_restrict);
  }

  // Does a request to an arbitrary URL and verifies that the SafeSearch
  // enforcement utility functions were called/not called as expected.
  void QueryURL(bool expect_google_safe_search, bool expect_youtube_restrict) {
    safe_search_util::ClearForceGoogleSafeSearchCountForTesting();
    safe_search_util::ClearForceYouTubeRestrictCountForTesting();

    std::unique_ptr<net::URLRequest> request(context_.CreateRequest(
        GURL("http://anyurl.com"), net::DEFAULT_PRIORITY, &delegate_));

    request->Start();
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(expect_google_safe_search ? 1 : 0,
              safe_search_util::GetForceGoogleSafeSearchCountForTesting());
    EXPECT_EQ(expect_youtube_restrict ? 1 : 0,
              safe_search_util::GetForceYouTubeRestrictCountForTesting());
  }

 private:
  BooleanPrefMember force_google_safe_search_;
  IntegerPrefMember force_youtube_restrict_;
};

TEST_F(ChromeNetworkDelegateSafeSearchTest, SafeSearch) {
  std::unique_ptr<net::NetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  static_assert(safe_search_util::YOUTUBE_RESTRICT_OFF      == 0 &&
                safe_search_util::YOUTUBE_RESTRICT_MODERATE == 1 &&
                safe_search_util::YOUTUBE_RESTRICT_STRICT   == 2 &&
                safe_search_util::YOUTUBE_RESTRICT_COUNT    == 3,
                "This test relies on mapping ints to enum values.");

  // Loop over all combinations of the two policies.
  for (int i = 0; i < 6; i++) {
    bool google_safe_search = (i / 3) != 0;
    int youtube_restrict = i % 3;
    SetSafeSearch(google_safe_search, youtube_restrict);

    QueryURL(google_safe_search, youtube_restrict != 0);
  }
}

class ChromeNetworkDelegateAllowedDomainsTest :
    public ChromeNetworkDelegatePolicyTest {
 public:

  void SetUp() override {
    ChromeNetworkDelegate::InitializePrefsOnUIThread(
        &enable_referrers_,
        nullptr,
        nullptr,
        nullptr,
        &allowed_domains_for_apps_,
        profile_.GetTestingPrefService());
  }

 protected:
  std::unique_ptr<net::NetworkDelegate> CreateNetworkDelegate() {
    std::unique_ptr<ChromeNetworkDelegate> network_delegate(
        new ChromeNetworkDelegate(forwarder(), &enable_referrers_,
                                  metrics::UpdateUsagePrefCallbackType()));
    network_delegate->set_allowed_domains_for_apps(&allowed_domains_for_apps_);
    return std::move(network_delegate);
  }

  // Will set the AllowedDomainsForApps policy to have the value of |allowed|.
  // Will make a request to |url| and check the headers in request.
  // If |expected| is passed as false, this routine verifies that no
  // X-GoogApps-Allowed-Domains header is set. If |expected| is passed as true,
  // this routine verifies that the X-GoogApps-Allowed-Domains header is set and
  // the value is identical to |allowed|.
  void CheckAllowedDomainsHeaders(const std::string& allowed,
                                  const GURL& url,
                                  bool expected) {
    allowed_domains_for_apps_.SetValue(allowed);

    std::unique_ptr<net::URLRequest> request(context_.CreateRequest(
        url, net::DEFAULT_PRIORITY, &delegate_));

    request->Start();
    base::RunLoop().RunUntilIdle();

    net::HttpRequestHeaders request_headers = request->extra_request_headers();

    const char allowed_domains_header_name[] = "X-GoogApps-Allowed-Domains";
    EXPECT_EQ(expected, request_headers.HasHeader(allowed_domains_header_name));

    if (expected) {
      std::string header_value;
      request_headers.GetHeader(allowed_domains_header_name, &header_value);
      EXPECT_EQ(allowed, header_value);
    }
  }

 private:
  StringPrefMember allowed_domains_for_apps_;
};

// Test the use case when the AllowedDomainsForApps policy is set and
// a request is done to Google servers. We expect the request
// headers to contain X-GoogApps-Allowed-Domains key and its value to be equal
// to the value from the policy.
TEST_F(ChromeNetworkDelegateAllowedDomainsTest, AllowedDomainsIncluded) {
  std::unique_ptr<net::NetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  CheckAllowedDomainsHeaders("gmail.com,mit.edu", GURL("http://google.com"),
                             true);
}

// Test the use case when the AllowedDomainsForApps policy is empty and
// a request is done to Google servers. We expect the request
// headers to not contain X-GoogApps-Allowed-Domains key because the policy
// is not set.
TEST_F(ChromeNetworkDelegateAllowedDomainsTest, AllowedDomainsEmpty) {
  std::unique_ptr<net::NetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());
  CheckAllowedDomainsHeaders("", GURL("http://google.com"), false);
}

// Test the use case when the AllowedDomainsForApps policy is set and
// a request is done to a non-Google domain. We expect the request
// headers to not contain X-GoogApps-Allowed-Domains key because the
// accessed URL is not from google.com domain.
TEST_F(ChromeNetworkDelegateAllowedDomainsTest, AllowedDomainsNonGoogleUrl) {
  std::unique_ptr<net::NetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());
  CheckAllowedDomainsHeaders("google.com", GURL("http://example.com"), false);
}

// Privacy Mode disables Channel Id if cookies are blocked (cr223191)
class ChromeNetworkDelegatePrivacyModeTest : public testing::Test {
 public:
  ChromeNetworkDelegatePrivacyModeTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
#if defined(ENABLE_EXTENSIONS)
        forwarder_(new extensions::EventRouterForwarder()),
#endif
        cookie_settings_(CookieSettingsFactory::GetForProfile(&profile_).get()),
        kBlockedSite("http://ads.thirdparty.com"),
        kAllowedSite("http://good.allays.com"),
        kFirstPartySite("http://cool.things.com"),
        kBlockedFirstPartySite("http://no.thirdparties.com") {
  }

  void SetUp() override {
    ChromeNetworkDelegate::InitializePrefsOnUIThread(
        &enable_referrers_, nullptr, nullptr, nullptr, nullptr,
        profile_.GetTestingPrefService());
  }

 protected:
  std::unique_ptr<ChromeNetworkDelegate> CreateNetworkDelegate() {
    std::unique_ptr<ChromeNetworkDelegate> network_delegate(
        new ChromeNetworkDelegate(forwarder(), &enable_referrers_,
                                  metrics::UpdateUsagePrefCallbackType()));
    network_delegate->set_cookie_settings(cookie_settings_);
    return network_delegate;
  }

  void SetDelegate(net::NetworkDelegate* delegate) {
    network_delegate_ = delegate;
    context_.set_network_delegate(network_delegate_);
  }

 protected:
  extensions::EventRouterForwarder* forwarder() {
#if defined(ENABLE_EXTENSIONS)
    return forwarder_.get();
#else
    return NULL;
#endif
  }

  content::TestBrowserThreadBundle thread_bundle_;
#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> forwarder_;
#endif
  TestingProfile profile_;
  content_settings::CookieSettings* cookie_settings_;
  BooleanPrefMember enable_referrers_;
  std::unique_ptr<net::URLRequest> request_;
  net::TestURLRequestContext context_;
  net::NetworkDelegate* network_delegate_;

  const GURL kBlockedSite;
  const GURL kAllowedSite;
  const GURL kEmptyFirstPartySite;
  const GURL kFirstPartySite;
  const GURL kBlockedFirstPartySite;
};

TEST_F(ChromeNetworkDelegatePrivacyModeTest, DisablePrivacyIfCookiesAllowed) {
  std::unique_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kEmptyFirstPartySite));
}


TEST_F(ChromeNetworkDelegatePrivacyModeTest, EnablePrivacyIfCookiesBlocked) {
  std::unique_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kBlockedSite,
                                                       kEmptyFirstPartySite));

  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(network_delegate_->CanEnablePrivacyMode(kBlockedSite,
                                                      kEmptyFirstPartySite));
}

TEST_F(ChromeNetworkDelegatePrivacyModeTest, EnablePrivacyIfThirdPartyBlocked) {
  std::unique_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kFirstPartySite));

  profile_.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_TRUE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                      kFirstPartySite));
  profile_.GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, false);
  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kFirstPartySite));
}

TEST_F(ChromeNetworkDelegatePrivacyModeTest,
       DisablePrivacyIfOnlyFirstPartyBlocked) {
  std::unique_ptr<ChromeNetworkDelegate> delegate(CreateNetworkDelegate());
  SetDelegate(delegate.get());

  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kBlockedFirstPartySite));

  cookie_settings_->SetCookieSetting(kBlockedFirstPartySite,
                                     CONTENT_SETTING_BLOCK);
  // Privacy mode is disabled as kAllowedSite is still getting cookies
  EXPECT_FALSE(network_delegate_->CanEnablePrivacyMode(kAllowedSite,
                                                       kBlockedFirstPartySite));
}
