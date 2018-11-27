// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/policy/cloud/policy_header_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_header_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

const char kTestPolicyHeader[] = "test_header";
const char kServerRedirectUrl[] = "/server-redirect";

enum class NetworkServiceState {
  kDisabled,
  kEnabled,
};

}  // namespace

// Use a test class with SetUpCommandLine to ensure the flag is sent to the
// first renderer process.
class ChromeContentBrowserClientBrowserTest : public InProcessBrowserTest {
 public:
  ChromeContentBrowserClientBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClientBrowserTest);
};

// Test that a basic navigation works in --site-per-process mode.  This prevents
// regressions when that mode calls out into the ChromeContentBrowserClient,
// such as http://crbug.com/164223.
IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       SitePerProcessNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/title1.html"));

  ui_test_utils::NavigateToURL(browser(), url);
  NavigationEntry* entry = browser()
                               ->tab_strip_model()
                               ->GetWebContentsAt(0)
                               ->GetController()
                               .GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_EQ(url, entry->GetVirtualURL());
}

// Helper class to mark "https://ntp.com/" as an isolated origin.
class IsolatedOriginNTPBrowserTest : public InProcessBrowserTest,
                                     public InstantTestBase {
 public:
  IsolatedOriginNTPBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(https_test_server().InitializeAndListen());

    // Mark ntp.com (with an appropriate port from the test server) as an
    // isolated origin.
    GURL isolated_url(https_test_server().GetURL("ntp.com", "/"));
    command_line->AppendSwitchASCII(switches::kIsolateOrigins,
                                    isolated_url.spec());
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server().StartAcceptingConnections();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IsolatedOriginNTPBrowserTest);
};

// Verifies that when the remote NTP URL has an origin which is also marked as
// an isolated origin (i.e., requiring a dedicated process), the NTP URL
// still loads successfully, and the resulting process is marked as an Instant
// process.  See https://crbug.com/755595.
IN_PROC_BROWSER_TEST_F(IsolatedOriginNTPBrowserTest,
                       IsolatedOriginDoesNotInterfereWithNTP) {
  GURL base_url =
      https_test_server().GetURL("ntp.com", "/instant_extended.html");
  GURL ntp_url =
      https_test_server().GetURL("ntp.com", "/instant_extended_ntp.html");
  InstantTestBase::Init(base_url, ntp_url, false);

  SetupInstant(browser());

  // Sanity check that a SiteInstance for a generic ntp.com URL requires a
  // dedicated process.
  content::BrowserContext* context = browser()->profile();
  GURL isolated_url(https_test_server().GetURL("ntp.com", "/title1.html"));
  scoped_refptr<SiteInstance> site_instance =
      SiteInstance::CreateForURL(context, isolated_url);
  EXPECT_TRUE(site_instance->RequiresDedicatedProcess());

  // The site URL for the NTP URL should resolve to a chrome-search:// URL via
  // GetEffectiveURL(), even if the NTP URL matches an isolated origin.
  GURL site_url(content::SiteInstance::GetSiteForURL(context, ntp_url));
  EXPECT_TRUE(site_url.SchemeIs(chrome::kChromeSearchScheme));

  // Navigate to the NTP URL and verify that the resulting process is marked as
  // an Instant process.
  ui_test_utils::NavigateToURL(browser(), ntp_url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(instant_service->IsInstantProcess(
      contents->GetMainFrame()->GetProcess()->GetID()));

  // Navigating to a non-NTP URL on ntp.com should not result in an Instant
  // process.
  ui_test_utils::NavigateToURL(browser(), isolated_url);
  EXPECT_FALSE(instant_service->IsInstantProcess(
      contents->GetMainFrame()->GetProcess()->GetID()));
}

// Helper class to mark "https://ntp.com/" as an isolated origin.
class SitePerProcessMemoryThresholdBrowserTest : public InProcessBrowserTest {
 public:
  SitePerProcessMemoryThresholdBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    // This way the test always sees the same amount of physical memory
    // (kLowMemoryDeviceThresholdMB = 512MB), regardless of how much memory is
    // available in the testing environment.
    command_line->AppendSwitch(switches::kEnableLowEndDeviceMode);
    EXPECT_EQ(512, base::SysInfo::AmountOfPhysicalMemoryMB());
  }

  // Some command-line switches override field trials - the tests need to be
  // skipped in this case.
  bool ShouldSkipBecauseOfConflictingCommandLineSwitches() {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSitePerProcess))
      return true;

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableSiteIsolation))
      return true;

    return false;
  }

 protected:
  // These are the origins we expect to be returned by
  // content::SiteIsolationPolicy::GetIsolatedOrigins() even if
  // ContentBrowserClient::ShouldDisableSiteIsolation() returns true.
  const std::vector<url::Origin> kExpectedEmbedderOrigins = {
#if !defined(OS_ANDROID)
    url::Origin::Create(GaiaUrls::GetInstance()->gaia_url())
#endif
  };

