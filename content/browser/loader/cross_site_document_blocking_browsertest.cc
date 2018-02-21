// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

enum HistogramExpectations {
  kShouldBeBlocked = 1 << 0,
  kShouldBeSniffed = 1 << 1,
  kShouldHaveContentLength = 1 << 2,

  kShouldBeAllowedWithoutSniffing = 0,
  kShouldBeBlockedWithoutSniffing = kShouldBeBlocked,
  kShouldBeSniffedAndAllowed = kShouldBeSniffed,
  kShouldBeSniffedAndBlocked = kShouldBeSniffed | kShouldBeBlocked,
};

HistogramExpectations operator|(HistogramExpectations a,
                                HistogramExpectations b) {
  return static_cast<HistogramExpectations>(static_cast<int>(a) |
                                            static_cast<int>(b));
}

std::ostream& operator<<(std::ostream& os, const HistogramExpectations& value) {
  if (value == 0) {
    os << "(none)";
    return os;
  }

  os << "( ";
  if (0 != (value & kShouldBeBlocked))
    os << "kShouldBeBlocked ";
  if (0 != (value & kShouldBeSniffed))
    os << "kShouldBeSniffed ";
  if (0 != (value & kShouldHaveContentLength))
    os << "kShouldHaveContentLength ";
  os << ")";
  return os;
}

// Ensure the correct histograms are incremented for blocking events.
// Assumes the resource type is XHR.
void InspectHistograms(const base::HistogramTester& histograms,
                       const HistogramExpectations& expectations,
                       const std::string& resource_name,
                       ResourceType resource_type) {
  std::string bucket;
  if (base::MatchPattern(resource_name, "*.html")) {
    bucket = "HTML";
  } else if (base::MatchPattern(resource_name, "*.xml")) {
    bucket = "XML";
  } else if (base::MatchPattern(resource_name, "*.json")) {
    bucket = "JSON";
  } else if (base::MatchPattern(resource_name, "*.txt")) {
    bucket = "Plain";
  } else {
    bucket = "Others";
  }

  // Determine the appropriate histograms, including a start and end action
  // (which are verified in unit tests), a read size if it was sniffed, and
  // additional blocked metrics if it was blocked.
  base::HistogramTester::CountsMap expected_counts;
  std::string base = "SiteIsolation.XSD.Browser";
  expected_counts[base + ".Action"] = 2;
  if ((base::MatchPattern(resource_name, "*prefixed*") || bucket == "Others") &&
      (0 != (expectations & kShouldBeBlocked))) {
    expected_counts[base + ".BlockedForParserBreaker"] = 1;
  }
  if (0 != (expectations & kShouldBeSniffed))
    expected_counts[base + ".BytesReadForSniffing"] = 1;
  if (0 != (expectations & kShouldBeBlocked)) {
    expected_counts[base + ".Blocked"] = 1;
    expected_counts[base + ".Blocked." + bucket] = 1;
    expected_counts[base + ".Blocked.ContentLength.WasAvailable"] = 1;
    if (0 != (expectations & kShouldHaveContentLength))
      expected_counts[base + ".Blocked.ContentLength.ValueIfAvailable"] = 1;
  }

  // Make sure that the expected metrics, and only those metrics, were
  // incremented.
  EXPECT_THAT(histograms.GetTotalCountsForPrefix("SiteIsolation.XSD.Browser"),
              testing::ContainerEq(expected_counts))
      << "For resource_name=" << resource_name
      << ", expectations=" << expectations;

  // Determine if the bucket for the resource type (XHR) was incremented.
  if (0 != (expectations & kShouldBeBlocked)) {
    EXPECT_THAT(histograms.GetAllSamples(base + ".Blocked"),
                testing::ElementsAre(base::Bucket(resource_type, 1)))
        << "The wrong Blocked bucket was incremented.";
    EXPECT_THAT(histograms.GetAllSamples(base + ".Blocked." + bucket),
                testing::ElementsAre(base::Bucket(resource_type, 1)))
        << "The wrong Blocked bucket was incremented.";
  }
}

}  // namespace

