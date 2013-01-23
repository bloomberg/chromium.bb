// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/load_time_stats.h"

#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceRequestInfo;
using std::string;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(chrome_browser_net::LoadTimeStatsTabHelper);

namespace {

bool GetRenderView(const net::URLRequest& request,
                   int* process_id, int* route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(&request);
  if (!info)
    return false;
  return info->GetAssociatedRenderView(process_id, route_id);
}

void CallLoadTimeStatsTabEventOnIOThread(
    std::pair<int, int> render_view_id,
    chrome_browser_net::LoadTimeStats::TabEvent event,
    IOThread* io_thread) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (io_thread)
    io_thread->globals()->load_time_stats->OnTabEvent(render_view_id, event);
}

// Times after a load has started at which stats are collected.
const int kStatsCollectionTimesMs[] = {
  1000,
  2000,
  4000,
  20000
};

static int kTabLoadStatsAutoCleanupTimeoutSeconds = 30;

const char* kRequestStatusNames[] = {
  "CacheWait",
  "NetworkWait",
  "Active",
  "None",
  "Max"
};

COMPILE_ASSERT(arraysize(kRequestStatusNames) ==
               chrome_browser_net::LoadTimeStats::REQUEST_STATUS_MAX + 1,
               LoadTimeStats_RequestStatus_names_mismatch);

const char* kHistogramTypeNames[] = {
  "FinalAggregate",
  "FinalCumulativePercentage",
  "IntermediateAggregate",
  "IntermediateCumulativePercentage",
  "Max"
};

COMPILE_ASSERT(arraysize(kHistogramTypeNames) ==
               chrome_browser_net::LoadTimeStats::HISTOGRAM_MAX + 1,
               LoadTimeStats_HistogramType_names_mismatch);

}  // namespace

namespace chrome_browser_net {

// Helper struct keeping stats about the page load progress & cache usage
// stats during the pageload so far for a given RenderView, identified
// by a pair of process id and route id.
class LoadTimeStats::TabLoadStats {
 public:
  // Stores the time taken by all requests while they have a certain
  // RequestStatus.
  class PerStatusStats {
   public:
    PerStatusStats() : num_active_(0) {
    }

    void UpdateTotalTimes() {
      DCHECK_GE(num_active_, 0);
      base::TimeTicks now = base::TimeTicks::Now();
      if (num_active_ > 0) {
        total_time_ += now - last_update_time_;
        total_cumulative_time_ +=
            (now - last_update_time_) * static_cast<int64>(num_active_);
      }
      last_update_time_ = now;
    }

    void ResetTimes() {
      last_update_time_ = base::TimeTicks::Now();
      total_time_ = base::TimeDelta();
      total_cumulative_time_ = base::TimeDelta();
    }

    void IncrementNumActive() {
      num_active_++;
    }

    void DecrementNumActiveIfPositive() {
      if (num_active_ > 0)
        num_active_--;
    }

    int num_active() { return num_active_; }
    void set_num_active(int num_active) { num_active_ = num_active; }
    base::TimeDelta total_time() { return total_time_; }
    base::TimeDelta total_cumulative_time() { return total_cumulative_time_; }

   private:
    int num_active_;
    base::TimeTicks last_update_time_;
    base::TimeDelta total_time_;
    base::TimeDelta total_cumulative_time_;
  };

  TabLoadStats(std::pair<int, int> render_view_id, LoadTimeStats* owner)
      : render_view_id_(render_view_id),
        spinner_started_(false),
        next_timer_index_(0),
        timer_(false, false) {
    // Initialize the timer to do an automatic cleanup.  If a pageload is
    // started for the TabLoadStats within that timeframe, LoadTimeStats
    // will start using the timer, thereby cancelling the cleanup.
    // Once LoadTimeStats starts the timer, the object is guaranteed to be
    // destroyed eventually, so there is no more need for automatic cleanup at
    // that point.
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(
                     kTabLoadStatsAutoCleanupTimeoutSeconds),
                 base::Bind(&LoadTimeStats::RemoveTabLoadStats,
                            base::Unretained(owner),
                            render_view_id_));
  }

