// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/page_load_histograms.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/url_pattern.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPerformance.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebPerformance;
using WebKit::WebString;
using base::Time;
using base::TimeDelta;
using content::DocumentState;

const size_t kPLTCount = 100;

namespace {

TimeDelta kPLTMin() {
  return TimeDelta::FromMilliseconds(10);
}
TimeDelta kPLTMax() {
  return TimeDelta::FromMinutes(10);
}

#define PLT_HISTOGRAM(name, sample) \
    UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, kPLTMin(), kPLTMax(), kPLTCount);

// Returns the scheme type of the given URL if its type is one for which we
// dump page load histograms. Otherwise returns NULL.
URLPattern::SchemeMasks GetSupportedSchemeType(const GURL& url) {
  if (url.SchemeIs("http"))
    return URLPattern::SCHEME_HTTP;
  else if (url.SchemeIs("https"))
    return URLPattern::SCHEME_HTTPS;
  return static_cast<URLPattern::SchemeMasks>(0);
}

void DumpPerformanceTiming(const WebPerformance& performance,
                           DocumentState* document_state) {
  Time request = document_state->request_time();

  Time navigation_start = Time::FromDoubleT(performance.navigationStart());
  Time request_start = Time::FromDoubleT(performance.requestStart());
  Time response_start = Time::FromDoubleT(performance.responseStart());
  Time load_event_start = Time::FromDoubleT(performance.loadEventStart());
  Time load_event_end = Time::FromDoubleT(performance.loadEventEnd());
  Time begin = (request.is_null() ? navigation_start : request_start);

  DCHECK(!navigation_start.is_null());
  DCHECK(!request_start.is_null());
  DCHECK(!response_start.is_null());
  // TODO(dominich): Investigate conditions under which |load_event_start| and
  // |load_event_end| may be NULL as in the non-PT_ case below. Examples in
  // http://crbug.com/112006.
  // DCHECK(!load_event_start.is_null());
  // DCHECK(!load_event_end.is_null());

  if (document_state->web_timing_histograms_recorded())
    return;
  document_state->set_web_timing_histograms_recorded(true);

  // TODO(simonjam): There is no way to distinguish between abandonment and
  // intentional Javascript navigation before the load event fires.
  // TODO(dominich): Load type breakdown
  if (!load_event_start.is_null()) {
    PLT_HISTOGRAM("PLT.PT_BeginToFinishDoc", load_event_start - begin);
    PLT_HISTOGRAM("PLT.PT_CommitToFinishDoc",
                  load_event_start - response_start);
  }
  if (!load_event_end.is_null()) {
    PLT_HISTOGRAM("PLT.PT_BeginToFinish", load_event_end - begin);
    PLT_HISTOGRAM("PLT.PT_CommitToFinish", load_event_end - response_start);
    PLT_HISTOGRAM("PLT.PT_RequestToFinish", load_event_end - navigation_start);
    PLT_HISTOGRAM("PLT.PT_StartToFinish", load_event_end - request_start);
  }
  if (!load_event_start.is_null() && !load_event_end.is_null()) {
    PLT_HISTOGRAM("PLT.PT_FinishDocToFinish",
                  load_event_end - load_event_start);
  }
  PLT_HISTOGRAM("PLT.PT_BeginToCommit", response_start - begin);
  PLT_HISTOGRAM("PLT.PT_RequestToStart", request_start - navigation_start);
  PLT_HISTOGRAM("PLT.PT_StartToCommit", response_start - request_start);
}

enum MissingStartType {
  START_MISSING = 0x1,
  COMMIT_MISSING = 0x2,
  NAV_START_MISSING = 0x4,
  MISSING_START_TYPE_MAX = 0x8
};

enum AbandonType {
  FINISH_DOC_MISSING = 0x1,
  FINISH_ALL_LOADS_MISSING = 0x2,
  LOAD_EVENT_START_MISSING = 0x4,
  LOAD_EVENT_END_MISSING = 0x8,
  ABANDON_TYPE_MAX = 0x10
};

}  // namespace

