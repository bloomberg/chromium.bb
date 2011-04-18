// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#pragma once

#include <list>
#include <map>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "googleurl/src/gurl.h"

class Profile;
class TabContents;

#if defined(COMPILER_GCC)

namespace __gnu_cxx {
template <>
struct hash<TabContents*> {
  std::size_t operator()(TabContents* value) const {
    return reinterpret_cast<std::size_t>(value);
  }
};
}

#endif

namespace prerender {

// PrerenderManager is responsible for initiating and keeping prerendered
// views of webpages.
class PrerenderManager : public base::RefCountedThreadSafe<PrerenderManager> {
 public:
  // PrerenderManagerMode is used in a UMA_HISTOGRAM, so please do not
  // add in the middle.
  enum PrerenderManagerMode {
    PRERENDER_MODE_DISABLED,
    PRERENDER_MODE_ENABLED,
    PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP,
    PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP,
    PRERENDER_MODE_MAX
  };

  // Owned by a Profile object for the lifetime of the profile.
  explicit PrerenderManager(Profile* profile);

  // Preloads the URL supplied.  alias_urls indicates URLs that redirect
  // to the same URL to be preloaded. Returns true if the URL was added,
  // false if it was not.
  bool AddPreload(const GURL& url, const std::vector<GURL>& alias_urls,
                  const GURL& referrer);

  void AddPendingPreload(const std::pair<int, int>& child_route_id_pair,
                         const GURL& url,
                         const std::vector<GURL>& alias_urls,
                         const GURL& referrer);

  // For a given TabContents that wants to navigate to the URL supplied,
  // determines whether a preloaded version of the URL can be used,
  // and substitutes the prerendered RVH into the TabContents.  Returns
  // whether or not a prerendered RVH could be used or not.
  bool MaybeUsePreloadedPage(TabContents* tc, const GURL& url);

  // Allows PrerenderContents to remove itself when prerendering should
  // be cancelled.
  void RemoveEntry(PrerenderContents* entry);

  // Retrieves the PrerenderContents object for the specified URL, if it
  // has been prerendered.  The caller will then have ownership of the
  // PrerenderContents object and is responsible for freeing it.
  // Returns NULL if the specified URL has not been prerendered.
  PrerenderContents* GetEntry(const GURL& url);

  // Records the perceived page load time for a page - effectively the time from
  // when the user navigates to a page to when it finishes loading. The actual
  // load may have started prior to navigation due to prerender hints.
  // This must be called on the UI thread.
  static void RecordPerceivedPageLoadTime(
      base::TimeDelta perceived_page_load_time,
      TabContents* tab_contents);

  // Records the time from when a page starts prerendering to when the user
  // navigates to it. This must be called on the UI thread.
  void RecordTimeUntilUsed(base::TimeDelta time_until_used);

  base::TimeDelta max_prerender_age() const { return max_prerender_age_; }
  void set_max_prerender_age(base::TimeDelta td) { max_prerender_age_ = td; }
  unsigned int max_elements() const { return max_elements_; }
  void set_max_elements(unsigned int num) { max_elements_ = num; }

  // Returns whether prerendering is currently enabled for this manager.
  // Must be called on the UI thread.
  bool is_enabled() const;

  // Set whether prerendering is currently enabled for this manager.
  // Must be called on the UI thread.
  // If |enabled| is false, existing prerendered pages will still persist until
  // they time out, but new ones will not be generated.
  void set_enabled(bool enabled);

  static PrerenderManagerMode GetMode();
  static void SetMode(PrerenderManagerMode mode);
  static bool IsPrerenderingPossible();
  static bool IsControlGroup();

  // The following static method can be called from any thread, but will result
  // in posting a task to the UI thread if we are not in the UI thread.
  static void RecordPrefetchTagObserved();

  // Maintaining and querying the set of TabContents belonging to this
  // PrerenderManager that are currently showing prerendered pages.
  void MarkTabContentsAsPrerendered(TabContents* tc);
  void MarkTabContentsAsWouldBePrerendered(TabContents* tc);
  void MarkTabContentsAsNotPrerendered(TabContents* tc);
  bool IsTabContentsPrerendered(TabContents* tc) const;
  bool WouldTabContentsBePrerendered(TabContents* tc) const;

