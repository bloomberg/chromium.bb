// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/amp_page_load_metrics_observer.h"

#include <algorithm>
#include <string>

#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace {

using AMPViewType = AMPPageLoadMetricsObserver::AMPViewType;

const char kHistogramPrefix[] = "PageLoad.Clients.AMP.";

const char kHistogramAMPDOMContentLoadedEventFired[] =
    "DocumentTiming.NavigationToDOMContentLoadedEventFired";
const char kHistogramAMPFirstLayout[] =
    "DocumentTiming.NavigationToFirstLayout";
const char kHistogramAMPLoadEventFired[] =
    "DocumentTiming.NavigationToLoadEventFired";
const char kHistogramAMPFirstContentfulPaint[] =
    "PaintTiming.NavigationToFirstContentfulPaint";
const char kHistogramAMPParseStart[] = "ParseTiming.NavigationToParseStart";
const char kHistogramAMPParseStartRedirect[] =
    "ParseTiming.NavigationToParseStart.RedirectToNonAmpPage";

const char kHistogramAMPSubframeNavigationToInput[] =
    "Experimental.PageTiming.NavigationToInput.Subframe";
const char kHistogramAMPSubframeInputToNavigation[] =
    "Experimental.PageTiming.InputToNavigation.Subframe";
const char kHistogramAMPSubframeMainFrameToSubFrameNavigation[] =
    "Experimental.PageTiming.MainFrameToSubFrameNavigationDelta.Subframe";
const char kHistogramAMPSubframeFirstContentfulPaint[] =
    "PaintTiming.InputToFirstContentfulPaint.Subframe";
const char kHistogramAMPSubframeFirstContentfulPaintFullNavigation[] =
    "PaintTiming.InputToFirstContentfulPaint.Subframe.FullNavigation";
const char kHistogramAMPSubframeLargestContentPaint[] =
    "PaintTiming.InputToLargestContentPaint.Subframe";
const char kHistogramAMPSubframeLargestContentPaintFullNavigation[] =
    "PaintTiming.InputToLargestContentPaint.Subframe.FullNavigation";
const char kHistogramAMPSubframeFirstInputDelay[] =
    "InteractiveTiming.FirstInputDelay3.Subframe";
const char kHistogramAMPSubframeFirstInputDelayFullNavigation[] =
    "InteractiveTiming.FirstInputDelay3.Subframe.FullNavigation";
const char kHistogramAMPSubframeLayoutStabilityJankScore[] =
    "Experimental.LayoutStability.JankScore.Subframe";
const char kHistogramAMPSubframeLayoutStabilityJankScoreFullNavigation[] =
    "Experimental.LayoutStability.JankScore.Subframe.FullNavigation";

// Host pattern for AMP Cache URLs.
// See https://developers.google.com/amp/cache/overview#amp-cache-url-format
// for a definition of the format of AMP Cache URLs.
const char kAmpCacheHostSuffix[] = "cdn.ampproject.org";

#define RECORD_HISTOGRAM_FOR_TYPE(name, amp_view_type, value)                 \
  do {                                                                        \
    PAGE_LOAD_HISTOGRAM(std::string(kHistogramPrefix).append(name), value);   \
    switch (amp_view_type) {                                                  \
      case AMPViewType::AMP_CACHE:                                            \
        PAGE_LOAD_HISTOGRAM(                                                  \
            std::string(kHistogramPrefix).append("AmpCache.").append(name),   \
            value);                                                           \
        break;                                                                \
      case AMPViewType::GOOGLE_SEARCH_AMP_VIEWER:                             \
        PAGE_LOAD_HISTOGRAM(std::string(kHistogramPrefix)                     \
                                .append("GoogleSearch.")                      \
                                .append(name),                                \
                            value);                                           \
        break;                                                                \
      case AMPViewType::GOOGLE_NEWS_AMP_VIEWER:                               \
        PAGE_LOAD_HISTOGRAM(                                                  \
            std::string(kHistogramPrefix).append("GoogleNews.").append(name), \
            value);                                                           \
        break;                                                                \
      case AMPViewType::NONE:                                                 \
        NOTREACHED();                                                         \
        break;                                                                \
      case AMPViewType::AMP_VIEW_TYPE_LAST:                                   \
        break;                                                                \
    }                                                                         \
  } while (false)