PageLoadHistograms::PageLoadHistograms(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      cross_origin_access_count_(0),
      same_origin_access_count_(0) {
}

void PageLoadHistograms::Dump(WebFrame* frame) {
  // We only dump histograms for main frames.
  // In the future, it may be interesting to tag subframes and dump them too.
  if (!frame || frame->parent())
    return;

  // Only dump for supported schemes.
  URLPattern::SchemeMasks scheme_type =
      GetSupportedSchemeType(frame->document().url());
  if (scheme_type == 0)
    return;

  // Ignore multipart requests.
  if (frame->dataSource()->response().isMultipartPayload())
    return;

  DocumentState* document_state =
      DocumentState::FromDataSource(frame->dataSource());

  // Times based on the Web Timing metrics.
  // http://www.w3.org/TR/navigation-timing/
  // TODO(tonyg, jar): We are in the process of vetting these metrics against
  // the existing ones. Once we understand any differences, we will standardize
  // on a single set of metrics.
  DumpPerformanceTiming(frame->performance(), document_state);

  // If we've already dumped, do nothing.
  // This simple bool works because we only dump for the main frame.
  if (document_state->load_histograms_recorded())
    return;

  // Collect measurement times.
  Time start = document_state->start_load_time();
  Time commit = document_state->commit_load_time();

  // TODO(tonyg, jar): Start can be missing after an in-document navigation and
  // possibly other cases like a very premature abandonment of the page.
  // The PLT.MissingStart histogram should help us troubleshoot and then we can
  // remove this.
  Time navigation_start =
      Time::FromDoubleT(frame->performance().navigationStart());
  int missing_start_type = 0;
  missing_start_type |= start.is_null() ? START_MISSING : 0;
  missing_start_type |= commit.is_null() ? COMMIT_MISSING : 0;
  missing_start_type |= navigation_start.is_null() ? NAV_START_MISSING : 0;
  UMA_HISTOGRAM_ENUMERATION("PLT.MissingStart", missing_start_type,
                            MISSING_START_TYPE_MAX);
  if (missing_start_type)
    return;

  // We properly handle null values for the next 3 variables.
  Time request = document_state->request_time();
  Time first_paint = document_state->first_paint_time();
  Time first_paint_after_load = document_state->first_paint_after_load_time();
  Time finish_doc = document_state->finish_document_load_time();
  Time finish_all_loads = document_state->finish_load_time();

  // TODO(tonyg, jar): We suspect a bug in abandonment counting, this temporary
  // histogram should help us to troubleshoot.
  Time load_event_start =
      Time::FromDoubleT(frame->performance().loadEventStart());
  Time load_event_end = Time::FromDoubleT(frame->performance().loadEventEnd());
  int abandon_type = 0;
  abandon_type |= finish_doc.is_null() ? FINISH_DOC_MISSING : 0;
  abandon_type |= finish_all_loads.is_null() ? FINISH_ALL_LOADS_MISSING : 0;
  abandon_type |= load_event_start.is_null() ? LOAD_EVENT_START_MISSING : 0;
  abandon_type |= load_event_end.is_null() ? LOAD_EVENT_END_MISSING : 0;
  UMA_HISTOGRAM_ENUMERATION("PLT.AbandonType", abandon_type, ABANDON_TYPE_MAX);

  // Handle case where user hits "stop" or "back" before loading completely.
  bool abandoned_page = finish_doc.is_null();
  if (abandoned_page) {
    finish_doc = Time::Now();
    document_state->set_finish_document_load_time(finish_doc);
  }

  // TODO(jar): We should really discriminate the definition of "abandon" more
  // finely.  We should have:
  // abandon_before_document_loaded
  // abandon_before_onload_fired

  if (finish_all_loads.is_null()) {
    finish_all_loads = Time::Now();
    document_state->set_finish_load_time(finish_all_loads);
  } else {
    DCHECK(!abandoned_page);  // How can the doc have finished but not the page?
    if (abandoned_page)
      return;  // Don't try to record a stat which is broken.
  }

  document_state->set_load_histograms_recorded(true);

  // Note: Client side redirects will have no request time.
  Time begin = request.is_null() ? start : request;
  TimeDelta begin_to_finish_doc = finish_doc - begin;
  TimeDelta begin_to_finish_all_loads = finish_all_loads - begin;
  TimeDelta start_to_finish_all_loads = finish_all_loads - start;
  TimeDelta start_to_commit = commit - start;

  DocumentState::LoadType load_type = document_state->load_type();

  // The above code sanitized all values of times, in preparation for creating
  // actual histograms.  The remainder of this code could be run at destructor
  // time for the document_state, since all data is intact.

  // Aggregate PLT data across all link types.
  UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned", abandoned_page ? 1 : 0, 2);
  UMA_HISTOGRAM_ENUMERATION("PLT.LoadType", load_type,
      DocumentState::kLoadTypeMax);
  PLT_HISTOGRAM("PLT.StartToCommit", start_to_commit);
  PLT_HISTOGRAM("PLT.CommitToFinishDoc", finish_doc - commit);
  PLT_HISTOGRAM("PLT.FinishDocToFinish", finish_all_loads - finish_doc);
  PLT_HISTOGRAM("PLT.BeginToCommit", commit - begin);
  PLT_HISTOGRAM("PLT.StartToFinish", start_to_finish_all_loads);
  if (!request.is_null()) {
    PLT_HISTOGRAM("PLT.RequestToStart", start - request);
    PLT_HISTOGRAM("PLT.RequestToFinish", finish_all_loads - request);
  }
  PLT_HISTOGRAM("PLT.CommitToFinish", finish_all_loads - commit);

  scoped_ptr<TimeDelta> begin_to_first_paint;
  scoped_ptr<TimeDelta> commit_to_first_paint;
  if (!first_paint.is_null()) {
    // 'first_paint' can be before 'begin' for an unknown reason.
    // See bug http://crbug.com/125273 for details.
    if (begin <= first_paint) {
      begin_to_first_paint.reset(new TimeDelta(first_paint - begin));
      PLT_HISTOGRAM("PLT.BeginToFirstPaint", *begin_to_first_paint);
    }
    DCHECK(commit <= first_paint);
    commit_to_first_paint.reset(new TimeDelta(first_paint - commit));
    PLT_HISTOGRAM("PLT.CommitToFirstPaint", *commit_to_first_paint);
  }
  if (!first_paint_after_load.is_null()) {
    // 'first_paint_after_load' can be before 'begin' for an unknown reason.
    // See bug http://crbug.com/125273 for details.
    if (begin <= first_paint_after_load) {
      PLT_HISTOGRAM("PLT.BeginToFirstPaintAfterLoad",
          first_paint_after_load - begin);
    }
    DCHECK(commit <= first_paint_after_load);
    PLT_HISTOGRAM("PLT.CommitToFirstPaintAfterLoad",
        first_paint_after_load - commit);
    DCHECK(finish_all_loads <= first_paint_after_load);
    PLT_HISTOGRAM("PLT.FinishToFirstPaintAfterLoad",
        first_paint_after_load - finish_all_loads);
  }
  PLT_HISTOGRAM("PLT.BeginToFinishDoc", begin_to_finish_doc);
  PLT_HISTOGRAM("PLT.BeginToFinish", begin_to_finish_all_loads);

  // Load type related histograms.
  switch (load_type) {
    case DocumentState::UNDEFINED_LOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_UndefLoad", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_UndefLoad", begin_to_finish_all_loads);
      break;
    case DocumentState::RELOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_Reload", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_Reload", begin_to_finish_all_loads);
      break;
    case DocumentState::HISTORY_LOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_HistoryLoad", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_HistoryLoad", begin_to_finish_all_loads);
      break;
    case DocumentState::NORMAL_LOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_NormalLoad", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_NormalLoad", begin_to_finish_all_loads);
      break;
    case DocumentState::LINK_LOAD_NORMAL:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadNormal",
          begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadNormal",
                    begin_to_finish_all_loads);
      break;
    case DocumentState::LINK_LOAD_RELOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadReload",
           begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadReload",
                    begin_to_finish_all_loads);
      break;
    case DocumentState::LINK_LOAD_CACHE_STALE_OK:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadStaleOk",
           begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadStaleOk",
                    begin_to_finish_all_loads);
      break;
    case DocumentState::LINK_LOAD_CACHE_ONLY:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadCacheOnly",
           begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadCacheOnly",
                    begin_to_finish_all_loads);
      break;
    default:
      break;
  }

  bool spdy_proxy_auth_origin_is_set = false;
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  spdy_proxy_auth_origin_is_set = true;
#else
  spdy_proxy_auth_origin_is_set = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kSpdyProxyAuthOrigin);