  typedef std::pair<int, int> RenderViewId;
  typedef PerStatusStats PerStatusStatsArray[REQUEST_STATUS_MAX];
  typedef base::hash_map<const net::URLRequest*, RequestStatus> RequestMap;

  RenderViewId& render_view_id() { return render_view_id_; }
  PerStatusStatsArray& per_status_stats() { return per_status_stats_; }
  bool spinner_started() { return spinner_started_; }
  void set_spinner_started(bool value) { spinner_started_ = value; }
  base::TimeTicks load_start_time() { return load_start_time_; }
  void set_load_start_time(base::TimeTicks time) { load_start_time_ = time; }
  int next_timer_index() { return next_timer_index_; }
  void set_next_timer_index(int index) { next_timer_index_ = index; }
  base::Timer& timer() { return timer_; }
  RequestMap& active_requests() { return active_requests_; }

 private:
  RenderViewId render_view_id_;
  PerStatusStatsArray per_status_stats_;
  bool spinner_started_;
  base::TimeTicks load_start_time_;
  int next_timer_index_;
  base::Timer timer_;
  // Currently active URLRequests.
  RequestMap active_requests_;
};

class LoadTimeStats::URLRequestStats {
 public:
  URLRequestStats()
      : done_(false),
        start_time_(GetCurrentTime()),
        status_(REQUEST_STATUS_ACTIVE) {
  }
  void OnStatusChange(RequestStatus new_status) {
    if (done_)
      return;
    DCHECK_GE(status_, 0);
    DCHECK_GE(new_status, 0);
    DCHECK_LE(status_, REQUEST_STATUS_ACTIVE);
    DCHECK_LE(new_status, REQUEST_STATUS_ACTIVE);
    base::TimeTicks now = GetCurrentTime();
    base::TimeDelta elapsed = now - start_time_;
    status_times_[status_] += elapsed;
    start_time_ = now;
    status_ = new_status;
  }
  void RequestDone() {
    DCHECK(!done_);
    // Dummy status change to add last elapsed time.
    OnStatusChange(REQUEST_STATUS_ACTIVE);
    done_ = true;

    UMA_HISTOGRAM_TIMES("LoadTimeStats.Request_CacheWait_Time",
                        status_times_[REQUEST_STATUS_CACHE_WAIT]);
    UMA_HISTOGRAM_TIMES("LoadTimeStats.Request_NetworkWait_Time",
                        status_times_[REQUEST_STATUS_NETWORK_WAIT]);
    UMA_HISTOGRAM_TIMES("LoadTimeStats.Request_Active_Time",
                        status_times_[REQUEST_STATUS_ACTIVE]);

    base::TimeDelta total_time;
    for (int status = REQUEST_STATUS_CACHE_WAIT;
         status <= REQUEST_STATUS_ACTIVE;
         status++) {
      total_time += status_times_[status];
    }
    if (total_time.InMilliseconds() <= 0)
      return;

    for (int status = REQUEST_STATUS_CACHE_WAIT;
         status <= REQUEST_STATUS_ACTIVE;
         status++) {
      int64 fraction_percentage = 100 *
          status_times_[status].InMilliseconds() /
          total_time.InMilliseconds();
      DCHECK(fraction_percentage >= 0 && fraction_percentage <= 100);
      switch (status) {
        case REQUEST_STATUS_CACHE_WAIT:
          UMA_HISTOGRAM_PERCENTAGE(
              "LoadTimeStats.Request_CacheWait_Percentage",
              fraction_percentage);
          break;
        case REQUEST_STATUS_NETWORK_WAIT:
          UMA_HISTOGRAM_PERCENTAGE(
              "LoadTimeStats.Request_NetworkWait_Percentage",
              fraction_percentage);
          break;
        case REQUEST_STATUS_ACTIVE:
          UMA_HISTOGRAM_PERCENTAGE(
              "LoadTimeStats.Request_Active_Percentage", fraction_percentage);
          break;
      }
    }
  }