#if defined(OS_ANDROID)
  // On Android we don't expect any trial origins because the 512MB
  // physical memory used for testing is below the Android specific
  // hardcoded 1024MB memory limit that disables site isolation.
  const std::size_t kExpectedTrialOrigins = 0;
#else
  // All other platforms expect the single trial origin to be returned because
  // they don't have the memory limit that disables site isolation.
  const std::size_t kExpectedTrialOrigins = 1;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(SitePerProcessMemoryThresholdBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       SitePerProcessEnabled_HighThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // 512MB of physical memory that the test simulates is below the 768MB
  // threshold.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "768"}});

  base::test::ScopedFeatureList site_per_process;
  site_per_process.InitAndEnableFeature(features::kSitePerProcess);

  // Despite enabled site-per-process trial, there should be no isolation
  // because the device has too little memory.
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       SitePerProcessEnabled_LowThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // 512MB of physical memory that the test simulates is above the 128MB
  // threshold.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "128"}});

  base::test::ScopedFeatureList site_per_process;
  site_per_process.InitAndEnableFeature(features::kSitePerProcess);

  // site-per-process trial is enabled, and the memory threshold is above the
  // memory present on the device.
  EXPECT_TRUE(content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       SitePerProcessEnabled_NoThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  base::test::ScopedFeatureList site_per_process;
  site_per_process.InitAndEnableFeature(features::kSitePerProcess);

#if defined(OS_ANDROID)
  // Expect false on Android because 512MB physical memory triggered by
  // kEnableLowEndDeviceMode in SetUpCommandLine() is below the 1024MB Android
  // specific memory limit which disbles site isolation for all sites.
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
#else
  EXPECT_TRUE(content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
#endif
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       SitePerProcessDisabled_HighThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // 512MB of physical memory that the test simulates is below the 768MB
  // threshold.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "768"}});

  base::test::ScopedFeatureList site_per_process;
  site_per_process.InitAndDisableFeature(features::kSitePerProcess);

  // site-per-process trial is disabled, so isolation should be disabled
  // (i.e. the memory threshold should be ignored).
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       SitePerProcessDisabled_LowThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // 512MB of physical memory that the test simulates is above the 128MB
  // threshold.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "128"}});

  base::test::ScopedFeatureList site_per_process;
  site_per_process.InitAndDisableFeature(features::kSitePerProcess);

  // site-per-process trial is disabled, so isolation should be disabled
  // (i.e. the memory threshold should be ignored).
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       SitePerProcessDisabled_NoThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  base::test::ScopedFeatureList site_per_process;
  site_per_process.InitAndDisableFeature(features::kSitePerProcess);

  // site-per-process trial is disabled, so isolation should be disabled
  // (i.e. the memory threshold should be ignored).
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       TrialIsolatedOrigins_HighThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // 512MB of physical memory that the test simulates is below the 768MB
  // threshold.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "768"}});

  const url::Origin trial_origin = url::Origin::Create(GURL("http://foo.com/"));
  base::test::ScopedFeatureList isolated_origins_feature;
  isolated_origins_feature.InitAndEnableFeatureWithParameters(
      features::kIsolateOrigins, {{features::kIsolateOriginsFieldTrialParamName,
                                   trial_origin.Serialize()}});

  std::vector<url::Origin> isolated_origins =
      content::SiteIsolationPolicy::GetIsolatedOrigins();
  EXPECT_EQ(kExpectedTrialOrigins, isolated_origins.size());

  // Verify that the expected embedder origins are present even though site
  // isolation has been disabled and the trial origins should not be present.
  EXPECT_THAT(kExpectedEmbedderOrigins,
              ::testing::IsSubsetOf(isolated_origins));

  // Verify that the trial origin is not present.
  EXPECT_THAT(isolated_origins,
              ::testing::Not(::testing::Contains(trial_origin)));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       TrialIsolatedOrigins_LowThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // 512MB of physical memory that the test simulates is above the 128MB
  // threshold.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "128"}});

  const url::Origin trial_origin = url::Origin::Create(GURL("http://foo.com/"));
  base::test::ScopedFeatureList isolated_origins_feature;
  isolated_origins_feature.InitAndEnableFeatureWithParameters(
      features::kIsolateOrigins, {{features::kIsolateOriginsFieldTrialParamName,
                                   trial_origin.Serialize()}});

  std::vector<url::Origin> isolated_origins =
      content::SiteIsolationPolicy::GetIsolatedOrigins();
  EXPECT_EQ(1u + kExpectedEmbedderOrigins.size(), isolated_origins.size());
  EXPECT_THAT(kExpectedEmbedderOrigins,
              ::testing::IsSubsetOf(isolated_origins));

  // Verify that the trial origin is present.
  EXPECT_THAT(isolated_origins, ::testing::Contains(trial_origin));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessMemoryThresholdBrowserTest,
                       TrialIsolatedOrigins_NoThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  const url::Origin trial_origin = url::Origin::Create(GURL("http://foo.com/"));
  base::test::ScopedFeatureList isolated_origins_feature;
  isolated_origins_feature.InitAndEnableFeatureWithParameters(
      features::kIsolateOrigins, {{features::kIsolateOriginsFieldTrialParamName,
                                   trial_origin.Serialize()}});

  std::vector<url::Origin> isolated_origins =
      content::SiteIsolationPolicy::GetIsolatedOrigins();
  EXPECT_EQ(kExpectedTrialOrigins + kExpectedEmbedderOrigins.size(),
            isolated_origins.size());
  EXPECT_THAT(kExpectedEmbedderOrigins,
              ::testing::IsSubsetOf(isolated_origins));

  if (kExpectedTrialOrigins > 0) {
    // Verify that the trial origin is present.
    EXPECT_THAT(isolated_origins, ::testing::Contains(trial_origin));
  } else {
    EXPECT_THAT(isolated_origins,
                ::testing::Not(::testing::Contains(trial_origin)));
  }
}

class PolicyHeaderServiceBrowserTest : public InProcessBrowserTest {
 public:
  PolicyHeaderServiceBrowserTest() = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&PolicyHeaderServiceBrowserTest::HandleTestRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Forge a dummy DMServer URL.
    dm_url_ = embedded_test_server()->GetURL("/DeviceManagement");

    // At this point, the Profile is already initialized and it's too
    // late to set the DMServer URL via command line flags, so directly
    // inject it to the PolicyHeaderService.
    policy::PolicyHeaderService* policy_header_service =
        policy::PolicyHeaderServiceFactory::GetForBrowserContext(
            browser()->profile());
    policy_header_service->SetServerURLForTest(dm_url_.spec());
    policy_header_service->SetHeaderForTest(kTestPolicyHeader);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleTestRequest(
      const net::test_server::HttpRequest& request) {
    last_request_headers_ = request.headers;

    if (base::StartsWith(request.relative_url, kServerRedirectUrl,
                         base::CompareCase::SENSITIVE)) {
      // Extract the target URL and redirect there.
      size_t query_string_pos = request.relative_url.find('?');
      std::string redirect_target =
          request.relative_url.substr(query_string_pos + 1);

      std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
          new net::test_server::BasicHttpResponse);
      http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
      http_response->AddCustomHeader("Location", redirect_target);
      return std::move(http_response);
    } else if (request.relative_url == "/") {
      std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
          new net::test_server::BasicHttpResponse);
      http_response->set_code(net::HTTP_OK);
      http_response->set_content("Success");
      return std::move(http_response);
    }
    return nullptr;
  }

  const GURL& dm_url() const { return dm_url_; }

  const net::test_server::HttpRequest::HeaderMap& last_request_headers() const {
    return last_request_headers_;
  }

 private:
  // The dummy URL for DMServer.
  GURL dm_url_;

  // List of request headers received by the embedded server.
  net::test_server::HttpRequest::HeaderMap last_request_headers_;

  DISALLOW_COPY_AND_ASSIGN(PolicyHeaderServiceBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PolicyHeaderServiceBrowserTest, NoPolicyHeader) {
  // When fetching non-DMServer URLs, we should not add a policy header to the
  // request.
  DCHECK(!embedded_test_server()->base_url().spec().empty());
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->base_url());
  auto iter = last_request_headers().find(policy::kChromePolicyHeader);
  EXPECT_EQ(iter, last_request_headers().end());
}

