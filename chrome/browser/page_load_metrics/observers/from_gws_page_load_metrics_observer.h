// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FROM_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FROM_GWS_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "url/gurl.h"

namespace internal {
// Exposed for tests.
extern const char kHistogramFromGWSFirstTextPaint[];
extern const char kHistogramFromGWSAbortStopBeforePaint[];
extern const char kHistogramFromGWSAbortStopBeforeCommit[];
extern const char kHistogramFromGWSAbortCloseBeforePaint[];
extern const char kHistogramFromGWSAbortCloseBeforeCommit[];
extern const char kHistogramFromGWSAbortNewNavigationBeforePaint[];
extern const char kHistogramFromGWSAbortUnknownNavigationBeforeCommit[];

}  // namespace internal

// FromGWSPageLoadMetricsLogger is a peer class to
// FromGWSPageLoadMetricsObserver. FromGWSPageLoadMetricsLogger is responsible
// for tracking state needed to decide if metrics should be logged, and to log
// metrics in cases where metrics should be logged. FromGWSPageLoadMetricsLogger
// exists to decouple the logging policy implementation from other Chromium
// classes such as NavigationHandle and related infrastructure, in order to make
// the code more unit testable.
class FromGWSPageLoadMetricsLogger {
 public:
  FromGWSPageLoadMetricsLogger() {}

  void SetPreviouslyCommittedUrl(const GURL& url);
  void SetProvisionalUrl(const GURL& url);

  void set_navigation_initiated_via_link(bool navigation_initiated_via_link) {
    navigation_initiated_via_link_ = navigation_initiated_via_link;
  }

  // Invoked when metrics for the given page are complete.
  void OnComplete(const page_load_metrics::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& extra_info);

  // The methods below are public only for testing.
  static bool IsGoogleSearchHostname(base::StringPiece host);
  static bool IsGoogleSearchResultUrl(const GURL& url);
  static bool IsGoogleRedirectorUrl(const GURL& url);
  static bool IsGoogleSearchRedirectorUrl(const GURL& url);
  bool ShouldLogMetrics(const GURL& url);

  // Whether the given query string contains the given component. The query
  // parameter should contain the query string of a URL (the portion following
  // the question mark, excluding the question mark). The component must fully
  // match a component in the query string. For example, 'foo=bar' would match
  // the query string 'a=b&foo=bar&c=d' but would not match 'a=b&zzzfoo=bar&c=d'
  // since, though foo=bar appears in the query string, the key specified in the
  // component 'foo' does not match the full key in the query string
  // 'zzzfoo'. For QueryContainsComponent, the component should of the form
  // 'key=value'. For QueryContainsComponentPrefix, the component should be of
  // the form 'key=' (where the value is not specified).
  static bool QueryContainsComponent(const base::StringPiece query,
                                     const base::StringPiece component);
  static bool QueryContainsComponentPrefix(const base::StringPiece query,
                                           const base::StringPiece component);

 private:
  bool previously_committed_url_is_search_results_ = false;
  bool previously_committed_url_is_search_redirector_ = false;
  bool navigation_initiated_via_link_ = false;
  bool provisional_url_is_search_results_or_google_redirector_ = false;

  static bool IsUrlFromGWS(const GURL& url);

  // Common helper for QueryContainsComponent and QueryContainsComponentPrefix.
  static bool QueryContainsComponentHelper(const base::StringPiece query,
                                           const base::StringPiece component,
                                           bool component_is_prefix);

  void LogCommittedMetrics(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info);
  void LogProvisionalMetrics(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info);

  DISALLOW_COPY_AND_ASSIGN(FromGWSPageLoadMetricsLogger);
};

class FromGWSPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  FromGWSPageLoadMetricsObserver();
  // page_load_metrics::PageLoadMetricsObserver implementation:
  void OnStart(content::NavigationHandle* navigation_handle,
               const GURL& currently_committed_url) override;
  void OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnComplete(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

 private:
  FromGWSPageLoadMetricsLogger logger_;

  DISALLOW_COPY_AND_ASSIGN(FromGWSPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FROM_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
