// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/cache_stats.h"

#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceRequestInfo;

#if defined(COMPILER_GCC)

namespace BASE_HASH_NAMESPACE {
template <>
struct hash<const net::URLRequest*> {
  std::size_t operator()(const net::URLRequest* value) const {
    return reinterpret_cast<std::size_t>(value);
  }
};
}

#endif

namespace {

bool GetRenderView(const net::URLRequest& request,
                   int* process_id, int* route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(&request);
  if (!info)
    return false;
  return info->GetAssociatedRenderView(process_id, route_id);
}

void CallCacheStatsTabEventOnIOThread(
    std::pair<int, int> render_view_id,
    chrome_browser_net::CacheStats::TabEvent event,
    IOThread* io_thread) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (io_thread)
    io_thread->globals()->cache_stats->OnTabEvent(render_view_id, event);
}

// Times after a load has started at which stats are collected.
const int kStatsCollectionTimesMs[] = {
  500,
  1000,
  2000,
  3000,
  4000,
  5000,
  7500,
  10000,
  15000,
  20000
};

static int kTabLoadStatsAutoCleanupTimeoutSeconds = 30;

}  // namespace

namespace chrome_browser_net {

// Helper struct keeping stats about the page load progress & cache usage
// stats during the pageload so far for a given RenderView, identified
// by a pair of process id and route id.
struct CacheStats::TabLoadStats {
  TabLoadStats(std::pair<int, int> render_view_id, CacheStats* owner)
      : render_view_id(render_view_id),
        num_active(0),
        spinner_started(false),
        next_timer_index(0),
        timer(false, false) {
    // Initialize the timer to do an automatic cleanup.  If a pageload is
    // started for the TabLoadStats within that timeframe, CacheStats
    // will start using the timer, thereby cancelling the cleanup.
    // Once CacheStats starts the timer, the object is guaranteed to be
    // destroyed eventually, so there is no more need for automatic cleanup at
    // that point.
    timer.Start(FROM_HERE,
                base::TimeDelta::FromSeconds(
                    kTabLoadStatsAutoCleanupTimeoutSeconds),
                base::Bind(&CacheStats::RemoveTabLoadStats,
                           base::Unretained(owner),
                           render_view_id));
  }

  std::pair<int, int> render_view_id;
  int num_active;
  bool spinner_started;
  base::TimeTicks load_start_time;
  base::TimeTicks cache_start_time;
  base::TimeDelta cache_total_time;
  int next_timer_index;
  base::Timer timer;
  // URLRequest's for which there are outstanding cache transactions.
  base::hash_set<const net::URLRequest*> active_requests;
};

CacheStatsTabHelper::CacheStatsTabHelper(TabContents* tab)
    : content::WebContentsObserver(tab->web_contents()),
      cache_stats_(NULL) {
  is_otr_profile_ = tab->profile()->IsOffTheRecord();
}

CacheStatsTabHelper::~CacheStatsTabHelper() {
}

void CacheStatsTabHelper::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    content::RenderViewHost* render_view_host) {
  if (!is_main_frame)
    return;
  if (!validated_url.SchemeIs("http"))
    return;
  NotifyCacheStats(CacheStats::SPINNER_START, render_view_host);
}

void CacheStatsTabHelper::DidStopLoading(RenderViewHost* render_view_host) {
  NotifyCacheStats(CacheStats::SPINNER_STOP, render_view_host);
}

void CacheStatsTabHelper::NotifyCacheStats(
    CacheStats::TabEvent event,
    RenderViewHost* render_view_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (is_otr_profile_)
    return;
  int process_id = render_view_host->GetProcess()->GetID();
  int route_id = render_view_host->GetRoutingID();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CallCacheStatsTabEventOnIOThread,
                 std::pair<int, int>(process_id, route_id),
                 event,
                 base::Unretained(g_browser_process->io_thread())));
}

CacheStats::CacheStats() {
  for (int i = 0;
       i < static_cast<int>(arraysize(kStatsCollectionTimesMs));
       i++) {
    final_histograms_.push_back(
        base::LinearHistogram::FactoryGet(
            "CacheStats.FractionCacheUseFinalPLT_" +
            base::IntToString(kStatsCollectionTimesMs[i]),
            0, 101, 102, base::Histogram::kUmaTargetedHistogramFlag));
    intermediate_histograms_.push_back(
        base::LinearHistogram::FactoryGet(
            "CacheStats.FractionCacheUseIntermediatePLT_" +
            base::IntToString(kStatsCollectionTimesMs[i]),
            0, 101, 102, base::Histogram::kUmaTargetedHistogramFlag));
  }
  DCHECK_EQ(final_histograms_.size(), arraysize(kStatsCollectionTimesMs));
  DCHECK_EQ(intermediate_histograms_.size(),
            arraysize(kStatsCollectionTimesMs));
}

CacheStats::~CacheStats() {
  STLDeleteValues(&tab_load_stats_);
}

CacheStats::TabLoadStats* CacheStats::GetTabLoadStats(
    std::pair<int, int> render_view_id) {
  TabLoadStatsMap::const_iterator it = tab_load_stats_.find(render_view_id);
  if (it != tab_load_stats_.end())
    return it->second;
  TabLoadStats* new_tab_load_stats = new TabLoadStats(render_view_id, this);
  tab_load_stats_[render_view_id] = new_tab_load_stats;
  return new_tab_load_stats;
}

void CacheStats::RemoveTabLoadStats(std::pair<int, int> render_view_id) {
  TabLoadStatsMap::iterator it = tab_load_stats_.find(render_view_id);
  if (it != tab_load_stats_.end()) {
    delete it->second;
    tab_load_stats_.erase(it);
  }
}

