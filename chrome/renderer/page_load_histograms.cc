// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/page_load_histograms.h"

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/renderer/navigation_state.h"
#include "chrome/renderer/render_thread.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"

using base::Time;
using base::TimeDelta;
using WebKit::WebDataSource;
using WebKit::WebFrame;

static const TimeDelta kPLTMin(TimeDelta::FromMilliseconds(10));
static const TimeDelta kPLTMax(TimeDelta::FromMinutes(10));
static const size_t kPLTCount(100);

#define PLT_HISTOGRAM(name, sample) \
    UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, kPLTMin, kPLTMax, kPLTCount);

// Returns the scheme type of the given URL if its type is one for which we
// dump page load histograms. Otherwise returns NULL.
static URLPattern::SchemeMasks GetSupportedSchemeType(const GURL& url) {
  if (url.SchemeIs("http"))
    return URLPattern::SCHEME_HTTP;
  else if (url.SchemeIs("https"))
    return URLPattern::SCHEME_HTTPS;
  return static_cast<URLPattern::SchemeMasks>(0);
}

PageLoadHistograms::PageLoadHistograms()
    : has_dumped_(false),
      cross_origin_access_count_(0),
      same_origin_access_count_(0) {
}

void PageLoadHistograms::Dump(WebFrame* frame) {
  // We only dump histograms for main frames.
  // In the future, it may be interesting to tag subframes and dump them too.
  if (!frame || frame->parent())
    return;

  // If we've already dumped, do nothing.
  // This simple bool works because we only dump for the main frame.
  if (has_dumped_)
    return;

  // Only dump for supported schemes.
  URLPattern::SchemeMasks scheme_type = GetSupportedSchemeType(frame->url());
  if (scheme_type == 0)
    return;

  // Configuration for PLT related histograms.
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->dataSource());

  // Collect measurement times.
  Time start = navigation_state->start_load_time();
  if (start.is_null())
    return;  // Probably very premature abandonment of page.
  Time commit = navigation_state->commit_load_time();
  if (commit.is_null())
    return;  // Probably very premature abandonment of page.

  // We properly handle null values for the next 3 variables.
  Time request = navigation_state->request_time();
  Time first_paint = navigation_state->first_paint_time();
  Time first_paint_after_load = navigation_state->first_paint_after_load_time();
  Time finish_doc = navigation_state->finish_document_load_time();
  Time finish_all_loads = navigation_state->finish_load_time();

  // Handle case where user hits "stop" or "back" before loading completely.
  bool abandoned_page = finish_doc.is_null();
  if (abandoned_page) {
    finish_doc = Time::Now();
    navigation_state->set_finish_document_load_time(finish_doc);
  }

  // TODO(jar): We should really discriminate the definition of "abandon" more
  // finely.  We should have:
  // abandon_before_document_loaded
  // abandon_before_onload_fired

  if (finish_all_loads.is_null()) {
    finish_all_loads = Time::Now();
    navigation_state->set_finish_load_time(finish_all_loads);
  } else {
    DCHECK(!abandoned_page);  // How can the doc have finished but not the page?
    if (abandoned_page)
      return;  // Don't try to record a stat which is broken.
  }

  has_dumped_ = true;

  // Note: Client side redirects will have no request time.
  Time begin = request.is_null() ? start : request;
  TimeDelta begin_to_finish_doc = finish_doc - begin;
  TimeDelta begin_to_finish_all_loads = finish_all_loads - begin;
  TimeDelta start_to_finish_all_loads = finish_all_loads - start;
  TimeDelta start_to_commit = commit - start;

  NavigationState::LoadType load_type = navigation_state->load_type();

  // The above code sanitized all values of times, in preparation for creating
  // actual histograms.  The remainder of this code could be run at destructor
  // time for the navigation_state, since all data is intact.

  // Aggregate PLT data across all link types.
  UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned", abandoned_page ? 1 : 0, 2);
  UMA_HISTOGRAM_ENUMERATION("PLT.LoadType", load_type,
      NavigationState::kLoadTypeMax);
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
  if (!first_paint.is_null()) {
    DCHECK(begin <= first_paint);
    PLT_HISTOGRAM("PLT.BeginToFirstPaint", first_paint - begin);
    DCHECK(commit <= first_paint);
    PLT_HISTOGRAM("PLT.CommitToFirstPaint", first_paint - commit);
  }
  if (!first_paint_after_load.is_null()) {
    DCHECK(begin <= first_paint_after_load);
    PLT_HISTOGRAM("PLT.BeginToFirstPaintAfterLoad",
      first_paint_after_load - begin);
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
    case NavigationState::UNDEFINED_LOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_UndefLoad", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_UndefLoad", begin_to_finish_all_loads);
      break;
    case NavigationState::RELOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_Reload", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_Reload", begin_to_finish_all_loads);
      break;
    case NavigationState::HISTORY_LOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_HistoryLoad", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_HistoryLoad", begin_to_finish_all_loads);
      break;
    case NavigationState::NORMAL_LOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_NormalLoad", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_NormalLoad", begin_to_finish_all_loads);
      break;
    case NavigationState::LINK_LOAD_NORMAL:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadNormal",
          begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadNormal",
                    begin_to_finish_all_loads);
      break;
    case NavigationState::LINK_LOAD_RELOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadReload",
           begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadReload",
                    begin_to_finish_all_loads);
      break;
    case NavigationState::LINK_LOAD_CACHE_STALE_OK:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadStaleOk",
           begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadStaleOk",
                    begin_to_finish_all_loads);
      break;
    case NavigationState::LINK_LOAD_CACHE_ONLY:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadCacheOnly",
           begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadCacheOnly",
                    begin_to_finish_all_loads);
      break;
    default:
      break;
  }

  // Histograms to determine if DNS prefetching has an impact on PLT.
  static bool use_dns_histogram(base::FieldTrialList::Find("DnsImpact") &&
      !base::FieldTrialList::Find("DnsImpact")->group_name().empty());
  if (use_dns_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("PLT.Abandoned", "DnsImpact"),
        abandoned_page ? 1 : 0, 2);
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("PLT.LoadType", "DnsImpact"),
        load_type, NavigationState::kLoadTypeMax);
    switch (load_type) {
      case NavigationState::NORMAL_LOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "DnsImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "DnsImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "DnsImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "DnsImpact"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Histograms to determine if content prefetching has an impact on PLT.
  static const bool prefetching_fieldtrial =
      base::FieldTrialList::Find("Prefetch") &&
      !base::FieldTrialList::Find("Prefetch")->group_name().empty();
  if (prefetching_fieldtrial) {
    if (navigation_state->was_prefetcher()) {
      PLT_HISTOGRAM(base::FieldTrial::MakeName(
          "PLT.BeginToFinishDoc_ContentPrefetcher", "Prefetch"),
          begin_to_finish_doc);
      PLT_HISTOGRAM(base::FieldTrial::MakeName(
          "PLT.BeginToFinish_ContentPrefetcher", "Prefetch"),
          begin_to_finish_all_loads);
    }
    if (navigation_state->was_referred_by_prefetcher()) {
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

  // Histograms to determine if backup connection jobs have an impact on PLT.
  static const bool connect_backup_jobs_fieldtrial(
      base::FieldTrialList::Find("ConnnectBackupJobs") &&
      !base::FieldTrialList::Find("ConnnectBackupJobs")->group_name().empty());
  if (connect_backup_jobs_fieldtrial) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("PLT.Abandoned", "ConnnectBackupJobs"),
        abandoned_page ? 1 : 0, 2);
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("PLT.LoadType", "ConnnectBackupJobs"),
        load_type, NavigationState::kLoadTypeMax);
    switch (load_type) {
      case NavigationState::NORMAL_LOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "ConnnectBackupJobs"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "ConnnectBackupJobs"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "ConnnectBackupJobs"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "ConnnectBackupJobs"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Histograms to determine if the number of connections has an
  // impact on PLT.
  // TODO(jar): Consider removing the per-link-type versions.  We
  //   really only need LINK_LOAD_NORMAL and NORMAL_LOAD.
  static bool use_connection_impact_histogram(
      base::FieldTrialList::Find("ConnCountImpact") &&
      !base::FieldTrialList::Find("ConnCountImpact")->group_name().empty());
  if (use_connection_impact_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("PLT.Abandoned", "ConnCountImpact"),
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case NavigationState::NORMAL_LOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "ConnCountImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "ConnCountImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "ConnCountImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "ConnCountImpact"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Histograms to determine effect of idle socket timeout.
  static bool use_idle_socket_timeout_histogram(
      base::FieldTrialList::Find("IdleSktToImpact") &&
      !base::FieldTrialList::Find("IdleSktToImpact")->group_name().empty());
  if (use_idle_socket_timeout_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("PLT.Abandoned", "IdleSktToImpact"),
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case NavigationState::NORMAL_LOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "IdleSktToImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "IdleSktToImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "IdleSktToImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "IdleSktToImpact"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Histograms to determine effect of number of connections per proxy.
  static bool use_proxy_connection_impact_histogram(
      base::FieldTrialList::Find("ProxyConnectionImpact") &&
      !base::FieldTrialList::Find(
          "ProxyConnectionImpact")->group_name().empty());
  if (use_proxy_connection_impact_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("PLT.Abandoned", "ProxyConnectionImpact"),
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case NavigationState::NORMAL_LOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "ProxyConnectionImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "ProxyConnectionImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "ProxyConnectionImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "ProxyConnectionImpact"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Histograms to determine if SDCH has an impact.
  // TODO(jar): Consider removing per-link load types and the enumeration.
  static bool use_sdch_histogram(base::FieldTrialList::Find("GlobalSdch") &&
      !base::FieldTrialList::Find("GlobalSdch")->group_name().empty());
  if (use_sdch_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("PLT.LoadType", "GlobalSdch"),
        load_type, NavigationState::kLoadTypeMax);
    switch (load_type) {
      case NavigationState::NORMAL_LOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_ONLY:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadCacheOnly", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Histograms to determine if cache size has an impact on PLT.
  static bool use_cache_histogram1(base::FieldTrialList::Find("CacheSize") &&
      !base::FieldTrialList::Find("CacheSize")->group_name().empty());
  if (use_cache_histogram1 && NavigationState::LINK_LOAD_NORMAL <= load_type &&
      NavigationState::LINK_LOAD_CACHE_ONLY >= load_type) {
    // TODO(mbelshe): Do we really want BeginToFinishDoc here?  It seems like
    //                StartToFinish or BeginToFinish would be better.
    PLT_HISTOGRAM(base::FieldTrial::MakeName(
        "PLT.BeginToFinishDoc_LinkLoad", "CacheSize"), begin_to_finish_doc);
  }

  // Histograms to determine if cache throttling has an impact on PLT.
  static bool use_cache_histogram2(
      base::FieldTrialList::Find("CacheThrottle") &&
      !base::FieldTrialList::Find("CacheThrottle")->group_name().empty());
  if (use_cache_histogram2) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("PLT.Abandoned", "CacheThrottle"),
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case NavigationState::RELOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_Reload", "CacheThrottle"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::HISTORY_LOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_HistoryLoad", "CacheThrottle"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::NORMAL_LOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "CacheThrottle"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "CacheThrottle"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "CacheThrottle"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "CacheThrottle"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_ONLY:
        PLT_HISTOGRAM(base::FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadCacheOnly", "CacheThrottle"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
    if (NavigationState::RELOAD <= load_type &&
        NavigationState::LINK_LOAD_CACHE_ONLY >= load_type) {
      PLT_HISTOGRAM(base::FieldTrial::MakeName(
          "PLT.BeginToFinish", "CacheThrottle"),
           begin_to_finish_all_loads);
    }
  }

  // For the SPDY field trials, we need to verify that the page loaded was
  // the type we requested:
  //   if we asked for a SPDY request, we got a SPDY request
  //   if we asked for a HTTP request, we got a HTTP request
  // Due to spdy version mismatches, it is possible that we ask for SPDY
  // but didn't get SPDY.
  static bool use_spdy_histogram(base::FieldTrialList::Find("SpdyImpact") &&
      !base::FieldTrialList::Find("SpdyImpact")->group_name().empty());
  if (use_spdy_histogram) {
    // We take extra effort to only compute these once.
    static bool in_spdy_trial = base::FieldTrialList::Find(
        "SpdyImpact")->group_name() == "npn_with_spdy";
    static bool in_http_trial = base::FieldTrialList::Find(
        "SpdyImpact")->group_name() == "npn_with_http";

    bool spdy_trial_success = navigation_state->was_fetched_via_spdy() ?
        in_spdy_trial : in_http_trial;
    if (spdy_trial_success) {
      // Histograms to determine if SPDY has an impact for https traffic.
      // TODO(mbelshe): After we've seen the difference between BeginToFinish
      //                and StartToFinish, consider removing one or the other.
      if (scheme_type == URLPattern::SCHEME_HTTPS &&
          navigation_state->was_npn_negotiated()) {
        UMA_HISTOGRAM_ENUMERATION(
            base::FieldTrial::MakeName("PLT.Abandoned", "SpdyImpact"),
            abandoned_page ? 1 : 0, 2);
        switch (load_type) {
          case NavigationState::LINK_LOAD_NORMAL:
            PLT_HISTOGRAM(base::FieldTrial::MakeName(
                "PLT.BeginToFinish_LinkLoadNormal_SpdyTrial", "SpdyImpact"),
                begin_to_finish_all_loads);
            PLT_HISTOGRAM(base::FieldTrial::MakeName(
                "PLT.StartToFinish_LinkLoadNormal_SpdyTrial", "SpdyImpact"),
                start_to_finish_all_loads);
            PLT_HISTOGRAM(base::FieldTrial::MakeName(
                "PLT.StartToCommit_LinkLoadNormal_SpdyTrial", "SpdyImpact"),
                start_to_commit);
            break;
          case NavigationState::NORMAL_LOAD:
            PLT_HISTOGRAM(base::FieldTrial::MakeName(
                "PLT.BeginToFinish_NormalLoad_SpdyTrial", "SpdyImpact"),
                begin_to_finish_all_loads);
            PLT_HISTOGRAM(base::FieldTrial::MakeName(
                "PLT.StartToFinish_NormalLoad_SpdyTrial", "SpdyImpact"),
                start_to_finish_all_loads);
            PLT_HISTOGRAM(base::FieldTrial::MakeName(
                "PLT.StartToCommit_NormalLoad_SpdyTrial", "SpdyImpact"),
                start_to_commit);
            break;
          default:
            break;
        }
      }

      // Histograms to compare the impact of alternate protocol over http
      // traffic: when spdy is used vs. when http is used.
      if (scheme_type == URLPattern::SCHEME_HTTP &&
          navigation_state->was_alternate_protocol_available()) {
        if (!navigation_state->was_npn_negotiated()) {
          // This means that even there is alternate protocols for npn_http or
          // npn_spdy, they are not taken (due to the base::FieldTrial).
          switch (load_type) {
            case NavigationState::LINK_LOAD_NORMAL:
              PLT_HISTOGRAM(
                  "PLT.StartToFinish_LinkLoadNormal_AlternateProtocol_http",
                  start_to_finish_all_loads);
              PLT_HISTOGRAM(
                  "PLT.StartToCommit_LinkLoadNormal_AlternateProtocol_http",
                  start_to_commit);
              break;
            case NavigationState::NORMAL_LOAD:
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
        } else if (navigation_state->was_fetched_via_spdy()) {
          switch (load_type) {
            case NavigationState::LINK_LOAD_NORMAL:
              PLT_HISTOGRAM(
                  "PLT.StartToFinish_LinkLoadNormal_AlternateProtocol_spdy",
                  start_to_finish_all_loads);
              PLT_HISTOGRAM(
                  "PLT.StartToCommit_LinkLoadNormal_AlternateProtocol_spdy",
                  start_to_commit);
              break;
            case NavigationState::NORMAL_LOAD:
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

  // Record page load time and abandonment rates for proxy cases.
  if (navigation_state->was_fetched_via_proxy()) {
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

  // Site isolation metrics.
  UMA_HISTOGRAM_COUNTS("SiteIsolation.PageLoadsWithCrossSiteFrameAccess",
                       cross_origin_access_count_);
  UMA_HISTOGRAM_COUNTS("SiteIsolation.PageLoadsWithSameSiteFrameAccess",
                       same_origin_access_count_);

  // Log the PLT to the info log.
  LogPageLoadTime(navigation_state, frame->dataSource());

  // Since there are currently no guarantees that renderer histograms will be
  // sent to the browser, we initiate a PostTask here to be sure that we send
  // the histograms we generated.  Without this call, pages that don't have an
  // on-close-handler might generate data that is lost when the renderer is
  // shutdown abruptly (perchance because the user closed the tab).
  // TODO(jar) BUG=33233: This needs to be moved to a PostDelayedTask, and it
  // should post when the onload is complete, so that it doesn't interfere with
  // the next load.
  if (RenderThread::current()) {
    RenderThread::current()->SendHistograms(
        chrome::kHistogramSynchronizerReservedSequenceNumber);
  }
}

void PageLoadHistograms::IncrementCrossFramePropertyAccess(bool cross_origin) {
  if (cross_origin)
    cross_origin_access_count_++;
  else
    same_origin_access_count_++;
}

void PageLoadHistograms::ResetCrossFramePropertyAccess() {
  cross_origin_access_count_ = 0;
  same_origin_access_count_ = 0;
}

void PageLoadHistograms::LogPageLoadTime(const NavigationState* state,
                                         const WebDataSource* ds) const {
  // Because this function gets called on every page load,
  // take extra care to optimize it away if logging is turned off.
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;

  DCHECK(state);
  DCHECK(ds);
  GURL url(ds->request().url());
  Time start = state->start_load_time();
  Time finish = state->finish_load_time();
  // TODO(mbelshe): should we log more stats?
  VLOG(1) << "PLT: " << (finish - start).InMilliseconds() << "ms "
          << url.spec();
}