// These tests verify that the browser process blocks cross-site HTML, XML,
// JSON, and some plain text responses when they are not otherwise permitted
// (e.g., by CORS).  This ensures that such responses never end up in the
// renderer process where they might be accessible via a bug.  Careful attention
// is paid to allow other cross-site resources necessary for rendering,
// including cases that may be mislabeled as blocked MIME type.
//
// Many of these tests work by turning off the Same Origin Policy in the
// renderer process via --disable-web-security, and then trying to access the
// resource via a cross-origin XHR.  If the response is blocked, the XHR should
// see an empty response body.
//
// Note that this BaseTest class does not specify an isolation mode via
// command-line flags.  Most of the tests are in the --site-per-process subclass
// below.
class CrossSiteDocumentBlockingBaseTest : public ContentBrowserTest {
 public:
  CrossSiteDocumentBlockingBaseTest() {}
  ~CrossSiteDocumentBlockingBaseTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // EmbeddedTestServer::InitializeAndListen() initializes its |base_url_|
    // which is required below. This cannot invoke Start() however as that kicks
    // off the "EmbeddedTestServer IO Thread" which then races with
    // initialization in ContentBrowserTest::SetUp(), http://crbug.com/674545.
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    // Add a host resolver rule to map all outgoing requests to the test server.
    // This allows us to use "real" hostnames and standard ports in URLs (i.e.,
    // without having to inject the port number into all URLs), which we can use
    // to create arbitrary SiteInstances.
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        "MAP * " + embedded_test_server()->host_port_pair().ToString() +
            ",EXCLUDE localhost");

    // To test that the renderer process does not receive blocked documents, we
    // disable the same origin policy to let it see cross-origin fetches if they
    // are received.
    command_line->AppendSwitch(switches::kDisableWebSecurity);
  }

  void SetUpOnMainThread() override {
    // Complete the manual Start() after ContentBrowserTest's own
    // initialization, ref. comment on InitializeAndListen() above.
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentBlockingBaseTest);
};

// Most tests here use --site-per-process, which enables document blocking
// everywhere.
class CrossSiteDocumentBlockingTest : public CrossSiteDocumentBlockingBaseTest {
 public:
  CrossSiteDocumentBlockingTest() {}
  ~CrossSiteDocumentBlockingTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
    CrossSiteDocumentBlockingBaseTest::SetUpCommandLine(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentBlockingTest);
};

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTest, BlockDocuments) {
  // Load a page that issues illegal cross-site document requests to bar.com.
  // The page uses XHR to request HTML/XML/JSON documents from bar.com, and
  // inspects if any of them were successfully received. This test is only
  // possible since we run the browser without the same origin policy, allowing
  // it to see the response body if it makes it to the renderer (even if the
  // renderer would normally block access to it).
  GURL foo_url("http://foo.com/cross_site_document_blocking/request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // The following are files under content/test/data/site_isolation. All
  // should be disallowed for cross site XHR under the document blocking policy.
  //   valid.*        - Correctly labeled HTML/XML/JSON files.
  //   *.txt          - Plain text that sniffs as HTML, XML, or JSON.
  //   htmlN_dtd.*    - Various HTML templates to test.
  //   json-prefixed* - parser-breaking prefixes
  const char* blocked_resources[] = {"valid.html",
                                     "valid.xml",
                                     "valid.json",
                                     "html.txt",
                                     "xml.txt",
                                     "json.txt",
                                     "comment_valid.html",
                                     "html4_dtd.html",
                                     "html4_dtd.txt",
                                     "html5_dtd.html",
                                     "html5_dtd.txt",
                                     "json.js",
                                     "json-prefixed-1.js",
                                     "json-prefixed-2.js",
                                     "json-prefixed-3.js",
                                     "json-prefixed-4.js",
                                     "nosniff.json.js",
                                     "nosniff.json-prefixed.js"};
  for (const char* resource : blocked_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    base::HistogramTester histograms;
    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), base::StringPrintf("sendRequest('%s');", resource),
        &was_blocked));
    EXPECT_TRUE(was_blocked);
    InspectHistograms(histograms,
                      kShouldBeSniffedAndBlocked | kShouldHaveContentLength,
                      resource, RESOURCE_TYPE_XHR);
  }

  // These files should be disallowed without sniffing.
  //   nosniff.*   - Won't sniff correctly, but blocked because of nosniff.
  const char* nosniff_blocked_resources[] = {"nosniff.html", "nosniff.xml",
                                             "nosniff.json", "nosniff.txt"};
  for (const char* resource : nosniff_blocked_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    base::HistogramTester histograms;
    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), base::StringPrintf("sendRequest('%s');", resource),
        &was_blocked));
    EXPECT_TRUE(was_blocked);
    InspectHistograms(histograms, kShouldBeBlockedWithoutSniffing, resource,
                      RESOURCE_TYPE_XHR);
  }

  // These files are allowed for XHR under the document blocking policy because
  // the sniffing logic determines they are not actually documents.
  //   *js.*   - JavaScript mislabeled as a document.
  //   jsonp.* - JSONP (i.e., script) mislabeled as a document.
  //   img.*   - Contents that won't match the document label.
  //   valid.* - Correctly labeled responses of non-document types.
  const char* sniff_allowed_resources[] = {
      "js.html",   "comment_js.html", "js.xml",       "js.json",
      "js.txt",    "jsonp.html",      "jsonp.xml",    "jsonp.json",
      "jsonp.txt", "img.html",        "img.xml",      "img.json",
      "img.txt",   "valid.js",        "json-list.js", "nosniff.json-list.js"};
  for (const char* resource : sniff_allowed_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    base::HistogramTester histograms;
    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), base::StringPrintf("sendRequest('%s');", resource),
        &was_blocked));
    EXPECT_FALSE(was_blocked);
    InspectHistograms(histograms, kShouldBeSniffedAndAllowed, resource,
                      RESOURCE_TYPE_XHR);
  }

  // These files should be allowed for XHR under the document blocking policy.
  //   cors.*  - Correctly labeled documents with valid CORS headers.
  const char* allowed_resources[] = {"cors.html", "cors.xml", "cors.json",
                                     "cors.txt"};
  for (const char* resource : allowed_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    base::HistogramTester histograms;
    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), base::StringPrintf("sendRequest('%s');", resource),
        &was_blocked));
    EXPECT_FALSE(was_blocked);
    InspectHistograms(histograms, kShouldBeAllowedWithoutSniffing, resource,
                      RESOURCE_TYPE_XHR);
  }
}