 private:
  base::TimeTicks GetCurrentTime() {
    return base::TimeTicks::Now();
  }
  bool done_;
  base::TimeTicks start_time_;
  RequestStatus status_;
  base::TimeDelta status_times_[REQUEST_STATUS_MAX];
};

LoadTimeStatsTabHelper::LoadTimeStatsTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  is_otr_profile_ = web_contents->GetBrowserContext()->IsOffTheRecord();
}

LoadTimeStatsTabHelper::~LoadTimeStatsTabHelper() {
}

void LoadTimeStatsTabHelper::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    int64 parent_frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc,
    content::RenderViewHost* render_view_host) {
  if (!is_main_frame)
    return;
  if (!validated_url.SchemeIs("http"))
    return;
  NotifyLoadTimeStats(LoadTimeStats::SPINNER_START, render_view_host);
}

void LoadTimeStatsTabHelper::DidStopLoading(RenderViewHost* render_view_host) {
  NotifyLoadTimeStats(LoadTimeStats::SPINNER_STOP, render_view_host);
}

void LoadTimeStatsTabHelper::NotifyLoadTimeStats(
    LoadTimeStats::TabEvent event,
    RenderViewHost* render_view_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (is_otr_profile_)
    return;
  int process_id = render_view_host->GetProcess()->GetID();
  int route_id = render_view_host->GetRoutingID();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CallLoadTimeStatsTabEventOnIOThread,
                 std::pair<int, int>(process_id, route_id),
                 event,
                 base::Unretained(g_browser_process->io_thread())));
}

LoadTimeStats::LoadTimeStats() {
  for (int status = REQUEST_STATUS_CACHE_WAIT;
       status <= REQUEST_STATUS_ACTIVE;
       status++) {
    for (int histogram_type = HISTOGRAM_FINAL_AGGREGATE;
         histogram_type < HISTOGRAM_MAX;
         histogram_type++) {
      for (int i = 0;
           i < static_cast<int>(arraysize(kStatsCollectionTimesMs));
           i++) {
        string histogram_name = string("LoadTimeStats.Fraction_") +
            string(kRequestStatusNames[status]) + string("_") +
            string(kHistogramTypeNames[histogram_type]) + string("_") +
            base::IntToString(kStatsCollectionTimesMs[i]);
        histograms_[status][histogram_type].push_back(
            base::LinearHistogram::FactoryGet(
                histogram_name,
                0, 101, 51, base::HistogramBase::kUmaTargetedHistogramFlag));
      }
      DCHECK_EQ(histograms_[status][histogram_type].size(),
                arraysize(kStatsCollectionTimesMs));
    }
  }
}

LoadTimeStats::~LoadTimeStats() {
  STLDeleteValues(&tab_load_stats_);
  STLDeleteValues(&request_stats_);
}

LoadTimeStats::URLRequestStats* LoadTimeStats::GetRequestStats(
    const net::URLRequest* request) {
  RequestStatsMap::const_iterator it = request_stats_.find(request);
  if (it != request_stats_.end())
    return it->second;
  URLRequestStats* new_request_stats = new URLRequestStats();
  request_stats_[request] = new_request_stats;
  return new_request_stats;
}

LoadTimeStats::TabLoadStats* LoadTimeStats::GetTabLoadStats(
    std::pair<int, int> render_view_id) {
  TabLoadStatsMap::const_iterator it = tab_load_stats_.find(render_view_id);
  if (it != tab_load_stats_.end())
    return it->second;
  TabLoadStats* new_tab_load_stats = new TabLoadStats(render_view_id, this);
  tab_load_stats_[render_view_id] = new_tab_load_stats;
  return new_tab_load_stats;
}

void LoadTimeStats::RemoveTabLoadStats(std::pair<int, int> render_view_id) {
  TabLoadStatsMap::iterator it = tab_load_stats_.find(render_view_id);
  if (it != tab_load_stats_.end()) {
    delete it->second;
    tab_load_stats_.erase(it);
  }
}