IN_PROC_BROWSER_TEST_F(PolicyHeaderServiceBrowserTest, PolicyHeader) {
  // When fetching a DMServer URL, we should add a policy header to the
  // request.
  ui_test_utils::NavigateToURL(browser(), dm_url());

  auto iter = last_request_headers().find(policy::kChromePolicyHeader);
  ASSERT_NE(iter, last_request_headers().end());
  EXPECT_EQ(iter->second, kTestPolicyHeader);
}

IN_PROC_BROWSER_TEST_F(PolicyHeaderServiceBrowserTest,
                       PolicyHeaderForRedirect) {
  // Build up a URL that results in a redirect to the DMServer URL to make
  // sure the policy header is still added.
  std::string redirect_url;
  redirect_url += kServerRedirectUrl;
  redirect_url += "?";
  redirect_url += dm_url().spec();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(redirect_url));

  auto iter = last_request_headers().find(policy::kChromePolicyHeader);
  ASSERT_NE(iter, last_request_headers().end());
  EXPECT_EQ(iter->second, kTestPolicyHeader);
}

// Helper class to test window creation from NTP.
class OpenWindowFromNTPBrowserTest : public InProcessBrowserTest,
                                     public InstantTestBase {
 public:
  OpenWindowFromNTPBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_test_server().InitializeAndListen());
    https_test_server().StartAcceptingConnections();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenWindowFromNTPBrowserTest);
};

