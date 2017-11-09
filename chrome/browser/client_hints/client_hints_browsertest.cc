// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"

class ClientHintsBrowserTest : public InProcessBrowserTest {
 public:
  ClientHintsBrowserTest()
      : http_server_(net::EmbeddedTestServer::TYPE_HTTP),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        expect_client_hints_(false),
        expect_client_hints_on_main_frame_only_(false),
        count_client_hints_headers_seen_(0) {
    http_server_.ServeFilesFromSourceDirectory("chrome/test/data/client_hints");
    https_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/client_hints");

    http_server_.RegisterRequestMonitor(
        base::Bind(&ClientHintsBrowserTest::MonitorResourceRequest,
                   base::Unretained(this)));
    https_server_.RegisterRequestMonitor(
        base::Bind(&ClientHintsBrowserTest::MonitorResourceRequest,
                   base::Unretained(this)));

    EXPECT_TRUE(http_server_.Start());
    EXPECT_TRUE(https_server_.Start());

    accept_ch_with_lifetime_http_local_url_ =
        http_server_.GetURL("/accept_ch_with_lifetime.html");
    EXPECT_TRUE(accept_ch_with_lifetime_http_local_url_.SchemeIsHTTPOrHTTPS());
    EXPECT_FALSE(
        accept_ch_with_lifetime_http_local_url_.SchemeIsCryptographic());

    accept_ch_with_lifetime_url_ =
        https_server_.GetURL("/accept_ch_with_lifetime.html");
    EXPECT_TRUE(accept_ch_with_lifetime_url_.SchemeIsHTTPOrHTTPS());
    EXPECT_TRUE(accept_ch_with_lifetime_url_.SchemeIsCryptographic());

    accept_ch_without_lifetime_url_ =
        https_server_.GetURL("/accept_ch_without_lifetime.html");
    EXPECT_TRUE(accept_ch_with_lifetime_url_.SchemeIsHTTPOrHTTPS());
    EXPECT_TRUE(accept_ch_with_lifetime_url_.SchemeIsCryptographic());

    without_accept_ch_without_lifetime_url_ =
        https_server_.GetURL("/without_accept_ch_without_lifetime.html");
    EXPECT_TRUE(without_accept_ch_without_lifetime_url_.SchemeIsHTTPOrHTTPS());
    EXPECT_TRUE(
        without_accept_ch_without_lifetime_url_.SchemeIsCryptographic());

    without_accept_ch_without_lifetime_local_url_ =
        http_server_.GetURL("/without_accept_ch_without_lifetime.html");
    EXPECT_TRUE(
        without_accept_ch_without_lifetime_local_url_.SchemeIsHTTPOrHTTPS());
    EXPECT_FALSE(
        without_accept_ch_without_lifetime_local_url_.SchemeIsCryptographic());
  }

  ~ClientHintsBrowserTest() override {}