void LoadTimeStats::OnRequestWaitStateChange(
    const net::URLRequest& request,
    net::NetworkDelegate::RequestWaitState state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (main_request_contexts_.count(request.context()) < 1)
    return;
  int process_id, route_id;
  if (!GetRenderView(request, &process_id, &route_id))
    return;
  TabLoadStats* stats =
      GetTabLoadStats(std::pair<int, int>(process_id, route_id));
  RequestStatus old_status = REQUEST_STATUS_NONE;
  if (stats->active_requests().count(&request) > 0)
    old_status = stats->active_requests()[&request];
  RequestStatus new_status = REQUEST_STATUS_NONE;
  switch (state) {
    case net::NetworkDelegate::REQUEST_WAIT_STATE_CACHE_START:
      DCHECK(old_status == REQUEST_STATUS_NONE ||
             old_status == REQUEST_STATUS_ACTIVE);
      new_status = REQUEST_STATUS_CACHE_WAIT;
      break;
    case net::NetworkDelegate::REQUEST_WAIT_STATE_CACHE_FINISH:
      DCHECK(old_status == REQUEST_STATUS_NONE ||
             old_status == REQUEST_STATUS_CACHE_WAIT);
      new_status = REQUEST_STATUS_ACTIVE;
      break;
    case net::NetworkDelegate::REQUEST_WAIT_STATE_NETWORK_START:
      DCHECK(old_status == REQUEST_STATUS_NONE ||
             old_status == REQUEST_STATUS_ACTIVE);
      new_status = REQUEST_STATUS_NETWORK_WAIT;
      break;
    case net::NetworkDelegate::REQUEST_WAIT_STATE_NETWORK_FINISH:
      DCHECK(old_status == REQUEST_STATUS_NONE ||
             old_status == REQUEST_STATUS_NETWORK_WAIT);
      new_status = REQUEST_STATUS_ACTIVE;
      break;
    case net::NetworkDelegate::REQUEST_WAIT_STATE_RESET:
      new_status = REQUEST_STATUS_NONE;
      break;
  }
  if (old_status == new_status)
    return;
  if (old_status != REQUEST_STATUS_NONE) {
    TabLoadStats::PerStatusStats* status_stats =
        &(stats->per_status_stats()[old_status]);
    DCHECK_GE(status_stats->num_active(), 0);
    status_stats->UpdateTotalTimes();
    status_stats->DecrementNumActiveIfPositive();
    DCHECK_GE(status_stats->num_active(), 0);
  }
  if (new_status != REQUEST_STATUS_NONE) {
    TabLoadStats::PerStatusStats* status_stats =
        &(stats->per_status_stats()[new_status]);
    DCHECK_GE(status_stats->num_active(), 0);
    status_stats->UpdateTotalTimes();
    status_stats->IncrementNumActive();
    DCHECK_GE(status_stats->num_active(), 0);
    stats->active_requests()[&request] = new_status;
    URLRequestStats* request_stats = GetRequestStats(&request);
    request_stats->OnStatusChange(new_status);
  } else {
    stats->active_requests().erase(&request);
  }
}

void LoadTimeStats::OnURLRequestDestroyed(const net::URLRequest& request) {
  if (request_stats_.count(&request) < 1)
    return;
  scoped_ptr<URLRequestStats> request_stats(GetRequestStats(&request));
  request_stats_.erase(&request);
  request_stats->RequestDone();
}

void LoadTimeStats::OnTabEvent(std::pair<int, int> render_view_id,
                            TabEvent event) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  TabLoadStats* stats = GetTabLoadStats(render_view_id);
  if (event == SPINNER_START) {
    stats->set_spinner_started(true);
    stats->set_load_start_time(base::TimeTicks::Now());
    for (int status = REQUEST_STATUS_CACHE_WAIT;
         status <= REQUEST_STATUS_ACTIVE; status++) {
      stats->per_status_stats()[status].ResetTimes();
    }
    stats->set_next_timer_index(0);
    ScheduleTimer(stats);
  } else {
    DCHECK_EQ(event, SPINNER_STOP);
    if (stats->spinner_started()) {
      stats->set_spinner_started(false);
      base::TimeDelta load_time =
          base::TimeTicks::Now() - stats->load_start_time();
      RecordHistograms(load_time, stats, true);
    }
    RemoveTabLoadStats(render_view_id);
  }
}

