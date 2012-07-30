// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CACHE_STATS_H_
#define CHROME_BROWSER_NET_CACHE_STATS_H_

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
#include "net/base/network_delegate.h"

class TabContents;

namespace base {
class Histogram;
}

namespace net {
class URLRequest;
class URLRequestContext;
}

#if defined(COMPILER_GCC)

namespace BASE_HASH_NAMESPACE {
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
class CacheStats {
 public:
  enum TabEvent {
    SPINNER_START,
    SPINNER_STOP
  };
  CacheStats();
  ~CacheStats();

  void OnCacheWaitStateChange(const net::URLRequest& request,
                              net::NetworkDelegate::CacheWaitState state);
  void OnTabEvent(std::pair<int, int> render_view_id, TabEvent event);
  void RegisterURLRequestContext(const net::URLRequestContext* context,
                                 ChromeURLRequestContext::ContextType type);
  void UnregisterURLRequestContext(const net::URLRequestContext* context);

 private:
  struct TabLoadStats;
  // A map mapping a renderer's process id and route id to a TabLoadStats,
  // representing that renderer's load statistics.
  typedef std::map<std::pair<int, int>, TabLoadStats*> TabLoadStatsMap;

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
  // Helper function to put the current set of cache statistics into an UMA
  // histogram.
  void RecordCacheFractionHistogram(base::TimeDelta elapsed,
                                    base::TimeDelta cache_time,
                                    bool is_load_done,
                                    int timer_index);

  TabLoadStatsMap tab_load_stats_;
  std::vector<base::Histogram*> final_histograms_;
  std::vector<base::Histogram*> intermediate_histograms_;
  base::hash_set<const net::URLRequestContext*> main_request_contexts_;

  DISALLOW_COPY_AND_ASSIGN(CacheStats);
};

// A WebContentsObserver watching all tabs, notifying CacheStats
// whenever the spinner starts or stops for a given tab, and when a renderer
// is no longer used.
class CacheStatsTabHelper : public content::WebContentsObserver {
 public:
  explicit CacheStatsTabHelper(TabContents* tab);
  virtual ~CacheStatsTabHelper();

  // content::WebContentsObserver implementation
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;

 private:
  // Calls into CacheStats to notify that a reportable event has occurred
  // for the tab being observed.
  void NotifyCacheStats(CacheStats::TabEvent event,
                        content::RenderViewHost* render_view_host);

  CacheStats* cache_stats_;
  bool is_otr_profile_;

  DISALLOW_COPY_AND_ASSIGN(CacheStatsTabHelper);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_CACHE_STATS_H_