  void SetUpOnMainThread() override {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetClientHintExpectations(bool expect_client_hints) {
    expect_client_hints_ = expect_client_hints;
  }

  void SetClientHintExpectationsOnMainFrameOnly(bool expect_client_hints) {
    expect_client_hints_on_main_frame_only_ = expect_client_hints;
  }

  const GURL& accept_ch_with_lifetime_http_local_url() const {
    return accept_ch_with_lifetime_http_local_url_;
  }

  // A URL whose response headers include Accept-CH and Accept-CH-Lifetime
  // headers.
  const GURL& accept_ch_with_lifetime_url() const {
    return accept_ch_with_lifetime_url_;
  }

  // A URL whose response headers include only Accept-CH header.
  const GURL& accept_ch_without_lifetime_url() const {
    return accept_ch_without_lifetime_url_;
  }

  // A URL whose response headers do not include either Accept-CH or
  // Accept-CH-Lifetime headers. Navigating to this URL also fetches an image.
  const GURL& without_accept_ch_without_lifetime_url() const {
    return without_accept_ch_without_lifetime_url_;
  }

  // A URL whose response headers do not include either Accept-CH or
  // Accept-CH-Lifetime headers. Navigating to this URL also fetches an image.
  const GURL& without_accept_ch_without_lifetime_local_url() const {
    return without_accept_ch_without_lifetime_local_url_;
  }

  size_t count_client_hints_headers_seen() const {
    return count_client_hints_headers_seen_;
  }

 private:
  // Called by |https_server_|.
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    bool is_main_frame_navigation =
        request.GetURL().spec().find(".html") != std::string::npos;

    // When browser side navigation is enabled, dpr headers is not attached to
    // the main frame request.
    if (!expect_client_hints_on_main_frame_only_) {
      EXPECT_EQ(
          expect_client_hints_ && (!content::IsBrowserSideNavigationEnabled() ||
                                   !is_main_frame_navigation),
          base::ContainsKey(request.headers, "dpr"));
    } else {
      EXPECT_EQ(expect_client_hints_on_main_frame_only_ &&
                    is_main_frame_navigation &&
                    (!content::IsBrowserSideNavigationEnabled() ||
                     !is_main_frame_navigation),
                base::ContainsKey(request.headers, "dpr"));
    }

    // When browser side navigation is enabled, device-memory header is attached
    // to the main frame request.
    if (!expect_client_hints_on_main_frame_only_) {
      EXPECT_EQ(expect_client_hints_,
                base::ContainsKey(request.headers, "device-memory"));
    } else {
      EXPECT_EQ(expect_client_hints_ && is_main_frame_navigation,
                base::ContainsKey(request.headers, "device-memory"));
    }

    if (base::ContainsKey(request.headers, "dpr"))
      count_client_hints_headers_seen_++;

    if (base::ContainsKey(request.headers, "device-memory"))
      count_client_hints_headers_seen_++;
  }

  net::EmbeddedTestServer http_server_;
  net::EmbeddedTestServer https_server_;
  GURL accept_ch_with_lifetime_http_local_url_;
  GURL accept_ch_with_lifetime_url_;
  GURL accept_ch_without_lifetime_url_;
  GURL without_accept_ch_without_lifetime_url_;
  GURL without_accept_ch_without_lifetime_local_url_;

  bool expect_client_hints_;
  // Expect client hints only on the main frame request, and not on
  // subresources.
  bool expect_client_hints_on_main_frame_only_;
  size_t count_client_hints_headers_seen_;

  DISALLOW_COPY_AND_ASSIGN(ClientHintsBrowserTest);
};

// Loads a webpage that requests persisting of client hints. Verifies that
// the browser receives the mojo notification from the renderer and persists the
// client hints to the disk.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, ClientHintsHttps) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), accept_ch_with_lifetime_url());

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // client_hints_url() sets two client hints.
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize", 2, 1);
  // accept_ch_with_lifetime_url() sets client hints persist duration to 3600
  // seconds.
  histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                      3600 * 1000, 1);
}

// Loads a HTTP local webpage (which qualifies as a secure context) that
// requests persisting of client hints. Verifies that the browser receives the
// mojo notification from the renderer and persists the client hints to the
// disk.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsLifetimeFollowedByNoClientHintHttpLocal) {
  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  ui_test_utils::NavigateToURL(browser(),
                               accept_ch_with_lifetime_http_local_url());

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // client_hints_url() sets two client hints.
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize", 2, 1);
  // accept_ch_with_lifetime_http_local_url() sets client hints persist duration
  // to 3600 seconds.
  histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                      3600 * 1000, 1);

  base::RunLoop().RunUntilIdle();

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  SetClientHintExpectations(true);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_local_url());

  if (content::IsBrowserSideNavigationEnabled()) {
    // When browser side navigation is enabled, two client hints are attached to
    // the image request, and the device-memory header is attached to the main
    // frame request.
    EXPECT_EQ(3u, count_client_hints_headers_seen());
  } else {
    // When browser side navigation is not enabled, two client hints are
    // attached to each of the HTML and the image requests.
    EXPECT_EQ(4u, count_client_hints_headers_seen());
  }
}