  // Extracts a urlencoded URL stored in a url= query parameter from a URL
  // supplied, if available, and stores it in alias_url.  Returns whether or not
  // the operation succeeded (i.e. a valid URL was found).
  static bool MaybeGetQueryStringBasedAliasURL(const GURL& url,
                                               GURL* alias_url);

 protected:
  struct PendingContentsData;

  virtual ~PrerenderManager();

  void SetPrerenderContentsFactory(
      PrerenderContents::Factory* prerender_contents_factory);
  bool rate_limit_enabled_;

  PendingContentsData* FindPendingEntry(const GURL& url);

 private:
  // Test that needs needs access to internal functions.
  friend class PrerenderBrowserTest;

  friend class base::RefCountedThreadSafe<PrerenderManager>;

  struct PrerenderContentsData;

  // Starts and stops scheduling periodic cleanups, respectively.
  void StartSchedulingPeriodicCleanups();
  void StopSchedulingPeriodicCleanups();

  // Deletes stale prerendered PrerenderContents.
  // Also identifies and kills PrerenderContents that use too much
  // resources.
  void PeriodicCleanup();

  bool IsPrerenderElementFresh(const base::Time start) const;
  void DeleteOldEntries();
  virtual base::Time GetCurrentTime() const;
  virtual base::TimeTicks GetCurrentTimeTicks() const;
  virtual PrerenderContents* CreatePrerenderContents(
      const GURL& url,
      const std::vector<GURL>& alias_urls,
      const GURL& referrer);

  // Finds the specified PrerenderContents and returns it, if it exists.
  // Returns NULL otherwise.  Unlike GetEntry, the PrerenderManager maintains
  // ownership of the PrerenderContents.
  PrerenderContents* FindEntry(const GURL& url);

  static bool WithinWindow();

  static void RecordPrefetchTagObservedOnUIThread();

  // Called when removing a preload to ensure we clean up any pending preloads
  // that might remain in the map.
  void RemovePendingPreload(PrerenderContents* entry);

  bool DoesRateLimitAllowPrerender() const;

  // Specifies whether prerendering is currently enabled for this
  // manager. The value can change dynamically during the lifetime
  // of the PrerenderManager.
  bool enabled_;

  Profile* profile_;

  base::TimeDelta max_prerender_age_;
  unsigned int max_elements_;

  // List of prerendered elements.
  std::list<PrerenderContentsData> prerender_list_;

  // Set of TabContents which are currently displaying a prerendered page.
  base::hash_set<TabContents*> prerendered_tc_set_;

  // Set of TabContents which would be displaying a prerendered page
  // (for the control group).
  base::hash_set<TabContents*> would_be_prerendered_tc_set_;

  // Map of child/route id pairs to pending prerender data.
  typedef std::map<std::pair<int, int>, std::vector<PendingContentsData> >
      PendingPrerenderList;
  PendingPrerenderList pending_prerender_list_;

  // Default maximum permitted elements to prerender.
  static const unsigned int kDefaultMaxPrerenderElements = 1;

  // Default maximum age a prerendered element may have, in seconds.
  static const int kDefaultMaxPrerenderAgeSeconds = 20;

  // Time window for which we will record windowed PLT's from the last
  // observed link rel=prefetch tag.
  static const int kWindowDurationSeconds = 30;

  // Time interval at which periodic cleanups are performed.
  static const int kPeriodicCleanupIntervalMs = 1000;

  // Time interval before a new prerender is allowed.
  static const int kMinTimeBetweenPrerendersMs = 500;

  scoped_ptr<PrerenderContents::Factory> prerender_contents_factory_;

  static PrerenderManagerMode mode_;

  // The time when we last saw a prefetch request coming from a renderer.
  // This is used to record perceived PLT's for a certain amount of time
  // from the point that we last saw a <link rel=prefetch> tag.
  // This static variable should only be modified on the UI thread.
  static base::TimeTicks last_prefetch_seen_time_;

  // A count of how many prerenders we do per session. Initialized to 0 then
  // incremented and emitted to a histogram on each successful prerender.
  static int prerenders_per_session_count_;

  // RepeatingTimer to perform periodic cleanups of pending prerendered
  // pages.
  base::RepeatingTimer<PrerenderManager> repeating_timer_;

  // Track time of last prerender to limit prerender spam.
  base::TimeTicks last_prerender_start_time_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderManager);
};

}  // prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
