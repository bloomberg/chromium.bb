// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

namespace content {

// These tests simulate exploited renderer processes, which can fetch arbitrary
// resources from other websites, not constrained by the Same Origin Policy.  We
// are trying to verify that the renderer cannot fetch any cross-site document
// responses even when the Same Origin Policy is turned off inside the renderer.
class SiteIsolationStatsGathererBrowserTest : public ContentBrowserTest {
 public:
  SiteIsolationStatsGathererBrowserTest() {}
  ~SiteIsolationStatsGathererBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(test_server()->Start());
    // Add a host resolver rule to map all outgoing requests to the test server.
    // This allows us to use "real" hostnames in URLs, which we can use to
    // create arbitrary SiteInstances.
    command_line->AppendSwitchASCII(
        switches::kHostResolverRules,
        "MAP * " + test_server()->host_port_pair().ToString() +
            ",EXCLUDE localhost");

    // Since we assume exploited renderer process, it can bypass the same origin
    // policy at will. Simulate that by passing the disable-web-security flag.
    command_line->AppendSwitch(switches::kDisableWebSecurity);
  }

  void InspectHistograms(const base::HistogramTester& histograms,
                         bool should_be_blocked,
                         const std::string& resource_name) {
    std::string bucket;
    int mime_type = 0;  // Hardcoded because histogram enums mustn't change.
    if (MatchPattern(resource_name, "*.html")) {
      bucket = "HTML";
      mime_type = 0;
    } else if (MatchPattern(resource_name, "*.xml")) {
      bucket = "XML";
      mime_type = 1;
    } else if (MatchPattern(resource_name, "*.json")) {
      bucket = "JSON";
      mime_type = 2;
    } else if (MatchPattern(resource_name, "*.txt")) {
      bucket = "Plain";
      mime_type = 3;
      if (MatchPattern(resource_name, "json.*")) {
        bucket += ".JSON";
      } else if (MatchPattern(resource_name, "html.*")) {
        bucket += ".HTML";
      } else if (MatchPattern(resource_name, "xml.*")) {
        bucket += ".XML";
      }
    } else {
      FAIL();
    }
    FetchHistogramsFromChildProcesses();

    // A few histograms are incremented unconditionally.
    histograms.ExpectUniqueSample("SiteIsolation.AllResponses", 1, 1);
    histograms.ExpectTotalCount("SiteIsolation.XSD.DataLength", 1);
    histograms.ExpectUniqueSample("SiteIsolation.XSD.MimeType", mime_type, 1);

    // Inspect the appropriate conditionally-incremented histogram[s].
    std::set<std::string> expected_metrics;
    std::string base_metric = "SiteIsolation.XSD." + bucket;
    base_metric += should_be_blocked ? ".Blocked" : ".NotBlocked";
    expected_metrics.insert(base_metric);
    if (should_be_blocked) {
      expected_metrics.insert(base_metric + ".RenderableStatusCode");
    } else if (MatchPattern(resource_name, "*js.*")) {
      expected_metrics.insert(base_metric + ".MaybeJS");
    }

    for (std::string metric : expected_metrics) {
      if (MatchPattern(metric, "*.RenderableStatusCode")) {
        histograms.ExpectUniqueSample(metric, RESOURCE_TYPE_XHR, 1);
      } else {
        histograms.ExpectUniqueSample(metric, 1, 1);
      }
    }

    // Make sure no other conditionally-incremented histograms were touched.
    const char* all_metrics[] = {
        "SiteIsolation.XSD.HTML.Blocked",
        "SiteIsolation.XSD.HTML.Blocked.NonRenderableStatusCode",
        "SiteIsolation.XSD.HTML.Blocked.RenderableStatusCode",
        "SiteIsolation.XSD.HTML.NoSniffBlocked",
        "SiteIsolation.XSD.HTML.NoSniffBlocked.NonRenderableStatusCode",
        "SiteIsolation.XSD.HTML.NoSniffBlocked.RenderableStatusCode",
        "SiteIsolation.XSD.HTML.NotBlocked",
        "SiteIsolation.XSD.HTML.NotBlocked.MaybeJS",
        "SiteIsolation.XSD.JSON.Blocked",
        "SiteIsolation.XSD.JSON.Blocked.NonRenderableStatusCode",
        "SiteIsolation.XSD.JSON.Blocked.RenderableStatusCode",
        "SiteIsolation.XSD.JSON.NoSniffBlocked",
        "SiteIsolation.XSD.JSON.NoSniffBlocked.NonRenderableStatusCode",
        "SiteIsolation.XSD.JSON.NoSniffBlocked.RenderableStatusCode",
        "SiteIsolation.XSD.JSON.NotBlocked",
        "SiteIsolation.XSD.JSON.NotBlocked.MaybeJS",
        "SiteIsolation.XSD.Plain.HTML.Blocked",
        "SiteIsolation.XSD.Plain.HTML.Blocked.NonRenderableStatusCode",
        "SiteIsolation.XSD.Plain.HTML.Blocked.RenderableStatusCode",
        "SiteIsolation.XSD.Plain.JSON.Blocked",
        "SiteIsolation.XSD.Plain.JSON.Blocked.NonRenderableStatusCode",
        "SiteIsolation.XSD.Plain.JSON.Blocked.RenderableStatusCode",
        "SiteIsolation.XSD.Plain.NoSniffBlocked",
        "SiteIsolation.XSD.Plain.NoSniffBlocked.NonRenderableStatusCode",
        "SiteIsolation.XSD.Plain.NoSniffBlocked.RenderableStatusCode",
        "SiteIsolation.XSD.Plain.NotBlocked",
        "SiteIsolation.XSD.Plain.NotBlocked.MaybeJS",
        "SiteIsolation.XSD.Plain.XML.Blocked",
        "SiteIsolation.XSD.Plain.XML.Blocked.NonRenderableStatusCode",
        "SiteIsolation.XSD.Plain.XML.Blocked.RenderableStatusCode",
        "SiteIsolation.XSD.XML.Blocked",
        "SiteIsolation.XSD.XML.Blocked.NonRenderableStatusCode",
        "SiteIsolation.XSD.XML.Blocked.RenderableStatusCode",
        "SiteIsolation.XSD.XML.NoSniffBlocked",
        "SiteIsolation.XSD.XML.NoSniffBlocked.NonRenderableStatusCode",
        "SiteIsolation.XSD.XML.NoSniffBlocked.RenderableStatusCode",
        "SiteIsolation.XSD.XML.NotBlocked",
        "SiteIsolation.XSD.XML.NotBlocked.MaybeJS"};

    for (const char* metric : all_metrics) {
      if (!expected_metrics.count(metric)) {
        histograms.ExpectTotalCount(metric, 0);
      }
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SiteIsolationStatsGathererBrowserTest);
};

// TODO(dsjang): we cannot run these tests on Android since SetUpCommandLine()
// is executed before the I/O thread is created on Android. After this bug
// (crbug.com/278425) is resolved, we can enable this test case on Android.
#if defined(OS_ANDROID)
#define MAYBE_CrossSiteDocumentBlockingForMimeType \
  DISABLED_CrossSiteDocumentBlockingForMimeType
#else
#define MAYBE_CrossSiteDocumentBlockingForMimeType \
  CrossSiteDocumentBlockingForMimeType
#endif

IN_PROC_BROWSER_TEST_F(SiteIsolationStatsGathererBrowserTest,
                       MAYBE_CrossSiteDocumentBlockingForMimeType) {
  // Load a page that issues illegal cross-site document requests to bar.com.
  // The page uses XHR to request HTML/XML/JSON documents from bar.com, and
  // inspects if any of them were successfully received. Currently, on illegal
  // access, the XHR requests should succeed, but the UMA histograms should
  // record that they would have been blocked. This test is only possible since
  // we run the browser without the same origin policy.
  GURL foo("http://foo.com/files/cross_site_document_request.html");

  NavigateToURL(shell(), foo);

  // Flush out existing histogram activity.
  FetchHistogramsFromChildProcesses();

  // The following are files under content/test/data/site_isolation. All
  // should be disallowed for XHR under the document blocking policy.
  // TODO(nick): xml.txt is logged under HTML, not XML. Not sure if this is a
  // bug with the logging or the test expectation.
  const char* blocked_resources[] = {"valid.html",
                                     "comment_valid.html",
                                     "valid.xml",
                                     "valid.json",
                                     "html.txt",
                                     /* "xml.txt", */  // Broken, see above.
                                     "json.txt"};

  for (const char* resource : blocked_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    base::HistogramTester histograms;

    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell()->web_contents(),
        base::StringPrintf("sendRequest(\"%s\");", resource), &was_blocked));
    ASSERT_FALSE(was_blocked);