// Loads a webpage that does not request persisting of client hints.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, NoClientHintsHttps) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // no_client_hints_url() does not sets the client hints.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  histogram_tester.ExpectTotalCount("ClientHints.PersistDuration", 0);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsLifetimeFollowedByNoClientHint) {
  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching accept_ch_with_lifetime_url() should persist the request for
  // client hints.
  ui_test_utils::NavigateToURL(browser(), accept_ch_with_lifetime_url());

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // client_hints_url() sets two client hints.
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize", 2, 1);
  // accept_ch_with_lifetime_url() sets client hints persist duration to 3600
  // seconds.
  histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                      3600 * 1000, 1);
  base::RunLoop().RunUntilIdle();

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  SetClientHintExpectations(true);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());

  if (content::IsBrowserSideNavigationEnabled()) {
    // When browser side navigation is enabled, two client hints are attached to
    // the image request, and the device-memory header is attached to the main
    // frame request.
    EXPECT_EQ(3u, count_client_hints_headers_seen());
  } else {
    // When browser side navigation is not enabled, two client hints are
    // attached to each of the HTML and the image requests.
    EXPECT_EQ(4u, count_client_hints_headers_seen());
  }
}

// Ensure that when cookies are blocked, client hint preferences are not
// persisted.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsLifetimeNotPersistedCookiesBlocked) {
  scoped_refptr<content_settings::CookieSettings> cookie_settings_ =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  // Block cookies.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(accept_ch_without_lifetime_url(), GURL(),
                                      CONTENT_SETTINGS_TYPE_COOKIES,
                                      std::string(), CONTENT_SETTING_BLOCK);

  // Fetching accept_ch_with_lifetime_url() should not persist the request for
  // client hints since cookies are blocked.
  ui_test_utils::NavigateToURL(browser(), accept_ch_with_lifetime_url());
  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Allow cookies.
  cookie_settings_->SetCookieSetting(accept_ch_without_lifetime_url(),
                                     CONTENT_SETTING_ALLOW);
  // Fetching accept_ch_with_lifetime_url() should persist the request for
  // client hints since cookies are allowed.
  ui_test_utils::NavigateToURL(browser(), accept_ch_with_lifetime_url());
  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 1);
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsLifetimeNotAttachedCookiesBlocked) {
  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching accept_ch_with_lifetime_url() should persist the request for
  // client hints.
  ui_test_utils::NavigateToURL(browser(), accept_ch_with_lifetime_url());
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);
  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // client_hints_url() sets two client hints.
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize", 2, 1);
  // accept_ch_with_lifetime_url() tries to set client hints persist duration to
  // 3600 seconds.
  histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                      3600 * 1000, 1);
  base::RunLoop().RunUntilIdle();

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  // Block the cookies: Client hints should not be attached.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(accept_ch_without_lifetime_url(), GURL(),
                                      CONTENT_SETTINGS_TYPE_COOKIES,
                                      std::string(), CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());
  EXPECT_EQ(0u, count_client_hints_headers_seen());

  // Allow the cookies: Client hints should now be attached.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(accept_ch_without_lifetime_url(), GURL(),
                                      CONTENT_SETTINGS_TYPE_COOKIES,
                                      std::string(), CONTENT_SETTING_ALLOW);

  SetClientHintExpectations(true);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());
  if (content::IsBrowserSideNavigationEnabled()) {
    // When browser side navigation is enabled, two client hints are attached to
    // the image request, and the device-memory header is attached to the main
    // frame request.
    EXPECT_EQ(3u, count_client_hints_headers_seen());
  } else {
    // When browser side navigation is not enabled, two client hints are
    // attached to each of the HTML and the image requests.
    EXPECT_EQ(4u, count_client_hints_headers_seen());
  }

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(CONTENT_SETTINGS_TYPE_COOKIES);
}

