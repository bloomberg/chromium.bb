// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/protocol_page_load_metrics_observer.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ProtocolPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  connection_info_ = navigation_handle->GetConnectionInfo();
  return CONTINUE_OBSERVING;
}

void ProtocolPageLoadMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  switch (connection_info_) {
    case net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY2:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY3:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_14:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_15:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP0_9:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_0:
    case net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS:
      return;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H1.PaintTiming."
          "ParseStartToFirstContentfulPaint",
          timing.first_contentful_paint.value() - timing.parse_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H2.PaintTiming."
          "ParseStartToFirstContentfulPaint",
          timing.first_contentful_paint.value() - timing.parse_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.QUIC.PaintTiming."
          "ParseStartToFirstContentfulPaint",
          timing.first_contentful_paint.value() - timing.parse_start.value());
      break;
  }
}

void ProtocolPageLoadMetricsObserver::OnFirstMeaningfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  switch (connection_info_) {
    case net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY2:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY3:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_14:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_15:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP0_9:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_0:
    case net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS:
      return;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H1.Experimental.PaintTiming."
          "ParseStartToFirstMeaningfulPaint",
          timing.first_meaningful_paint.value() - timing.parse_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H2.Experimental.PaintTiming."
          "ParseStartToFirstMeaningfulPaint",
          timing.first_meaningful_paint.value() - timing.parse_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.QUIC.Experimental.PaintTiming."
          "ParseStartToFirstMeaningfulPaint",
          timing.first_meaningful_paint.value() - timing.parse_start.value());
      break;
  }
}

void ProtocolPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  switch (connection_info_) {
    case net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY2:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY3:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_14:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_15:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP0_9:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_0:
    case net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS:
      return;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H1.DocumentTiming."
          "NavigationToDOMContentLoadedEventFired",
          timing.dom_content_loaded_event_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H2.DocumentTiming."
          "NavigationToDOMContentLoadedEventFired",
          timing.dom_content_loaded_event_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.QUIC.DocumentTiming."
          "NavigationToDOMContentLoadedEventFired",
          timing.dom_content_loaded_event_start.value());
      break;
  }
}

void ProtocolPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  switch (connection_info_) {
    case net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY2:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_SPDY3:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_14:
    case net::HttpResponseInfo::CONNECTION_INFO_DEPRECATED_HTTP2_15:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP0_9:
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_0:
    case net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS:
      return;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H1.DocumentTiming."
          "NavigationToLoadEventFired",
          timing.load_event_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_HTTP2:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.H2.DocumentTiming."
          "NavigationToLoadEventFired",
          timing.load_event_start.value());
      break;
    case net::HttpResponseInfo::CONNECTION_INFO_QUIC:
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.Protocol.QUIC.DocumentTiming."
          "NavigationToLoadEventFired",
          timing.load_event_start.value());
      break;
  }
}