GURL GetCanonicalizedSameDocumentUrl(const GURL& url) {
  if (!url.has_ref())
    return url;

  // We're only interested in same document navigations where the full URL
  // changes, so we ignore the 'ref' or '#fragment' portion of the URL.
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

// Extracts the AMP viewer URL from an AMP cache URL, as encoded in a fragment
// parameter.
GURL GetViewerUrlFromCacheUrl(const GURL& url) {
  // The viewer URL is encoded in the fragment as a query string parameter
  // (&viewerURL=<URL>). net::QueryIterator only operates on the query string,
  // so we copy the fragment into the query string, then iterate over the
  // parameters below.
  std::string ref = url.ref();
  GURL::Replacements replacements;
  replacements.SetQuery(ref.c_str(), url::Component(0, ref.length()));
  GURL modified_url = url.ReplaceComponents(replacements);
  for (net::QueryIterator it(modified_url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == "viewerUrl")
      return GURL(it.GetUnescapedValue());
  }
  return GURL::EmptyGURL();
}

base::TimeDelta ClampToZero(base::TimeDelta t) {
  return std::max(base::TimeDelta(), t);
}

}  // namespace

AMPPageLoadMetricsObserver::AMPPageLoadMetricsObserver() {}

AMPPageLoadMetricsObserver::~AMPPageLoadMetricsObserver() {}

AMPPageLoadMetricsObserver::SubFrameInfo::SubFrameInfo() = default;
AMPPageLoadMetricsObserver::SubFrameInfo::~SubFrameInfo() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AMPPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  current_url_ = navigation_handle->GetURL();
  view_type_ = GetAMPViewType(current_url_);
  ProcessMainFrameNavigation(navigation_handle, view_type_);
  return CONTINUE_OBSERVING;
}

void AMPPageLoadMetricsObserver::OnCommitSameDocumentNavigation(
    content::NavigationHandle* navigation_handle) {
  const GURL url = GetCanonicalizedSameDocumentUrl(navigation_handle->GetURL());

  // Ignore same document navigations where the URL doesn't change.
  if (url == current_url_)
    return;
  current_url_ = url;

  // We're transitioning to a new URL, so record metrics for the previous AMP
  // document, if any.
  MaybeRecordAmpDocumentMetrics();
  current_main_frame_nav_info_ = nullptr;

  AMPViewType same_document_view_type = GetAMPViewType(url);
  if (same_document_view_type == AMPViewType::NONE)
    return;

  // Count how often AMP same-document navigations occur.
  UMA_HISTOGRAM_ENUMERATION(
      std::string(kHistogramPrefix).append("SameDocumentView"),
      same_document_view_type, AMPViewType::AMP_VIEW_TYPE_LAST);

  ProcessMainFrameNavigation(navigation_handle, same_document_view_type);
}

void AMPPageLoadMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (!navigation_handle->HasCommitted())
    return;

  // A new navigation is committing, so ensure any old information associated
  // with this frame is discarded.
  amp_subframe_info_.erase(navigation_handle->GetRenderFrameHost());

  // Only track frames that are direct descendents of the main frame.
  if (navigation_handle->GetParentFrame() == nullptr ||
      navigation_handle->GetParentFrame()->GetParent() != nullptr)
    return;

  // Only track frames that have AMP cache URLs.
  if (GetAMPViewType(navigation_handle->GetURL()) != AMPViewType::AMP_CACHE)
    return;

  GURL viewer_url = GetViewerUrlFromCacheUrl(navigation_handle->GetURL());
  if (viewer_url.is_empty())
    return;

  // Record information about the AMP document loaded in this subframe, which we
  // may use later to record metrics.
  auto& subframe_info =
      amp_subframe_info_[navigation_handle->GetRenderFrameHost()];
  subframe_info.viewer_url = viewer_url;
  subframe_info.navigation_start = navigation_handle->NavigationStart();

  // If the current MainFrameNavigationInfo doesn't yet have a subframe
  // RenderFrameHost, and its URL matches our viewer URL, then associate the
  // MainFrameNavigationInfo with this frame.
  if (current_main_frame_nav_info_ &&
      current_main_frame_nav_info_->subframe_rfh == nullptr &&
      subframe_info.viewer_url == current_main_frame_nav_info_->url) {
    current_main_frame_nav_info_->subframe_rfh =
        navigation_handle->GetRenderFrameHost();
  }
}