// Verify that range requests disable the sniffing logic, so that attackers
// can't cause sniffing to fail to force a response to be allowed.  This won't
// be a problem for script files mislabeled as HTML/XML/JSON/text (i.e., the
// reason for sniffing), since script tags won't send Range headers.
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTest, RangeRequest) {
  GURL foo_url("http://foo.com/cross_site_document_blocking/request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  {
    // Try to skip the first byte using a range request in an attempt to get the
    // response to fail sniffing and be allowed through.  It should still be
    // blocked because sniffing is disabled.
    base::HistogramTester histograms;
    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), "sendRequest('valid.html', 'bytes=1-24');", &was_blocked));
    EXPECT_TRUE(was_blocked);
    InspectHistograms(
        histograms, kShouldBeBlockedWithoutSniffing | kShouldHaveContentLength,
        "valid.html", RESOURCE_TYPE_XHR);
  }
  {
    // Verify that a response which would have been allowed by MIME type anyway
    // is still allowed for range requests.
    base::HistogramTester histograms;
    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), "sendRequest('valid.js', 'bytes=1-5');", &was_blocked));
    EXPECT_FALSE(was_blocked);
    InspectHistograms(histograms, kShouldBeAllowedWithoutSniffing, "valid.js",
                      RESOURCE_TYPE_XHR);
  }
  {
    // Verify that a response which would have been allowed by CORS anyway is
    // still allowed for range requests.
    base::HistogramTester histograms;
    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), "sendRequest('cors.json', 'bytes=2-7');", &was_blocked));
    EXPECT_FALSE(was_blocked);
    InspectHistograms(histograms, kShouldBeAllowedWithoutSniffing, "cors.json",
                      RESOURCE_TYPE_XHR);
  }
}

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTest, BlockForVariousTargets) {
  // This webpage loads a cross-site HTML page in different targets such as
  // <img>,<link>,<embed>, etc. Since the requested document is blocked, and one
  // character string (' ') is returned instead, this tests that the renderer
  // does not crash even when it receives a response body which is " ", whose
  // length is different from what's described in "content-length" for such
  // different targets.

  // TODO(nick): Split up these cases, and add positive assertions here about
  // what actually happens in these various resource-block cases.
  GURL foo("http://foo.com/cross_site_document_blocking/request_target.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo));

  // TODO(creis): Wait for all the subresources to load and ensure renderer
  // process is still alive.
}

// This test class sets up a service worker that can be used to try to respond
// to same-origin requests with cross-origin responses.
class CrossSiteDocumentBlockingServiceWorkerTest : public ContentBrowserTest {
 public:
  CrossSiteDocumentBlockingServiceWorkerTest()
      : service_worker_https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        cross_origin_https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~CrossSiteDocumentBlockingServiceWorkerTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);

    // To test that the renderer process does not receive blocked documents, we
    // disable the same origin policy to let it see cross-origin fetches if they
    // are received.
    command_line->AppendSwitch(switches::kDisableWebSecurity);

    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    SetupCrossSiteRedirector(embedded_test_server());

    service_worker_https_server_.ServeFilesFromSourceDirectory(
        "content/test/data");
    ASSERT_TRUE(service_worker_https_server_.Start());

    cross_origin_https_server_.ServeFilesFromSourceDirectory(
        "content/test/data");
    cross_origin_https_server_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
    ASSERT_TRUE(cross_origin_https_server_.Start());

    // Sanity check of test setup - the 2 https servers should be cross-site
    // (the second server should have a different hostname because of the call
    // to SetSSLConfig with CERT_COMMON_NAME_IS_DOMAIN argument).
    ASSERT_FALSE(SiteInstance::IsSameWebSite(
        shell()->web_contents()->GetBrowserContext(),
        GetURLOnServiceWorkerServer("/"), GetURLOnCrossOriginServer("/")));
  }

  GURL GetURLOnServiceWorkerServer(const std::string& path) {
    return service_worker_https_server_.GetURL(path);
  }

  GURL GetURLOnCrossOriginServer(const std::string& path) {
    return cross_origin_https_server_.GetURL(path);
  }

  void StopCrossOriginServer() {
    EXPECT_TRUE(cross_origin_https_server_.ShutdownAndWaitUntilComplete());
  }

  void SetUpServiceWorker() {
    GURL url = GetURLOnServiceWorkerServer(
        "/cross_site_document_blocking/request.html");
    ASSERT_TRUE(NavigateToURL(shell(), url));

    // Register the service worker.
    bool is_script_done;
    std::string script = R"(
        navigator.serviceWorker
            .register('/cross_site_document_blocking/service_worker.js')
            .then(registration => navigator.serviceWorker.ready)
            .then(function(r) { domAutomationController.send(true); })
            .catch(function(e) {
                console.log('error: ' + e);
                domAutomationController.send(false);
            }); )";
    ASSERT_TRUE(ExecuteScriptAndExtractBool(shell(), script, &is_script_done));
    ASSERT_TRUE(is_script_done);

    // Navigate again to the same URL - the service worker should be 1) active
    // at this time (because of waiting for |navigator.serviceWorker.ready|
    // above) and 2) controlling the current page (because of the reload).
    ASSERT_TRUE(NavigateToURL(shell(), url));
    bool is_controlled_by_service_worker;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(),
        "domAutomationController.send(!!navigator.serviceWorker.controller)",
        &is_controlled_by_service_worker));
    ASSERT_TRUE(is_controlled_by_service_worker);
  }

 private:
  // The test requires 2 https servers, because:
  // 1. Service workers are only supported on secure origins.
  // 2. One of tests requires fetching cross-origin resources from the
  //    original page and/or service worker - the target of the fetch needs to
  //    be a https server to avoid hitting the mixed content error.
  net::EmbeddedTestServer service_worker_https_server_;
  net::EmbeddedTestServer cross_origin_https_server_;

  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentBlockingServiceWorkerTest);
};

