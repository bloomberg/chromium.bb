// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

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
        switches::kHostResolverRules,
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

  // Ensure the correct histograms are incremented for blocking events.
  // Assumes the resource type is XHR.
  void InspectHistograms(const base::HistogramTester& histograms,
                         bool should_be_blocked,
                         bool should_be_sniffed,
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
      EXPECT_FALSE(should_be_blocked);
      bucket = "Other";
    }

    // Determine the appropriate histograms, including a start and end action
    // (which are verified in unit tests), a read size if it was sniffed, and
    // additional blocked metrics if it was blocked.
    base::HistogramTester::CountsMap expected_counts;
    std::string base = "SiteIsolation.XSD.Browser";
    expected_counts[base + ".Action"] = 2;
    if (should_be_sniffed)
      expected_counts[base + ".BytesReadForSniffing"] = 1;
    if (should_be_blocked) {
      expected_counts[base + ".Blocked"] = 1;
      expected_counts[base + ".Blocked." + bucket] = 1;
    }

    // Make sure that the expected metrics, and only those metrics, were
    // incremented.
    EXPECT_THAT(histograms.GetTotalCountsForPrefix("SiteIsolation.XSD.Browser"),
                testing::ContainerEq(expected_counts))
        << "For resource_name=" << resource_name
        << ", should_be_blocked=" << should_be_blocked;

    // Determine if the bucket for the resource type (XHR) was incremented.
    if (should_be_blocked) {
      EXPECT_THAT(histograms.GetAllSamples(base + ".Blocked"),
                  testing::ElementsAre(base::Bucket(resource_type, 1)))
          << "The wrong Blocked bucket was incremented.";
      EXPECT_THAT(histograms.GetAllSamples(base + ".Blocked." + bucket),
                  testing::ElementsAre(base::Bucket(resource_type, 1)))
          << "The wrong Blocked bucket was incremented.";
    }
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
  GURL foo_url("http://foo.com/cross_site_document_request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // The following are files under content/test/data/site_isolation. All
  // should be disallowed for cross site XHR under the document blocking policy.
  //   valid.*     - Correctly labeled HTML/XML/JSON files.
  //   *.txt       - Plain text that sniffs as HTML, XML, or JSON.
  //   htmlN_dtd.* - Various HTML templates to test.
  const char* blocked_resources[] = {
      "valid.html",    "valid.xml",      "valid.json",         "html.txt",
      "xml.txt",       "json.txt",       "comment_valid.html", "html4_dtd.html",
      "html4_dtd.txt", "html5_dtd.html", "html5_dtd.txt"};
  for (const char* resource : blocked_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    base::HistogramTester histograms;
    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), base::StringPrintf("sendRequest('%s');", resource),
        &was_blocked));
    EXPECT_TRUE(was_blocked);
    InspectHistograms(histograms, true /* should_be_blocked */,
                      true /* should_be_sniffed */, resource,
                      RESOURCE_TYPE_XHR);
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
    InspectHistograms(histograms, true /* should_be_blocked */,
                      false /* should_be_sniffed */, resource,
                      RESOURCE_TYPE_XHR);
  }

  // These files are allowed for XHR under the document blocking policy because
  // the sniffing logic determines they are not actually documents.
  //   *js.*   - JavaScript mislabeled as a document.
  //   jsonp.* - JSONP (i.e., script) mislabeled as a document.
  //   img.*   - Contents that won't match the document label.
  const char* sniff_allowed_resources[] = {
      "js.html",    "comment_js.html", "js.xml",     "js.json",   "js.txt",
      "jsonp.html", "jsonp.xml",       "jsonp.json", "jsonp.txt", "img.html",
      "img.xml",    "img.json",        "img.txt"};
  for (const char* resource : sniff_allowed_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    base::HistogramTester histograms;
    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), base::StringPrintf("sendRequest('%s');", resource),
        &was_blocked));
    EXPECT_FALSE(was_blocked);
    InspectHistograms(histograms, false /* should_be_blocked */,
                      true /* should_be_sniffed */, resource,
                      RESOURCE_TYPE_XHR);
  }

  // These files should be allowed for XHR under the document blocking policy.
  //   cors.*  - Correctly labeled documents with valid CORS headers.
  //   valid.* - Correctly labeled responses of non-document types.
  const char* allowed_resources[] = {"cors.html", "cors.xml", "cors.json",
                                     "cors.txt", "valid.js"};
  for (const char* resource : allowed_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    base::HistogramTester histograms;
    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), base::StringPrintf("sendRequest('%s');", resource),
        &was_blocked));
    EXPECT_FALSE(was_blocked);
    InspectHistograms(histograms, false /* should_be_blocked */,
                      false /* should_be_sniffed */, resource,
                      RESOURCE_TYPE_XHR);
  }
}

// Verify that range requests disable the sniffing logic, so that attackers
// can't cause sniffing to fail to force a response to be allowed.  This won't
// be a problem for script files mislabeled as HTML/XML/JSON/text (i.e., the
// reason for sniffing), since script tags won't send Range headers.
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTest, RangeRequest) {
  GURL foo_url("http://foo.com/cross_site_document_request.html");
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
    InspectHistograms(histograms, true /* should_be_blocked */,
                      false /* should_be_sniffed */, "valid.html",
                      RESOURCE_TYPE_XHR);
  }
  {
    // Verify that a response which would have been allowed by MIME type anyway
    // is still allowed for range requests.
    base::HistogramTester histograms;
    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), "sendRequest('valid.js', 'bytes=1-5');", &was_blocked));
    EXPECT_FALSE(was_blocked);
    InspectHistograms(histograms, false /* should_be_blocked */,
                      false /* should_be_sniffed */, "valid.js",
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
    InspectHistograms(histograms, false /* should_be_blocked */,
                      false /* should_be_sniffed */, "cors.json",
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
  GURL foo("http://foo.com/cross_site_document_request_target.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo));
  WaitForLoadStop(shell()->web_contents());

  // TODO(creis): Wait for all the subresources to load and ensure renderer
  // process is still alive.
}

// Without any Site Isolation (in the base test class), there should be no
// document blocking.
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingBaseTest,
                       DontBlockDocumentsByDefault) {
  if (AreAllSitesIsolatedForTesting())
    return;

  // Load a page that issues illegal cross-site document requests to bar.com.
  GURL foo_url("http://foo.com/cross_site_document_request.html");
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
  GURL foo_url("http://foo.com/cross_site_document_request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  bool was_blocked;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "sendRequest(\"valid.html\");", &was_blocked));
  EXPECT_TRUE(was_blocked);
}

}  // namespace content