void AMPPageLoadMetricsObserver::OnFrameDeleted(content::RenderFrameHost* rfh) {
  if (current_main_frame_nav_info_ &&
      current_main_frame_nav_info_->subframe_rfh == rfh) {
    MaybeRecordAmpDocumentMetrics();
    current_main_frame_nav_info_->subframe_rfh = nullptr;
  }

  amp_subframe_info_.erase(rfh);
}

void AMPPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (view_type_ == AMPViewType::NONE)
    return;

  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start, info)) {
    return;
  }
  RECORD_HISTOGRAM_FOR_TYPE(
      kHistogramAMPDOMContentLoadedEventFired, view_type_,
      timing.document_timing->dom_content_loaded_event_start.value());
}

void AMPPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (view_type_ == AMPViewType::NONE)
    return;

  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, info)) {
    return;
  }
  RECORD_HISTOGRAM_FOR_TYPE(kHistogramAMPLoadEventFired, view_type_,
                            timing.document_timing->load_event_start.value());
}

void AMPPageLoadMetricsObserver::OnFirstLayout(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (view_type_ == AMPViewType::NONE)
    return;

  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->first_layout, info)) {
    return;
  }
  RECORD_HISTOGRAM_FOR_TYPE(kHistogramAMPFirstLayout, view_type_,
                            timing.document_timing->first_layout.value());
}

void AMPPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (view_type_ == AMPViewType::NONE)
    return;

  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    return;
  }
  RECORD_HISTOGRAM_FOR_TYPE(
      kHistogramAMPFirstContentfulPaint, view_type_,
      timing.paint_timing->first_contentful_paint.value());
}

void AMPPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, info)) {
    return;
  }

  if (view_type_ == AMPViewType::NONE) {
    // If we ended up on a non-AMP document, but the initial URL matched an AMP
    // document, record the time it took to get to this point. We encounter this
    // case in the Google News AMP viewer, for example: when a user loads a news
    // AMP URL in a non-same-document navigation context, the user is presented
    // with a redirect prompt which they must click through to continue to the
    // canonical document on the non-AMP origin.
    AMPViewType initial_view_type = GetAMPViewType(info.start_url);
    if (initial_view_type != AMPViewType::NONE) {
      RECORD_HISTOGRAM_FOR_TYPE(kHistogramAMPParseStartRedirect,
                                initial_view_type,
                                timing.parse_timing->parse_start.value());
    }
    return;
  }

  RECORD_HISTOGRAM_FOR_TYPE(kHistogramAMPParseStart, view_type_,
                            timing.parse_timing->parse_start.value());
}

void AMPPageLoadMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (subframe_rfh == nullptr)
    return;

  auto it = amp_subframe_info_.find(subframe_rfh);
  if (it == amp_subframe_info_.end())
    return;

  it->second.timing = timing.Clone();
}

void AMPPageLoadMetricsObserver::OnSubFrameRenderDataUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageRenderData& render_data,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (subframe_rfh == nullptr)
    return;

  auto it = amp_subframe_info_.find(subframe_rfh);
  if (it == amp_subframe_info_.end())
    return;

  it->second.render_data = render_data.Clone();
}

void AMPPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  MaybeRecordAmpDocumentMetrics();
  current_main_frame_nav_info_ = nullptr;
}

void AMPPageLoadMetricsObserver::ProcessMainFrameNavigation(
    content::NavigationHandle* navigation_handle,
    AMPViewType view_type) {
  if (view_type != AMPViewType::GOOGLE_SEARCH_AMP_VIEWER)
    return;

  // Find the subframe RenderFrameHost hosting the AMP document for this
  // navigation. Note that in some cases, the subframe may not exist yet, in
  // which case logic in OnDidFinishSubFrameNavigation will associate the
  // subframe with current_main_frame_nav_info_.
  content::RenderFrameHost* subframe_rfh = nullptr;
  for (const auto& kv : amp_subframe_info_) {
    if (navigation_handle->GetURL() == kv.second.viewer_url) {
      subframe_rfh = kv.first;
      break;
    }
  }

  current_main_frame_nav_info_ = base::WrapUnique(new MainFrameNavigationInfo{
      navigation_handle->GetURL(),
      ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                             ukm::SourceIdType::NAVIGATION_ID),
      subframe_rfh, navigation_handle->NavigationStart(),
      navigation_handle->IsSameDocument()});
}