// Issue a cross-origin request that will be handled entirely within a service
// worker (without reaching the network - the cross-origin response will be
// "faked" within the same-origin service worker, because the service worker
// used by the test recognizes the "data_from_service_worker" suffix in the
// URL).  This testcase is designed to hit the case in
// CrossSiteDocumentResourceHandler::ShouldBlockBasedOnHeaders where
// |response_type_via_service_worker| is equal to |kDefault|.  See also
// https://crbug.com/803672.
//
// TODO(lukasza): https://crbug.com/715640: This test might become invalid
// after servicification of service workers.
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingServiceWorkerTest, NoNetwork) {
  SetUpServiceWorker();

  base::HistogramTester histograms;
  std::string response;
  std::string script = R"(
      // Any cross-origin URL ending with .../data_from_service_worker can be
      // used here - it will be intercepted by the service worker and will never
      // go to the network.
      fetch('https://bar.com/data_from_service_worker')
          .then(response => response.text())
          .then(responseText => {
              domAutomationController.send(responseText);
          })
          .catch(error => {
              var errorMessage = 'error: ' + error;
              console.log(errorMessage);
              domAutomationController.send(errorMessage);
          }); )";
  EXPECT_TRUE(ExecuteScriptAndExtractString(shell(), script, &response));

  // Verify that XSDB didn't block the response (since it was "faked" within the
  // service worker and didn't cross any security boundaries).
  EXPECT_EQ("Response created by service worker", response);
  InspectHistograms(histograms, kShouldBeAllowedWithoutSniffing, "blah.html",
                    RESOURCE_TYPE_XHR);
}

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingServiceWorkerTest,
                       NetworkAndOpaqueResponse) {
  SetUpServiceWorker();

  // Build a script for XHR-ing a cross-origin, nosniff HTML document.
  GURL cross_origin_url =
      GetURLOnCrossOriginServer("/site_isolation/nosniff.html");
  const char* script_template = R"(
      fetch('%s', { mode: 'no-cors' })
          .then(response => response.text())
          .then(responseText => {
              domAutomationController.send(responseText);
          })
          .catch(error => {
              var errorMessage = 'error: ' + error;
              domAutomationController.send(errorMessage);
          }); )";
  std::string script =
      base::StringPrintf(script_template, cross_origin_url.spec().c_str());

  {
    // The first time the request reaches the service worker, it will be
    // forwarded to the network, but a response will be intercepted by the
    // service worker and replaced with a new, artificial error.
    base::HistogramTester histograms;
    std::string response;
    EXPECT_TRUE(ExecuteScriptAndExtractString(shell(), script, &response));

    // Verify that XSDB blocked the response from the network (from
    // |cross_origin_https_server_|) to the service worker.
    InspectHistograms(histograms, kShouldBeBlockedWithoutSniffing,
                      "nosniff.html", RESOURCE_TYPE_XHR);

    // Verify that the service worker replied with an expected error.
    // Replying with an error means that XSDB is only active once (for the
    // initial, real network request) and therefore the test doesn't get
    // confused (second successful response would have added noise to the
    // histograms captured by the test).
    EXPECT_EQ("error: TypeError: Failed to fetch", response);
  }

  // TODO(lukasza): https://crbug.com/715640: The remainder of this test might
  // become invalid after servicification of service workers.
  {
    // Stop the server, to make sure the response below comes back from the
    // service worker (and not from the network).
    StopCrossOriginServer();

    // The second time the request reaches the service worker, it will return
    // the previously cached response from the network.  This should hit the
    // case in CrossSiteDocumentResourceHandler::ShouldBlockBasedOnHeaders where
    // |response_type_via_service_worker| is equal to |kOpaque|.
    base::HistogramTester histograms;
    std::string response;
    EXPECT_TRUE(ExecuteScriptAndExtractString(shell(), script, &response));

    // Verify that XSDB blocked the cached/opaque service worker response from
    // reaching a cross-origin page.
    EXPECT_EQ("", response);
    InspectHistograms(histograms, kShouldBeBlockedWithoutSniffing,
                      "nosniff.html", RESOURCE_TYPE_XHR);
  }
}

