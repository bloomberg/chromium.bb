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
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task.h"
#include "base/threading/non_thread_safe.h"
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

// Adds either a preload or a pending preload to the PrerenderManager.
// Must be called on the UI thread.
void HandlePrefetchTag(
    const base::WeakPtr<PrerenderManager>& prerender_manager,
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const GURL& referrer,
    bool make_pending);

// PrerenderManager is responsible for initiating and keeping prerendered
// views of webpages. All methods must be called on the UI thread unless
// indicated otherwise.
class PrerenderManager : public base::SupportsWeakPtr<PrerenderManager>,
                         public base::NonThreadSafe {
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

  virtual ~PrerenderManager();

  // Preloads |url| if valid.  |child_route_id_pair| identifies the
  // RenderViewHost that the prerender request came from and is used to
  // set the initial window size of the RenderViewHost used for prerendering.
  // Returns true if the URL was added, false if it was not.
  bool AddPreload(
      const std::pair<int, int>& child_route_id_pair,
      const GURL& url,
      const GURL& referrer);

  void AddPendingPreload(const std::pair<int, int>& child_route_id_pair,
                         const GURL& url,
                         const GURL& referrer);

  // Destroy all preloads for the given child route id pair and assign a final
  // status to them.
  virtual void DestroyPreloadForChildRouteIdPair(
      const std::pair<int, int>& child_route_id_pair,
      FinalStatus final_status);

  // For a given TabContents that wants to navigate to the URL supplied,
  // determines whether a preloaded version of the URL can be used,
  // and substitutes the prerendered RVH into the TabContents.  Returns
  // whether or not a prerendered RVH could be used or not.
  bool MaybeUsePreloadedPage(TabContents* tab_contents, const GURL& url);
  bool MaybeUsePreloadedPageOld(TabContents* tab_contents, const GURL& url);

  // Moves a PrerenderContents to the pending delete list from the list of
  // active prerenders when prerendering should be cancelled.
  void MoveEntryToPendingDelete(PrerenderContents* entry);

  // Checks if the PrerenderContents has been added to the pending delete list.
  bool IsPendingDelete(PrerenderContents* entry) const;

  // Retrieves the PrerenderContents object for the specified URL, if it
  // has been prerendered.  The caller will then have ownership of the
  // PrerenderContents object and is responsible for freeing it.
  // Returns NULL if the specified URL has not been prerendered.
  PrerenderContents* GetEntry(const GURL& url);

  // Identical to GetEntry, with one exception:
  // The TabContents specified indicates the TC in which to swap the
  // prerendering into.  If the TabContents specified is the one
  // to doing the prerendered itself, will return NULL.
  PrerenderContents* GetEntryButNotSpecifiedTC(const GURL& url,
                                               TabContents* tc);

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

  base::TimeDelta max_prerender_age() const;
  void set_max_prerender_age(base::TimeDelta max_age);
  size_t max_prerender_memory_mb() const;
  void set_max_prerender_memory_mb(size_t prerender_memory_mb);
  unsigned int max_elements() const;
  void set_max_elements(unsigned int num);

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

  // Records that a prefetch tag has been observed.
  void RecordPrefetchTagObserved();

  // Query the list of current prerender pages to see if the given tab contents
  // is prerendering a page.
  bool IsTabContentsPrerendering(TabContents* tab_contents) const;

  // Maintaining and querying the set of TabContents belonging to this
  // PrerenderManager that are currently showing prerendered pages.
  void MarkTabContentsAsPrerendered(TabContents* tab_contents);
  void MarkTabContentsAsWouldBePrerendered(TabContents* tab_contents);
  void MarkTabContentsAsNotPrerendered(TabContents* tab_contents);
  bool IsTabContentsPrerendered(TabContents* tab_contents) const;
  bool WouldTabContentsBePrerendered(TabContents* tab_contents) const;

  // Records that some visible tab navigated (or was redirected) to the
  // provided URL.
  void RecordNavigation(const GURL& url);

  // Checks whether navigation to the provided URL has occured in a visible
  // tab recently.
  bool HasRecentlyBeenNavigatedTo(const GURL& url);

  // Extracts a urlencoded URL stored in a url= query parameter from a URL
  // supplied, if available, and stores it in alias_url.  Returns whether or not
  // the operation succeeded (i.e. a valid URL was found).
  static bool MaybeGetQueryStringBasedAliasURL(const GURL& url,
                                               GURL* alias_url);

  // Returns true if the method given is invalid for prerendering.
  static bool IsValidHttpMethod(const std::string& method);

 protected:
  struct PendingContentsData;

  void SetPrerenderContentsFactory(
      PrerenderContents::Factory* prerender_contents_factory);
  bool rate_limit_enabled_;

  PendingContentsData* FindPendingEntry(const GURL& url);

 private:
  // Test that needs needs access to internal functions.
  friend class PrerenderBrowserTest;

  friend class base::RefCountedThreadSafe<PrerenderManager>;

  struct PrerenderContentsData;
  struct NavigationRecord;

  // Starts scheduling periodic cleanups.
  void StartSchedulingPeriodicCleanups();
  // Stops scheduling periodic cleanups if they're no longer needed.
  void MaybeStopSchedulingPeriodicCleanups();

  // Deletes stale and cancelled prerendered PrerenderContents, as well as
  // TabContents that have been replaced by prerendered TabContents.
  // Also identifies and kills PrerenderContents that use too much
  // resources.
  void PeriodicCleanup();

  // Posts a task to call PeriodicCleanup.  Results in quicker destruction of
  // objects.  If |this| is deleted before the task is run, the task will
  // automatically be cancelled.
  void PostCleanupTask();

  bool IsPrerenderElementFresh(const base::Time start) const;
  void DeleteOldEntries();
  virtual base::Time GetCurrentTime() const;
  virtual base::TimeTicks GetCurrentTimeTicks() const;
  virtual PrerenderContents* CreatePrerenderContents(
      const GURL& url,
      const GURL& referrer);

  // Deletes any PrerenderContents that have been added to the pending delete
  // list.
  void DeletePendingDeleteEntries();

  // Finds the specified PrerenderContents and returns it, if it exists.
  // Returns NULL otherwise.  Unlike GetEntry, the PrerenderManager maintains
  // ownership of the PrerenderContents.
  PrerenderContents* FindEntry(const GURL& url);

  // Returns the iterator to the PrerenderContentsData entry that is being
  // prerendered from the given child route id pair.
  std::list<PrerenderContentsData>::iterator
      FindPrerenderContentsForChildRouteIdPair(
          const std::pair<int, int>& child_route_id_pair);

  // Returns whether the PrerenderManager is currently within the prerender
  // window - effectively, up to 30 seconds after a prefetch tag has been
  // observed.
  bool WithinWindow() const;

  // Called when removing a preload to ensure we clean up any pending preloads
  // that might remain in the map.
  void RemovePendingPreload(PrerenderContents* entry);

  bool DoesRateLimitAllowPrerender() const;

  // Deletes old TabContents that have been replaced by prerendered ones.  This
  // is needed because they're replaced in a callback from the old TabContents,
  // so cannot immediately be deleted.
  void DeleteOldTabContents();

  // Cleans up old NavigationRecord's.
  void CleanUpOldNavigations();

  // Specifies whether prerendering is currently enabled for this
  // manager. The value can change dynamically during the lifetime
  // of the PrerenderManager.
  bool enabled_;

  Profile* profile_;

  base::TimeDelta max_prerender_age_;
  // Maximum amount of memory, in megabytes, that a single PrerenderContents
  // can use before it's cancelled.
  size_t max_prerender_memory_mb_;
  unsigned int max_elements_;

  // List of prerendered elements.
  std::list<PrerenderContentsData> prerender_list_;

  // List of recent navigations in this profile, sorted by ascending
  // navigate_time_.
  std::list<NavigationRecord> navigations_;

  // List of prerender elements to be deleted
  std::list<PrerenderContents*> pending_delete_list_;

  // Set of TabContents which are currently displaying a prerendered page.
  base::hash_set<TabContents*> prerendered_tab_contents_set_;

  // Set of TabContents which would be displaying a prerendered page
  // (for the control group).
  base::hash_set<TabContents*> would_be_prerendered_tab_contents_set_;

  // Map of child/route id pairs to pending prerender data.
  typedef std::map<std::pair<int, int>, std::vector<PendingContentsData> >
      PendingPrerenderList;
  PendingPrerenderList pending_prerender_list_;

  scoped_ptr<PrerenderContents::Factory> prerender_contents_factory_;

  static PrerenderManagerMode mode_;

  // The time when we last saw a prefetch request coming from a renderer.
  // This is used to record perceived PLT's for a certain amount of time
  // from the point that we last saw a <link rel=prefetch> tag.
  base::TimeTicks last_prefetch_seen_time_;

  // A count of how many prerenders we do per session. Initialized to 0 then
  // incremented and emitted to a histogram on each successful prerender.
  static int prerenders_per_session_count_;

  // RepeatingTimer to perform periodic cleanups of pending prerendered
  // pages.
  base::RepeatingTimer<PrerenderManager> repeating_timer_;

  // Track time of last prerender to limit prerender spam.
  base::TimeTicks last_prerender_start_time_;

  std::list<TabContentsWrapper*> old_tab_contents_list_;

  // Cancels pending tasks on deletion.
  ScopedRunnableMethodFactory<PrerenderManager> runnable_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderManager);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