void AMPPageLoadMetricsObserver::MaybeRecordAmpDocumentMetrics() {
  if (current_main_frame_nav_info_ == nullptr ||
      current_main_frame_nav_info_->subframe_rfh == nullptr)
    return;

  auto it = amp_subframe_info_.find(current_main_frame_nav_info_->subframe_rfh);
  if (it == amp_subframe_info_.end())
    return;

  const SubFrameInfo& subframe_info = it->second;
  if (subframe_info.viewer_url != current_main_frame_nav_info_->url)
    return;

  // TimeDeltas in subframe_info are relative to the navigation start in the AMP
  // subframe. Given that AMP subframes can be prerendered and thus their
  // navigation start may be long before a user initiates the navigation to that
  // AMP document, we need to adjust the times by the difference between the
  // top-level navigation start (which is when the top-level URL was updated to
  // reflect the AMP Viewer URL for the AMP document) and the navigation start
  // in the AMP subframe. Note that we use the top-level navigation start as our
  // best estimate of when the user initiated the navigation.
  base::TimeDelta navigation_input_delta =
      current_main_frame_nav_info_->navigation_start -
      subframe_info.navigation_start;

  ukm::builders::AmpPageLoad builder(
      current_main_frame_nav_info_->ukm_source_id);
  builder.SetSubFrame_MainFrameToSubFrameNavigationDelta(
      -navigation_input_delta.InMilliseconds());

  if (!current_main_frame_nav_info_->is_same_document_navigation) {
    // For non same document navigations, we expect the main frame navigation
    // to be before the subframe navigation. This measures the time from main
    // frame navigation to the time the AMP subframe is added to the document.
    PAGE_LOAD_HISTOGRAM(
        std::string(kHistogramPrefix)
            .append(kHistogramAMPSubframeMainFrameToSubFrameNavigation),
        -navigation_input_delta);
  } else {
    if (navigation_input_delta >= base::TimeDelta()) {
      // Prerender case: subframe navigation happens before main frame
      // navigation.
      PAGE_LOAD_HISTOGRAM(std::string(kHistogramPrefix)
                              .append(kHistogramAMPSubframeNavigationToInput),
                          navigation_input_delta);
    } else {
      // For same document navigations, if the main frame navigation is
      // initiated before the AMP subframe is navigated,
      // |navigation_input_delta| will be negative. This happens in the
      // non-prerender case. We record this delta to ensure it's consistently a
      // small value (the expected case).
      PAGE_LOAD_HISTOGRAM(std::string(kHistogramPrefix)
                              .append(kHistogramAMPSubframeInputToNavigation),
                          -navigation_input_delta);
    }
  }

  if (!subframe_info.timing.is_null()) {
    if (subframe_info.timing->paint_timing->first_contentful_paint
            .has_value()) {
      builder.SetSubFrame_PaintTiming_NavigationToFirstContentfulPaint(
          subframe_info.timing->paint_timing->first_contentful_paint.value()
              .InMilliseconds());

      base::TimeDelta first_contentful_paint = ClampToZero(
          subframe_info.timing->paint_timing->first_contentful_paint.value() -
          navigation_input_delta);
      if (current_main_frame_nav_info_->is_same_document_navigation) {
        PAGE_LOAD_HISTOGRAM(
            std::string(kHistogramPrefix)
                .append(kHistogramAMPSubframeFirstContentfulPaint),
            first_contentful_paint);
      } else {
        PAGE_LOAD_HISTOGRAM(
            std::string(kHistogramPrefix)
                .append(
                    kHistogramAMPSubframeFirstContentfulPaintFullNavigation),
            first_contentful_paint);
      }
    }

    base::Optional<base::TimeDelta> largest_content_paint_time;
    uint64_t largest_content_paint_size;
    PageLoadMetricsObserver::LargestContentType largest_content_type;
    if (AssignTimeAndSizeForLargestContentfulPaint(
            subframe_info.timing->paint_timing, &largest_content_paint_time,
            &largest_content_paint_size, &largest_content_type)) {
      builder.SetSubFrame_PaintTiming_NavigationToLargestContentPaint(
          largest_content_paint_time.value().InMilliseconds());

      // Adjust by the navigation_input_delta.
      largest_content_paint_time = ClampToZero(
          largest_content_paint_time.value() - navigation_input_delta);
      if (current_main_frame_nav_info_->is_same_document_navigation) {
        PAGE_LOAD_HISTOGRAM(
            std::string(kHistogramPrefix)
                .append(kHistogramAMPSubframeLargestContentPaint),
            largest_content_paint_time.value());
      } else {
        PAGE_LOAD_HISTOGRAM(
            std::string(kHistogramPrefix)
                .append(kHistogramAMPSubframeLargestContentPaintFullNavigation),
            largest_content_paint_time.value());
      }
    }

    if (subframe_info.timing->interactive_timing->first_input_delay
            .has_value()) {
      builder.SetSubFrame_InteractiveTiming_FirstInputDelay3(
          subframe_info.timing->interactive_timing->first_input_delay.value()
              .InMilliseconds());

      if (current_main_frame_nav_info_->is_same_document_navigation) {
        UMA_HISTOGRAM_CUSTOM_TIMES(
            std::string(kHistogramPrefix)
                .append(kHistogramAMPSubframeFirstInputDelay),
            subframe_info.timing->interactive_timing->first_input_delay.value(),
            base::TimeDelta::FromMilliseconds(1),
            base::TimeDelta::FromSeconds(60), 50);
      } else {
        UMA_HISTOGRAM_CUSTOM_TIMES(
            std::string(kHistogramPrefix)
                .append(kHistogramAMPSubframeFirstInputDelayFullNavigation),
            subframe_info.timing->interactive_timing->first_input_delay.value(),
            base::TimeDelta::FromMilliseconds(1),
            base::TimeDelta::FromSeconds(60), 50);
      }
    }
  }

  if (!subframe_info.render_data.is_null()) {
    // Clamp the score to a max of 10, which is equivalent to a frame with 10
    // full-frame janks.
    float clamped_jank_score =
        std::min(subframe_info.render_data->layout_jank_score, 10.0f);

    // For UKM, report (jank_score * 100) as an int in the range [0, 1000].
    builder.SetSubFrame_LayoutStability_JankScore(
        static_cast<int>(roundf(clamped_jank_score * 100.0f)));

    // For UMA, report (jank_score * 10) an an int in the range [0,100].
    int32_t uma_value = static_cast<int>(roundf(clamped_jank_score * 10.0f));
    if (current_main_frame_nav_info_->is_same_document_navigation) {
      UMA_HISTOGRAM_COUNTS_100(
          std::string(kHistogramPrefix)
              .append(kHistogramAMPSubframeLayoutStabilityJankScore),
          uma_value);
    } else {
      UMA_HISTOGRAM_COUNTS_100(
          std::string(kHistogramPrefix)
              .append(
                  kHistogramAMPSubframeLayoutStabilityJankScoreFullNavigation),
          uma_value);
    }
  }

  builder.Record(ukm::UkmRecorder::Get());
}

// static
AMPPageLoadMetricsObserver::AMPViewType
AMPPageLoadMetricsObserver::GetAMPViewType(const GURL& url) {
  const char kAmpViewerUrlPrefix[] = "/amp/";

  if (base::EndsWith(url.host(), kAmpCacheHostSuffix,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    return AMPViewType::AMP_CACHE;
  }

  base::Optional<std::string> google_hostname_prefix =
      page_load_metrics::GetGoogleHostnamePrefix(url);
  if (!google_hostname_prefix.has_value())
    return AMPViewType::NONE;

  if (google_hostname_prefix.value() == "www" &&
      base::StartsWith(url.path_piece(), kAmpViewerUrlPrefix,
                       base::CompareCase::SENSITIVE) &&
      url.path_piece().length() > strlen(kAmpViewerUrlPrefix)) {
    return AMPViewType::GOOGLE_SEARCH_AMP_VIEWER;
  }

  if (google_hostname_prefix.value() == "news" &&
      url.path_piece() == "/news/amp" && !url.query_piece().empty()) {
    return AMPViewType::GOOGLE_NEWS_AMP_VIEWER;
  }
  return AMPViewType::NONE;
}