void LoadTimeStats::ScheduleTimer(TabLoadStats* stats) {
  int timer_index = stats->next_timer_index();
  DCHECK(timer_index >= 0 &&
         timer_index < static_cast<int>(arraysize(kStatsCollectionTimesMs)));
  base::TimeDelta delta =
      base::TimeDelta::FromMilliseconds(kStatsCollectionTimesMs[timer_index]);
  delta -= base::TimeTicks::Now() - stats->load_start_time();

  // If the ScheduleTimer call was delayed significantly, like when one's using
  // a debugger, don't try to start the timer with a negative time.
  if (delta < base::TimeDelta()) {
    RemoveTabLoadStats(stats->render_view_id());
    return;
  }

  stats->timer().Start(FROM_HERE,
                       delta,
                       base::Bind(&LoadTimeStats::TimerCallback,
                                  base::Unretained(this),
                                  base::Unretained(stats)));
}

void LoadTimeStats::TimerCallback(TabLoadStats* stats) {
  DCHECK(stats->spinner_started());
  base::TimeDelta load_time = base::TimeTicks::Now() - stats->load_start_time();
  RecordHistograms(load_time, stats, false);
  stats->set_next_timer_index(stats->next_timer_index() + 1);
  if (stats->next_timer_index() <
      static_cast<int>(arraysize(kStatsCollectionTimesMs))) {
    ScheduleTimer(stats);
  } else {
    RemoveTabLoadStats(stats->render_view_id());
  }
}

void LoadTimeStats::RecordHistograms(base::TimeDelta elapsed,
                                  TabLoadStats* stats,
                                  bool is_load_done) {
  int timer_index = stats->next_timer_index();
  DCHECK(timer_index >= 0 &&
         timer_index < static_cast<int>(arraysize(kStatsCollectionTimesMs)));

  if (elapsed.InMilliseconds() <= 0)
    return;

  base::TimeDelta total_cumulative;
  for (int status = REQUEST_STATUS_CACHE_WAIT;
       status <= REQUEST_STATUS_ACTIVE;
       status++) {
    total_cumulative +=
        stats->per_status_stats()[status].total_cumulative_time();
  }

  for (int status = REQUEST_STATUS_CACHE_WAIT;
       status <= REQUEST_STATUS_ACTIVE;
       status++) {
    TabLoadStats::PerStatusStats* status_stats =
        &(stats->per_status_stats()[status]);

    int64 fraction_percentage = 100 *
        status_stats->total_time().InMilliseconds() / elapsed.InMilliseconds();
    DCHECK(fraction_percentage >= 0 && fraction_percentage <= 100);
    if (is_load_done) {
      histograms_[status][HISTOGRAM_FINAL_AGGREGATE][timer_index]->Add(
          fraction_percentage);
    } else {
      histograms_[status][HISTOGRAM_INTERMEDIATE_AGGREGATE][timer_index]->Add(
          fraction_percentage);
    }

    if (total_cumulative.InMilliseconds() > 0) {
      fraction_percentage = 100 *
          status_stats->total_cumulative_time().InMilliseconds() /
          total_cumulative.InMilliseconds();
      DCHECK(fraction_percentage >= 0 && fraction_percentage <= 100);
      if (is_load_done) {
        histograms_[status][HISTOGRAM_FINAL_CUMULATIVE_PERCENTAGE]
            [timer_index]->Add(fraction_percentage);
      } else {
        histograms_[status][HISTOGRAM_INTERMEDIATE_CUMULATIVE_PERCENTAGE]
            [timer_index]->Add(fraction_percentage);
      }
    }
  }
}

void LoadTimeStats::RegisterURLRequestContext(
    const net::URLRequestContext* context,
    ChromeURLRequestContext::ContextType type) {
  if (type == ChromeURLRequestContext::CONTEXT_TYPE_MAIN)
    main_request_contexts_.insert(context);
}

void LoadTimeStats::UnregisterURLRequestContext(
    const net::URLRequestContext* context) {
  main_request_contexts_.erase(context);
}

}  // namespace chrome_browser_net