// Ensure that when the JavaScript is blocked, client hint preferences are not
// persisted.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsLifetimeNotPersistedJavaScriptBlocked) {
  ContentSettingsForOneType host_settings;

  // Start a navigation. This navigation makes it possible to block JavaScript
  // later.
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());

  // Block the JavaScript: Client hint preferences should not be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(accept_ch_with_lifetime_url(), GURL(),
                                      CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                      std::string(), CONTENT_SETTING_BLOCK);
  ui_test_utils::NavigateToURL(browser(), accept_ch_with_lifetime_url());
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Allow the JavaScript: Client hint preferences should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(accept_ch_with_lifetime_url(), GURL(),
                                      CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                      std::string(), CONTENT_SETTING_ALLOW);
  ui_test_utils::NavigateToURL(browser(), accept_ch_with_lifetime_url());
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());
}

// Ensure that when the JavaScript is blocked, persisted client hints are not
// attached to the request headers.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsLifetimeNotAttachedJavaScriptBlocked) {
  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching accept_ch_with_lifetime_url() should persist the request for
  // client hints.
  ui_test_utils::NavigateToURL(browser(), accept_ch_with_lifetime_url());
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);
  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // client_hints_url() sets two client hints.
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize", 2, 1);
  // accept_ch_with_lifetime_url() tries to set client hints persist duration to
  // 3600 seconds.
  histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                      3600 * 1000, 1);
  base::RunLoop().RunUntilIdle();

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  // Block the Javascript: Client hints should not be attached.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(without_accept_ch_without_lifetime_url(),
                                      GURL(), CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                      std::string(), CONTENT_SETTING_BLOCK);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());
  EXPECT_EQ(0u, count_client_hints_headers_seen());

  // Allow the Javascript: Client hints should now be attached.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(without_accept_ch_without_lifetime_url(),
                                      GURL(), CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                      std::string(), CONTENT_SETTING_ALLOW);

  SetClientHintExpectations(true);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());
  if (content::IsBrowserSideNavigationEnabled()) {
    // When browser side navigation is enabled, two client hints are attached to
    // the image request, and the device-memory header is attached to the main
    // frame request.
    EXPECT_EQ(3u, count_client_hints_headers_seen());
  } else {
    // When browser side navigation is not enabled, two client hints are
    // attached to each of the HTML and the image requests.
    EXPECT_EQ(4u, count_client_hints_headers_seen());
  }

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(CONTENT_SETTINGS_TYPE_JAVASCRIPT);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsNoLifetimeFollowedByNoClientHint) {
  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching accept_ch_without_lifetime_url() should not persist the request
  // for client hints.
  ui_test_utils::NavigateToURL(browser(), accept_ch_without_lifetime_url());

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  histogram_tester.ExpectTotalCount("ClientHints.PersistDuration", 0);
  base::RunLoop().RunUntilIdle();

  // Clients hints preferences should not be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Next request should not have client hint headers attached.
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());
}

// Check the client hints for the given URL in an incognito window.
// Start incognito browser twice to ensure that client hints prefs are
// not carried over.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, ClientHintsHttpsIncognito) {
  for (size_t i = 0; i < 2; ++i) {
    base::HistogramTester histogram_tester;

    Browser* incognito = CreateIncognitoBrowser();
    ui_test_utils::NavigateToURL(incognito, accept_ch_with_lifetime_url());

    histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    // accept_ch_with_lifetime_url() sets two client hints.
    histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize", 2, 1);

    // At least one renderer must have been created. All the renderers created
    // must have read 0 client hints.
    EXPECT_LE(1u,
              histogram_tester.GetAllSamples("ClientHints.CountRulesReceived")
                  .size());
    for (const auto& bucket :
         histogram_tester.GetAllSamples("ClientHints.CountRulesReceived")) {
      EXPECT_EQ(0, bucket.min);
    }
    // |url| sets client hints persist duration to 3600 seconds.
    histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                        3600 * 1000, 1);

    CloseBrowserSynchronously(incognito);
  }
}
