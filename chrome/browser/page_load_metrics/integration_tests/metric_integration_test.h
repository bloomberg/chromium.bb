// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_INTEGRATION_TESTS_METRIC_INTEGRATION_TEST_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_INTEGRATION_TESTS_METRIC_INTEGRATION_TEST_H_

#include "chrome/test/base/in_process_browser_test.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/ukm/test_ukm_recorder.h"

namespace base {
class CommandLine;
}

namespace content {
class WebContents;
}

namespace net {
namespace test_server {
struct HttpRequest;
class HttpResponse;
}  // namespace test_server
}  // namespace net

namespace trace_analyzer {
class TraceAnalyzer;
}

// Base class for end to end integration tests of speed metrics.
// See README.md for more information.
class MetricIntegrationTest : public InProcessBrowserTest {
 public:
  MetricIntegrationTest();
  ~MetricIntegrationTest() override;

  // Override of BrowserTestBase::SetUpOnMainThread.
  void SetUpOnMainThread() override;

 protected:
  // Configures a request handler for the specified URL, which supplies
  // the specified response content.  Example:
  //
  //   Serve("/foo.html", "<html> Hello, World! </html>");
  //
  // The response is served with an HTTP 200 status code and a content
  // type of "text/html; charset=utf-8".
  void Serve(const std::string& url, const std::string& content);

  // Like Serve, but with an artificial time delay in the response.
  void ServeDelayed(const std::string& url,
                    const std::string& content,
                    base::TimeDelta delay);

  // Starts the test server.  Call this after configuring request
  // handlers, and before loading any pages.
  void Start();

  // Navigates Chrome to the specified URL in the current tab.
  void Load(const std::string& relative_url);

  // Convenience helper for Serve + Start + Load of a single HTML
  // resource at the URL "/test.html".
  void LoadHTML(const std::string& content);

  // Begin trace collection for the specified trace categories. The
  // trace includes events from all processes (browser and renderer).
  void StartTracing(const std::vector<std::string>& categories);

  // End trace collection and write trace output as JSON into the
  // specified string.
  void StopTracing(std::string& trace_output);

  // End trace collection and return a TraceAnalyzer which can run
  // queries on the trace output.
  std::unique_ptr<trace_analyzer::TraceAnalyzer> StopTracingAndAnalyze();

  // Returns the WebContents object for the current Chrome tab.
  content::WebContents* web_contents() const;

  // Override for command-line flags.
  void SetUpCommandLine(base::CommandLine* command_line) override;

  ukm::TestAutoSetUkmRecorder& ukm_recorder() { return *ukm_recorder_; }
  base::HistogramTester& histogram_tester() { return *histogram_tester_; }

  // Checks for a single UKM entry under the PageLoad event with the specified
  // metric name and value.
  void ExpectUKMPageLoadMetric(base::StringPiece metric_name,
                               int64_t expected_value);

  void ExpectUKMPageLoadMetricNear(base::StringPiece metric_name,
                                   double expected_value,
                                   double epsilon);

  // Checks that the UMA entry is in the bucket for |expected_value| or within
  // the bucket for |expected_value| +- 1.
  void ExpectUniqueUMAPageLoadMetricNear(base::StringPiece metric_name,
                                         double expected_value);

 private:
  static std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const std::string& relative_url,
      const std::string& content,
      base::TimeDelta delay,
      const net::test_server::HttpRequest& request);

  base::Optional<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  base::Optional<base::HistogramTester> histogram_tester_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_INTEGRATION_TESTS_METRIC_INTEGRATION_TEST_H_