void CacheStats::OnCacheWaitStateChange(
    const net::URLRequest& request,
    net::NetworkDelegate::CacheWaitState state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (main_request_contexts_.count(request.context()) < 1)
    return;
  int process_id, route_id;
  if (!GetRenderView(request, &process_id, &route_id))
    return;
  TabLoadStats* stats =
      GetTabLoadStats(std::pair<int, int>(process_id, route_id));
  bool newly_started = false;
  bool newly_finished = false;
  switch (state) {
    case net::NetworkDelegate::CACHE_WAIT_STATE_START:
      DCHECK(stats->active_requests.count(&request) == 0);
      newly_started = true;
      stats->active_requests.insert(&request);
      break;
    case net::NetworkDelegate::CACHE_WAIT_STATE_FINISH:
      if (stats->active_requests.count(&request) > 0) {
        stats->active_requests.erase(&request);
        newly_finished = true;
      }
      break;
    case net::NetworkDelegate::CACHE_WAIT_STATE_RESET:
      if (stats->active_requests.count(&request) > 0) {
        stats->active_requests.erase(&request);
        newly_finished = true;
      }
      break;
  }
  DCHECK_GE(stats->num_active, 0);
  if (newly_started) {
    DCHECK(!newly_finished);
    if (stats->num_active == 0) {
      stats->cache_start_time = base::TimeTicks::Now();
    }
    stats->num_active++;
  }
  if (newly_finished) {
    DCHECK(!newly_started);
    if (stats->num_active == 1) {
      stats->cache_total_time +=
          base::TimeTicks::Now() - stats->cache_start_time;
    }
    stats->num_active--;
  }
  DCHECK_GE(stats->num_active, 0);
}

void CacheStats::OnTabEvent(std::pair<int, int> render_view_id,
                            TabEvent event) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  TabLoadStats* stats = GetTabLoadStats(render_view_id);
  if (event == SPINNER_START) {
    stats->spinner_started = true;
    stats->cache_total_time = base::TimeDelta();
    stats->cache_start_time = base::TimeTicks::Now();
    stats->load_start_time = base::TimeTicks::Now();
    stats->next_timer_index = 0;
    ScheduleTimer(stats);
  } else {
    DCHECK_EQ(event, SPINNER_STOP);
    if (stats->spinner_started) {
      stats->spinner_started = false;
      base::TimeDelta load_time =
          base::TimeTicks::Now() - stats->load_start_time;
      if (stats->num_active > 1)
        stats->cache_total_time +=
            base::TimeTicks::Now() - stats->cache_start_time;
      RecordCacheFractionHistogram(load_time, stats->cache_total_time, true,
                                   stats->next_timer_index);
    }
    RemoveTabLoadStats(render_view_id);
  }
}

void CacheStats::ScheduleTimer(TabLoadStats* stats) {
  int timer_index = stats->next_timer_index;
  DCHECK(timer_index >= 0 &&
         timer_index < static_cast<int>(arraysize(kStatsCollectionTimesMs)));
  base::TimeDelta delta =
      base::TimeDelta::FromMilliseconds(kStatsCollectionTimesMs[timer_index]);
  delta -= base::TimeTicks::Now() - stats->load_start_time;

  // If the ScheduleTimer call was delayed significantly, like when one's using
  // a debugger, don't try to start the timer with a negative time.
  if (delta < base::TimeDelta()) {
    RemoveTabLoadStats(stats->render_view_id);
    return;
  }

  stats->timer.Start(FROM_HERE,
                     delta,
                     base::Bind(&CacheStats::TimerCallback,
                                base::Unretained(this),
                                base::Unretained(stats)));
}

void CacheStats::TimerCallback(TabLoadStats* stats) {
  DCHECK(stats->spinner_started);
  base::TimeDelta load_time = base::TimeTicks::Now() - stats->load_start_time;
  base::TimeDelta cache_time = stats->cache_total_time;
  if (stats->num_active > 1)
    cache_time += base::TimeTicks::Now() - stats->cache_start_time;
  RecordCacheFractionHistogram(load_time, cache_time, false,
                               stats->next_timer_index);
  stats->next_timer_index++;
  if (stats->next_timer_index <
      static_cast<int>(arraysize(kStatsCollectionTimesMs))) {
    ScheduleTimer(stats);
  } else {
    RemoveTabLoadStats(stats->render_view_id);
  }
}

void CacheStats::RecordCacheFractionHistogram(base::TimeDelta elapsed,
                                              base::TimeDelta cache_time,
                                              bool is_load_done,
                                              int timer_index) {
  DCHECK(timer_index >= 0 &&
         timer_index < static_cast<int>(arraysize(kStatsCollectionTimesMs)));

  if (elapsed.InMilliseconds() <= 0)
    return;

  int64 cache_fraction_percentage =
      100 * cache_time.InMilliseconds() / elapsed.InMilliseconds();

  DCHECK(cache_fraction_percentage >= 0 && cache_fraction_percentage <= 100);

  if (is_load_done) {
    final_histograms_[timer_index]->Add(cache_fraction_percentage);
  } else {
    intermediate_histograms_[timer_index]->Add(cache_fraction_percentage);
  }
}

void CacheStats::RegisterURLRequestContext(
    const net::URLRequestContext* context,
    ChromeURLRequestContext::ContextType type) {
  if (type == ChromeURLRequestContext::CONTEXT_TYPE_MAIN)
    main_request_contexts_.insert(context);
}

void CacheStats::UnregisterURLRequestContext(
    const net::URLRequestContext* context) {
  main_request_contexts_.erase(context);
}

}  // namespace chrome_browser_net
