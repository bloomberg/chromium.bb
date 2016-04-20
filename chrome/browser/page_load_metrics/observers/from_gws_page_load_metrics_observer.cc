// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"
#include <string>

#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace internal {

const char kHistogramFromGWSFirstPaint[] =
    "PageLoad.Clients.FromGWS2.Timing2.NavigationToFirstPaint";
const char kHistogramFromGWSFirstTextPaint[] =
    "PageLoad.Clients.FromGWS2.Timing2.NavigationToFirstTextPaint";
const char kHistogramFromGWSFirstImagePaint[] =
    "PageLoad.Clients.FromGWS2.Timing2.NavigationToFirstImagePaint";
const char kHistogramFromGWSFirstContentfulPaint[] =
    "PageLoad.Clients.FromGWS2.Timing2.NavigationToFirstContentfulPaint";
const char kHistogramFromGWSParseStartToFirstContentfulPaint[] =
    "PageLoad.Clients.FromGWS2.Timing2.ParseStartToFirstContentfulPaint";
const char kHistogramFromGWSDomContentLoaded[] =
    "PageLoad.Clients.FromGWS2.Timing2.NavigationToDOMContentLoadedEventFired";
const char kHistogramFromGWSParseDuration[] =
    "PageLoad.Clients.FromGWS2.Timing2.ParseDuration";
const char kHistogramFromGWSLoad[] =
    "PageLoad.Clients.FromGWS2.Timing2.NavigationToLoadEventFired";

}  // namespace internal

// See
// https://docs.google.com/document/d/1jNPZ6Aeh0KV6umw1yZrrkfXRfxWNruwu7FELLx_cpOg/edit
// for additional details.

// static
bool FromGWSPageLoadMetricsLogger::IsGoogleSearchHostname(
    base::StringPiece host) {
  const char kGoogleSearchHostnamePrefix[] = "www.";

  // Hostname must start with 'www.' Hostnames are not case sensitive.
  if (!base::StartsWith(host, kGoogleSearchHostnamePrefix,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return false;
  }
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      host,
      // Do not include private registries, such as appspot.com. We don't want
      // to match URLs like www.google.appspot.com.
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);

  // Domain and registry must start with 'google.' e.g. 'google.com' or
  // 'google.co.uk'.
  if (!base::StartsWith(domain, "google.",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return false;
  }

  // Finally, the length of the URL before the domain and registry must be equal
  // in length to the search hostname prefix.
  const size_t url_hostname_prefix_length = host.length() - domain.length();
  return url_hostname_prefix_length == strlen(kGoogleSearchHostnamePrefix);
}

// static
bool FromGWSPageLoadMetricsLogger::IsGoogleSearchResultUrl(const GURL& url) {
  // NOTE: we do not require 'q=' in the query, as AJAXy search may instead
  // store the query in the URL fragment.
  if (!IsGoogleSearchHostname(url.host_piece())) {
    return false;
  }

  if (!QueryContainsComponentPrefix(url.query_piece(), "q=") &&
      !QueryContainsComponentPrefix(url.ref_piece(), "q=")) {
    return false;
  }

  const base::StringPiece path = url.path_piece();
  return path == "/search" || path == "/webhp" || path == "/custom" ||
         path == "/";
}

// static
bool FromGWSPageLoadMetricsLogger::IsGoogleRedirectorUrl(const GURL& url) {
  return IsGoogleSearchHostname(url.host_piece()) &&
         url.path_piece() == "/url" && url.has_query();
}

// static
bool FromGWSPageLoadMetricsLogger::IsGoogleSearchRedirectorUrl(
    const GURL& url) {
  return IsGoogleRedirectorUrl(url) &&
         // Google search result redirects are differentiated from other
         // redirects by 'source=web'.
         QueryContainsComponent(url.query_piece(), "source=web");
}

// static
bool FromGWSPageLoadMetricsLogger::QueryContainsComponent(
    const base::StringPiece query,
    const base::StringPiece component) {
  return QueryContainsComponentHelper(query, component, false);
}

// static
bool FromGWSPageLoadMetricsLogger::QueryContainsComponentPrefix(
    const base::StringPiece query,
    const base::StringPiece component) {
  return QueryContainsComponentHelper(query, component, true);
}

// static
bool FromGWSPageLoadMetricsLogger::QueryContainsComponentHelper(
    const base::StringPiece query,
    const base::StringPiece component,
    bool component_is_prefix) {
  if (query.empty() || component.empty() ||
      component.length() > query.length()) {
    return false;
  }

  // Verify that the provided query string does not include the query or
  // fragment start character, as the logic below depends on this character not
  // being included.
  DCHECK(query[0] != '?' && query[0] != '#');

  // We shouldn't try to find matches beyond the point where there aren't enough
  // characters left in query to fully match the component.
  const size_t last_search_start = query.length() - component.length();

  // We need to search for matches in a loop, rather than stopping at the first
  // match, because we may initially match a substring that isn't a full query
  // string component. Consider, for instance, the query string 'ab=cd&b=c'. If
  // we search for component 'b=c', the first substring match will be characters
  // 1-3 (zero-based) in the query string. However, this isn't a full component
  // (the full component is ab=cd) so the match will fail. Thus, we must
  // continue our search to find the second substring match, which in the
  // example is at characters 6-8 (the end of the query string) and is a
  // successful component match.
  for (size_t start_offset = 0; start_offset <= last_search_start;
       start_offset += component.length()) {
    start_offset = query.find(component, start_offset);
    if (start_offset == std::string::npos) {
      // We searched to end of string and did not find a match.
      return false;
    }
    // Verify that the character prior to the component is valid (either we're
    // at the beginning of the query string, or are preceded by an ampersand).
    if (start_offset != 0 && query[start_offset - 1] != '&') {
      continue;
    }
    if (!component_is_prefix) {
      // Verify that the character after the component substring is valid
      // (either we're at the end of the query string, or are followed by an
      // ampersand).
      const size_t after_offset = start_offset + component.length();
      if (after_offset < query.length() && query[after_offset] != '&') {
        continue;
      }
    }
    return true;
  }
  return false;
}

FromGWSPageLoadMetricsObserver::FromGWSPageLoadMetricsObserver() {}

void FromGWSPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  logger_.set_previously_committed_url(currently_committed_url);
}

void FromGWSPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  logger_.set_navigation_initiated_via_link(
      navigation_handle->HasUserGesture() &&
      ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_LINK));
}

void FromGWSPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  logger_.OnComplete(timing, extra_info);
}

void FromGWSPageLoadMetricsLogger::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (ShouldLogMetrics(extra_info.committed_url)) {
    LogMetrics(timing, extra_info);
  }
}

bool FromGWSPageLoadMetricsLogger::ShouldLogMetrics(
    const GURL& committed_url) const {
  // If we didn't navigate from another page, then this couldn't be a navigation
  // from search, so don't log stats.
  if (previously_committed_url_.is_empty())
    return false;

  // If this navigation didn't commit, or this page is a known google redirector
  // URL or Google search results URL, then we should not log stats.
  if (committed_url.is_empty() || IsGoogleRedirectorUrl(committed_url) ||
      IsGoogleSearchResultUrl(committed_url))
    return false;

  // We're only interested in tracking user gesture initiated navigations
  // (e.g. clicks) initiated via links.
  if (!navigation_initiated_via_link_)
    return false;

  // We're only interested in navigations from the SRP or through the JS
  // redirector.
  if (!IsGoogleSearchResultUrl(previously_committed_url_) &&
      !IsGoogleSearchRedirectorUrl(previously_committed_url_))
    return false;

  return true;
}

void FromGWSPageLoadMetricsLogger::LogMetrics(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (WasStartedInForegroundEventInForeground(
          timing.dom_content_loaded_event_start, extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSDomContentLoaded,
                        timing.dom_content_loaded_event_start);
  }
  if (WasStartedInForegroundEventInForeground(timing.parse_stop, extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSParseDuration,
                        timing.parse_stop - timing.parse_start);
  }
  if (WasStartedInForegroundEventInForeground(timing.load_event_start,
                                              extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSLoad,
                        timing.load_event_start);
  }
  if (WasStartedInForegroundEventInForeground(timing.first_text_paint,
                                              extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSFirstTextPaint,
                        timing.first_text_paint);
  }
  if (WasStartedInForegroundEventInForeground(timing.first_image_paint,
                                              extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSFirstImagePaint,
                        timing.first_image_paint);
  }
  if (WasStartedInForegroundEventInForeground(timing.first_paint, extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSFirstPaint,
                        timing.first_paint);
  }
  if (WasStartedInForegroundEventInForeground(timing.first_contentful_paint,
                                              extra_info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFromGWSFirstContentfulPaint,
                        timing.first_contentful_paint);

    // If we have a foreground paint, we should have a foreground parse start,
    // since paints can't happen until after parsing starts.
    DCHECK(WasStartedInForegroundEventInForeground(timing.parse_start,
                                                   extra_info));
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramFromGWSParseStartToFirstContentfulPaint,
        timing.first_contentful_paint - timing.parse_start);
  }
}