class CrossSiteDocumentBlockingKillSwitchTest
    : public CrossSiteDocumentBlockingTest {
 public:
  CrossSiteDocumentBlockingKillSwitchTest() {
    // Simulate flipping the kill switch.
    scoped_feature_list_.InitAndDisableFeature(
        features::kCrossSiteDocumentBlockingIfIsolating);
  }

  ~CrossSiteDocumentBlockingKillSwitchTest() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentBlockingKillSwitchTest);
};

// After the kill switch is flipped, there should be no document blocking.
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingKillSwitchTest,
                       NoBlockingWithKillSwitch) {
  // Load a page that issues illegal cross-site document requests to bar.com.
  GURL foo_url("http://foo.com/cross_site_document_blocking/request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  bool was_blocked;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "sendRequest(\"valid.html\");", &was_blocked));
  EXPECT_FALSE(was_blocked);
}

// Without any Site Isolation (in the base test class), there should be no
// document blocking.
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingBaseTest,
                       DontBlockDocumentsByDefault) {
  if (AreAllSitesIsolatedForTesting())
    return;

  // Load a page that issues illegal cross-site document requests to bar.com.
  GURL foo_url("http://foo.com/cross_site_document_blocking/request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  bool was_blocked;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "sendRequest(\"valid.html\");", &was_blocked));
  EXPECT_FALSE(was_blocked);
}

// Test class to verify that documents are blocked for isolated origins as well.
class CrossSiteDocumentBlockingIsolatedOriginTest
    : public CrossSiteDocumentBlockingBaseTest {
 public:
  CrossSiteDocumentBlockingIsolatedOriginTest() {}
  ~CrossSiteDocumentBlockingIsolatedOriginTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kIsolateOrigins,
                                    "http://bar.com");
    CrossSiteDocumentBlockingBaseTest::SetUpCommandLine(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentBlockingIsolatedOriginTest);
};

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingIsolatedOriginTest,
                       BlockDocumentsFromIsolatedOrigin) {
  if (AreAllSitesIsolatedForTesting())
    return;

  // Load a page that issues illegal cross-site document requests to the
  // isolated origin.
  GURL foo_url("http://foo.com/cross_site_document_blocking/request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  bool was_blocked;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "sendRequest(\"valid.html\");", &was_blocked));
  EXPECT_TRUE(was_blocked);
}

}  // namespace content