// Test checks that navigations from NTP tab to URLs with same host as NTP but
// different path do not reuse NTP SiteInstance. See https://crbug.com/859062
// for details.
IN_PROC_BROWSER_TEST_F(OpenWindowFromNTPBrowserTest,
                       TransferFromNTPCreateNewTab) {
  GURL search_url =
      https_test_server().GetURL("ntp.com", "/instant_extended.html");
  GURL ntp_url =
      https_test_server().GetURL("ntp.com", "/instant_extended_ntp.html");
  InstantTestBase::Init(search_url, ntp_url, false);

  SetupInstant(browser());

  // Navigate to the NTP URL and verify that the resulting process is marked as
  // an Instant process.
  ui_test_utils::NavigateToURL(browser(), ntp_url);
  content::WebContents* ntp_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(instant_service->IsInstantProcess(
      ntp_tab->GetMainFrame()->GetProcess()->GetID()));

  // Execute script that creates new window from ntp tab with
  // ntp.com/title1.html as target url. Host is same as remote-ntp host, yet
  // path is different.
  GURL generic_url(https_test_server().GetURL("ntp.com", "/title1.html"));
  content::TestNavigationObserver opened_tab_observer(nullptr);
  opened_tab_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(
      ExecuteScript(ntp_tab, "window.open('" + generic_url.spec() + "');"));
  opened_tab_observer.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* opened_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait until newly opened tab is fully loaded.
  EXPECT_TRUE(WaitForLoadStop(opened_tab));

  EXPECT_NE(opened_tab, ntp_tab);
  EXPECT_EQ(generic_url, opened_tab->GetLastCommittedURL());
  // New created tab should not reside in an Instant process.
  EXPECT_FALSE(instant_service->IsInstantProcess(
      opened_tab->GetMainFrame()->GetProcess()->GetID()));
}

}  // namespace content
