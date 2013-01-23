// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_LOAD_TIME_STATS_H_
#define CHROME_BROWSER_NET_LOAD_TIME_STATS_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/network_delegate.h"

namespace base {
class HistogramBase;
}

namespace net {
class URLRequest;
class URLRequestContext;
}

#if defined(COMPILER_GCC)

namespace BASE_HASH_NAMESPACE {
template <>
struct hash<const net::URLRequest*> {
  std::size_t operator()(const net::URLRequest* value) const {
    return reinterpret_cast<std::size_t>(value);
  }
};
template <>
struct hash<const net::URLRequestContext*> {
  std::size_t operator()(const net::URLRequestContext* value) const {
    return reinterpret_cast<std::size_t>(value);
  }
};
}

#endif

namespace chrome_browser_net {

// This class collects UMA stats about cache performance.
class LoadTimeStats {
 public:
  enum TabEvent {
    SPINNER_START,
    SPINNER_STOP
  };
  enum RequestStatus {
    REQUEST_STATUS_CACHE_WAIT,
    REQUEST_STATUS_NETWORK_WAIT,
    REQUEST_STATUS_ACTIVE,
    REQUEST_STATUS_NONE,
    REQUEST_STATUS_MAX
  };
  enum HistogramType {
    HISTOGRAM_FINAL_AGGREGATE,
    HISTOGRAM_FINAL_CUMULATIVE_PERCENTAGE,
    HISTOGRAM_INTERMEDIATE_AGGREGATE,
    HISTOGRAM_INTERMEDIATE_CUMULATIVE_PERCENTAGE,
    HISTOGRAM_MAX
  };

  LoadTimeStats();
  ~LoadTimeStats();

  void OnRequestWaitStateChange(const net::URLRequest& request,
                                net::NetworkDelegate::RequestWaitState state);
  void OnURLRequestDestroyed(const net::URLRequest& request);
  void OnTabEvent(std::pair<int, int> render_view_id, TabEvent event);
  void RegisterURLRequestContext(const net::URLRequestContext* context,
                                 ChromeURLRequestContext::ContextType type);
  void UnregisterURLRequestContext(const net::URLRequestContext* context);

 private:
  class TabLoadStats;
  // A map mapping a renderer's process id and route id to a TabLoadStats,
  // representing that renderer's load statistics.
  typedef std::map<std::pair<int, int>, TabLoadStats*> TabLoadStatsMap;

  class URLRequestStats;
  typedef base::hash_map<const net::URLRequest*,
                         URLRequestStats*> RequestStatsMap;

  // Gets RequestStats for a given request.
  URLRequestStats* GetRequestStats(const net::URLRequest* request);
  // Gets TabLoadStats for a given RenderView.
  TabLoadStats* GetTabLoadStats(std::pair<int, int> render_view_id);
  // Deletes TabLoadStats no longer needed for a render view.
  void RemoveTabLoadStats(std::pair<int, int> render_view_id);
  // Sets a timer for a given tab to collect stats.  |timer_index| indicates
  // how many times stats have been collected since the navigation has started
  // for this tab.
  void ScheduleTimer(TabLoadStats* stats);
  // The callback when a timer fires to collect stats again.
  void TimerCallback(TabLoadStats* stats);
  // Helper function to put the current set of statistics into UMA histograms.
  void RecordHistograms(base::TimeDelta elapsed,
                        TabLoadStats* stats,
                        bool is_load_done);

  TabLoadStatsMap tab_load_stats_;
  RequestStatsMap request_stats_;
  std::vector<base::HistogramBase*>
      histograms_[REQUEST_STATUS_MAX][HISTOGRAM_MAX];
  base::hash_set<const net::URLRequestContext*> main_request_contexts_;

  DISALLOW_COPY_AND_ASSIGN(LoadTimeStats);
};

// A WebContentsObserver watching a tab, notifying LoadTimeStats whenever the
// spinner starts or stops for it, and whenever a renderer is no longer used.
class LoadTimeStatsTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<LoadTimeStatsTabHelper> {
 public:
  virtual ~LoadTimeStatsTabHelper();

  // content::WebContentsObserver implementation
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      int64 parent_frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;

 private:
  explicit LoadTimeStatsTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<LoadTimeStatsTabHelper>;

  // Calls into LoadTimeStats to notify that a reportable event has occurred
  // for the tab being observed.
  void NotifyLoadTimeStats(LoadTimeStats::TabEvent event,
                           content::RenderViewHost* render_view_host);

  bool is_otr_profile_;

  DISALLOW_COPY_AND_ASSIGN(LoadTimeStatsTabHelper);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_LOAD_TIME_STATS_H_