#endif

  if (document_state->was_fetched_via_proxy() &&
      document_state->was_fetched_via_spdy() && spdy_proxy_auth_origin_is_set) {
    UMA_HISTOGRAM_ENUMERATION(
        "PLT.Abandoned_SpdyProxy", abandoned_page ? 1 : 0, 2);
    PLT_HISTOGRAM("PLT.BeginToFinishDoc_SpdyProxy", begin_to_finish_doc);
    PLT_HISTOGRAM("PLT.BeginToFinish_SpdyProxy", begin_to_finish_all_loads);
  }

  // Histograms to determine if prefetch & prerender has an impact on PLT.
  static const bool prefetching_fieldtrial =
      base::FieldTrialList::TrialExists("Prefetch");
  if (prefetching_fieldtrial) {
    if (document_state->was_prefetcher()) {
      PLT_HISTOGRAM(base::FieldTrial::MakeName(
          "PLT.BeginToFinishDoc_ContentPrefetcher", "Prefetch"),
          begin_to_finish_doc);
      PLT_HISTOGRAM(base::FieldTrial::MakeName(
          "PLT.BeginToFinish_ContentPrefetcher", "Prefetch"),
          begin_to_finish_all_loads);
    }
    if (document_state->was_referred_by_prefetcher()) {
      PLT_HISTOGRAM(base::FieldTrial::MakeName(
          "PLT.BeginToFinishDoc_ContentPrefetcherReferrer", "Prefetch"),
          begin_to_finish_doc);
      PLT_HISTOGRAM(base::FieldTrial::MakeName(
          "PLT.BeginToFinish_ContentPrefetcherReferrer", "Prefetch"),
          begin_to_finish_all_loads);
    }
    UMA_HISTOGRAM_ENUMERATION(base::FieldTrial::MakeName(
        "PLT.Abandoned", "Prefetch"),
        abandoned_page ? 1 : 0, 2);
    PLT_HISTOGRAM(base::FieldTrial::MakeName(
        "PLT.BeginToFinishDoc", "Prefetch"),
        begin_to_finish_doc);
    PLT_HISTOGRAM(base::FieldTrial::MakeName(
        "PLT.BeginToFinish", "Prefetch"),
        begin_to_finish_all_loads);
  }

  static const bool prerendering_fieldtrial =
      base::FieldTrialList::TrialExists("Prerender");
  if (prerendering_fieldtrial) {
    if (document_state->was_prefetcher()) {
      PLT_HISTOGRAM(base::FieldTrial::MakeName(
          "PLT.BeginToFinishDoc_ContentPrefetcher", "Prerender"),
          begin_to_finish_doc);
      PLT_HISTOGRAM(base::FieldTrial::MakeName(
          "PLT.BeginToFinish_ContentPrefetcher", "Prerender"),
          begin_to_finish_all_loads);
    }
    if (document_state->was_referred_by_prefetcher()) {
      PLT_HISTOGRAM(base::FieldTrial::MakeName(
          "PLT.BeginToFinishDoc_ContentPrefetcherReferrer", "Prerender"),
          begin_to_finish_doc);
      PLT_HISTOGRAM(base::FieldTrial::MakeName(
          "PLT.BeginToFinish_ContentPrefetcherReferrer", "Prerender"),
          begin_to_finish_all_loads);
    }
    UMA_HISTOGRAM_ENUMERATION(base::FieldTrial::MakeName(
        "PLT.Abandoned", "Prerender"),
        abandoned_page ? 1 : 0, 2);
    PLT_HISTOGRAM(base::FieldTrial::MakeName(
        "PLT.BeginToFinishDoc", "Prerender"),
        begin_to_finish_doc);
    PLT_HISTOGRAM(base::FieldTrial::MakeName(
        "PLT.BeginToFinish", "Prerender"),
        begin_to_finish_all_loads);
  }

  // Histograms to determine if SDCH has an impact.
  // TODO(jar): Consider removing per-link load types and the enumeration.
  static const bool use_sdch_histogram =
      base::FieldTrialList::TrialExists("GlobalSdch");
  if (use_sdch_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("PLT.LoadType", "GlobalSdch"),
        load_type, DocumentState::kLoadTypeMax);
    switch (load_type) {
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_ONLY:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadCacheOnly", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // TODO(mpcomplete): remove the extension-related histograms after we collect
  // enough data. http://crbug.com/100411
  chrome::ChromeContentRendererClient* client =
      static_cast<chrome::ChromeContentRendererClient*>(
          content::GetContentClient()->renderer());

  const bool use_adblock_histogram = client->IsAdblockInstalled();
  if (use_adblock_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        "PLT.Abandoned_ExtensionAdblock",
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_NormalLoad_ExtensionAdblock",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadNormal_ExtensionAdblock",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadReload_ExtensionAdblock",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadStaleOk_ExtensionAdblock",
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  const bool use_adblockplus_histogram = client->IsAdblockPlusInstalled();
  if (use_adblockplus_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        "PLT.Abandoned_ExtensionAdblockPlus",
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_NormalLoad_ExtensionAdblockPlus",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadNormal_ExtensionAdblockPlus",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadReload_ExtensionAdblockPlus",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadStaleOk_ExtensionAdblockPlus",
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  const bool use_webrequest_adblock_histogram =
      client->IsAdblockWithWebRequestInstalled();
  if (use_webrequest_adblock_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        "PLT.Abandoned_ExtensionWebRequestAdblock",
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_NormalLoad_ExtensionWebRequestAdblock",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadNormal_ExtensionWebRequestAdblock",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadReload_ExtensionWebRequestAdblock",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadStaleOk_ExtensionWebRequestAdblock",
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  const bool use_webrequest_adblockplus_histogram =
      client->IsAdblockPlusWithWebRequestInstalled();
  if (use_webrequest_adblockplus_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        "PLT.Abandoned_ExtensionWebRequestAdblockPlus",
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_NormalLoad_ExtensionWebRequestAdblockPlus",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadNormal_ExtensionWebRequestAdblockPlus",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadReload_ExtensionWebRequestAdblockPlus",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadStaleOk_ExtensionWebRequestAdblockPlus",
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  const bool use_webrequest_other_histogram =
      client->IsOtherExtensionWithWebRequestInstalled();
  if (use_webrequest_other_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        "PLT.Abandoned_ExtensionWebRequestOther",
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_NormalLoad_ExtensionWebRequestOther",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadNormal_ExtensionWebRequestOther",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadReload_ExtensionWebRequestOther",
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(
            "PLT.BeginToFinish_LinkLoadStaleOk_ExtensionWebRequestOther",
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // For the SPDY field trials, we need to verify that the page loaded was
  // the type we requested:
  //   if we asked for a SPDY request, we got a SPDY request
  //   if we asked for a HTTP request, we got a HTTP request
  // Due to spdy version mismatches, it is possible that we ask for SPDY
  // but didn't get SPDY.
  static const bool use_spdy_histogram =
      base::FieldTrialList::TrialExists("SpdyImpact");
  if (use_spdy_histogram) {
    // We take extra effort to only compute these once.
    static bool in_spdy_trial = base::FieldTrialList::Find(
        "SpdyImpact")->group_name() == "npn_with_spdy";
    static bool in_http_trial = base::FieldTrialList::Find(
        "SpdyImpact")->group_name() == "npn_with_http";

    bool spdy_trial_success = document_state->was_fetched_via_spdy() ?
        in_spdy_trial : in_http_trial;
    if (spdy_trial_success) {
      // Histograms to determine if SPDY has an impact for https traffic.
      // TODO(mbelshe): After we've seen the difference between BeginToFinish
      //                and StartToFinish, consider removing one or the other.
      if (scheme_type == URLPattern::SCHEME_HTTPS &&
          document_state->was_npn_negotiated()) {
        UMA_HISTOGRAM_ENUMERATION(
            base::FieldTrial::MakeName("PLT.Abandoned", "SpdyImpact"),
            abandoned_page ? 1 : 0, 2);
        switch (load_type) {
          case DocumentState::LINK_LOAD_NORMAL:
            PLT_HISTOGRAM(base::FieldTrial::MakeName(
                "PLT.BeginToFinish_LinkLoadNormal", "SpdyImpact"),
                begin_to_finish_all_loads);
            PLT_HISTOGRAM(base::FieldTrial::MakeName(
                "PLT.StartToFinish_LinkLoadNormal", "SpdyImpact"),
                start_to_finish_all_loads);
            PLT_HISTOGRAM(base::FieldTrial::MakeName(
                "PLT.StartToCommit_LinkLoadNormal", "SpdyImpact"),
                start_to_commit);
            break;
          case DocumentState::NORMAL_LOAD:
            PLT_HISTOGRAM(base::FieldTrial::MakeName(
                "PLT.BeginToFinish_NormalLoad", "SpdyImpact"),
                begin_to_finish_all_loads);
            PLT_HISTOGRAM(base::FieldTrial::MakeName(
                "PLT.StartToFinish_NormalLoad", "SpdyImpact"),
                start_to_finish_all_loads);
            PLT_HISTOGRAM(base::FieldTrial::MakeName(
                "PLT.StartToCommit_NormalLoad", "SpdyImpact"),
                start_to_commit);
            break;
          default:
            break;
        }
      }

      // Histograms to compare the impact of alternate protocol over http
      // traffic: when spdy is used vs. when http is used.
      if (scheme_type == URLPattern::SCHEME_HTTP &&
          document_state->was_alternate_protocol_available()) {
        if (!document_state->was_npn_negotiated()) {
          // This means that even there is alternate protocols for npn_http or
          // npn_spdy, they are not taken (due to the base::FieldTrial).
          switch (load_type) {
            case DocumentState::LINK_LOAD_NORMAL:
              PLT_HISTOGRAM(
                  "PLT.StartToFinish_LinkLoadNormal_AlternateProtocol_http",
                  start_to_finish_all_loads);
              PLT_HISTOGRAM(
                  "PLT.StartToCommit_LinkLoadNormal_AlternateProtocol_http",
                  start_to_commit);
              break;
            case DocumentState::NORMAL_LOAD:
              PLT_HISTOGRAM(
                  "PLT.StartToFinish_NormalLoad_AlternateProtocol_http",
                  start_to_finish_all_loads);
              PLT_HISTOGRAM(
                  "PLT.StartToCommit_NormalLoad_AlternateProtocol_http",
                  start_to_commit);
              break;
            default:
              break;
          }
        } else if (document_state->was_fetched_via_spdy()) {
          switch (load_type) {
            case DocumentState::LINK_LOAD_NORMAL:
              PLT_HISTOGRAM(
                  "PLT.StartToFinish_LinkLoadNormal_AlternateProtocol_spdy",
                  start_to_finish_all_loads);
              PLT_HISTOGRAM(
                  "PLT.StartToCommit_LinkLoadNormal_AlternateProtocol_spdy",
                  start_to_commit);
              break;
            case DocumentState::NORMAL_LOAD:
              PLT_HISTOGRAM(
                  "PLT.StartToFinish_NormalLoad_AlternateProtocol_spdy",
                  start_to_finish_all_loads);
              PLT_HISTOGRAM(
                  "PLT.StartToCommit_NormalLoad_AlternateProtocol_spdy",
                  start_to_commit);
              break;
            default:
              break;
          }
        }
      }
    }
  }

  // Record SpdyCwnd field trial results.
  if (document_state->was_fetched_via_spdy()) {
    switch (load_type) {
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "SpdyCwnd"),
            begin_to_finish_all_loads);
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.StartToFinish_LinkLoadNormal", "SpdyCwnd"),
            start_to_finish_all_loads);
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.StartToCommit_LinkLoadNormal", "SpdyCwnd"),
            start_to_commit);
        break;
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "SpdyCwnd"),
            begin_to_finish_all_loads);
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.StartToFinish_NormalLoad", "SpdyCwnd"),
            start_to_finish_all_loads);
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.StartToCommit_NormalLoad", "SpdyCwnd"),
            start_to_commit);
        break;
      default:
        break;
    }
  }

  // Record page load time and abandonment rates for proxy cases.
  if (document_state->was_fetched_via_proxy()) {
    if (scheme_type == URLPattern::SCHEME_HTTPS) {
      PLT_HISTOGRAM("PLT.StartToFinish.Proxy.https", start_to_finish_all_loads);
      UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned.Proxy.https",
                                abandoned_page ? 1 : 0, 2);
    } else {
      DCHECK(scheme_type == URLPattern::SCHEME_HTTP);
      PLT_HISTOGRAM("PLT.StartToFinish.Proxy.http", start_to_finish_all_loads);
      UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned.Proxy.http",
                                abandoned_page ? 1 : 0, 2);
    }
  } else {
    if (scheme_type == URLPattern::SCHEME_HTTPS) {
      PLT_HISTOGRAM("PLT.StartToFinish.NoProxy.https",
                    start_to_finish_all_loads);
      UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned.NoProxy.https",
                                abandoned_page ? 1 : 0, 2);
    } else {
      DCHECK(scheme_type == URLPattern::SCHEME_HTTP);
      PLT_HISTOGRAM("PLT.StartToFinish.NoProxy.http",
                    start_to_finish_all_loads);
      UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned.NoProxy.http",
                                abandoned_page ? 1 : 0, 2);
    }
  }

  // Histograms to determine if disabling overlapped TCP reads
  // has an impact on PLT.
  static const bool use_overlapped_read_histogram =
      base::FieldTrialList::TrialExists("OverlappedReadImpact");
  if (use_overlapped_read_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("PLT.Abandoned", "OverlappedReadImpact"),
        abandoned_page ? 1 : 0, 2);
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("PLT.LoadType", "OverlappedReadImpact"),
        load_type, DocumentState::kLoadTypeMax);
    switch (load_type) {
      case DocumentState::NORMAL_LOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "OverlappedReadImpact"),
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "OverlappedReadImpact"),
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "OverlappedReadImpact"),
            begin_to_finish_all_loads);
        break;
      case DocumentState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "OverlappedReadImpact"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Site isolation metrics.
  UMA_HISTOGRAM_COUNTS("SiteIsolation.PageLoadsWithCrossSiteFrameAccess",
                       cross_origin_access_count_);
  UMA_HISTOGRAM_COUNTS("SiteIsolation.PageLoadsWithSameSiteFrameAccess",
                       same_origin_access_count_);

  // Log the PLT to the info log.
  LogPageLoadTime(document_state, frame->dataSource());

  // Record histograms for cache sensitivity analysis.
  static const bool cache_sensitivity_histogram =
      base::FieldTrialList::TrialExists("CacheSensitivityAnalysis");
  if (cache_sensitivity_histogram) {
    PLT_HISTOGRAM(base::FieldTrial::MakeName(
        "PLT.BeginToFinishDoc_CacheSensitivity", "CacheSensitivityAnalysis"),
                  begin_to_finish_doc);
    PLT_HISTOGRAM(base::FieldTrial::MakeName(
        "PLT.BeginToFinish_CacheSensitivity", "CacheSensitivityAnalysis"),
                  begin_to_finish_all_loads);
    if (begin_to_first_paint.get()) {
    PLT_HISTOGRAM(base::FieldTrial::MakeName(
        "PLT.BeginToFirstPaint_CacheSensitivity", "CacheSensitivityAnalysis"),
                  *begin_to_first_paint);
    }
    if (commit_to_first_paint.get()) {
      PLT_HISTOGRAM(base::FieldTrial::MakeName(
        "PLT.CommitToFirstPaint_CacheSensitivity", "CacheSensitivityAnalysis"),
                    *commit_to_first_paint);
    }
  }

  // Since there are currently no guarantees that renderer histograms will be
  // sent to the browser, we initiate a PostTask here to be sure that we send
  // the histograms we generated.  Without this call, pages that don't have an
  // on-close-handler might generate data that is lost when the renderer is
  // shutdown abruptly (perchance because the user closed the tab).
  // TODO(jar) BUG=33233: This needs to be moved to a PostDelayedTask, and it
  // should post when the onload is complete, so that it doesn't interfere with
  // the next load.
  content::RenderThread::Get()->UpdateHistograms(
      content::kHistogramSynchronizerReservedSequenceNumber);
}

void PageLoadHistograms::ResetCrossFramePropertyAccess() {
  cross_origin_access_count_ = 0;
  same_origin_access_count_ = 0;
}

void PageLoadHistograms::FrameWillClose(WebFrame* frame) {
  Dump(frame);
}

void PageLoadHistograms::ClosePage() {
  // TODO(davemoore) This code should be removed once willClose() gets
  // called when a page is destroyed. page_load_histograms_.Dump() is safe
  // to call multiple times for the same frame, but it will simplify things.
  Dump(render_view()->GetWebView()->mainFrame());
  ResetCrossFramePropertyAccess();
}

void PageLoadHistograms::LogPageLoadTime(const DocumentState* document_state,
                                         const WebDataSource* ds) const {
  // Because this function gets called on every page load,
  // take extra care to optimize it away if logging is turned off.
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;

  DCHECK(document_state);
  DCHECK(ds);
  GURL url(ds->request().url());
  Time start = document_state->start_load_time();
  Time finish = document_state->finish_load_time();
  // TODO(mbelshe): should we log more stats?
  VLOG(1) << "PLT: " << (finish - start).InMilliseconds() << "ms "
          << url.spec();
}