    InspectHistograms(histograms, true, resource);
  }

  // These files should be allowed for XHR under the document blocking policy.
  const char* allowed_resources[] = {"js.html",
                                     "comment_js.html",
                                     "js.xml",
                                     "js.json",
                                     "js.txt",
                                     "img.html",
                                     "img.xml",
                                     "img.json",
                                     "img.txt",
                                     "comment_js.html"};
  for (const char* resource : allowed_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    base::HistogramTester histograms;

    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell()->web_contents(),
        base::StringPrintf("sendRequest(\"%s\");", resource), &was_blocked));
    ASSERT_FALSE(was_blocked);

    InspectHistograms(histograms, false, resource);
  }
}

// TODO(dsjang): we cannot run these tests on Android since SetUpCommandLine()
// is executed before the I/O thread is created on Android. After this bug
// (crbug.com/278425) is resolved, we can enable this test case on Android.
#if defined(OS_ANDROID)
#define MAYBE_CrossSiteDocumentBlockingForDifferentTargets \
  DISABLED_CrossSiteDocumentBlockingForDifferentTargets
#else
#define MAYBE_CrossSiteDocumentBlockingForDifferentTargets \
  CrossSiteDocumentBlockingForDifferentTargets
#endif

IN_PROC_BROWSER_TEST_F(SiteIsolationStatsGathererBrowserTest,
                       MAYBE_CrossSiteDocumentBlockingForDifferentTargets) {
  // This webpage loads a cross-site HTML page in different targets such as
  // <img>,<link>,<embed>, etc. Since the requested document is blocked, and one
  // character string (' ') is returned instead, this tests that the renderer
  // does not crash even when it receives a response body which is " ", whose
  // length is different from what's described in "content-length" for such
  // different targets.

  // TODO(nick): Split up these cases, and add positive assertions here about
  // what actually happens in these various resource-block cases.
  GURL foo("http://foo.com/files/cross_site_document_request_target.html");
  NavigateToURL(shell(), foo);
}

}  // namespace content
