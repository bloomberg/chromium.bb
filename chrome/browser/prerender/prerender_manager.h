// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#pragma once

#include <list>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/task.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/prerender/prerender_config.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "googleurl/src/gurl.h"

class Profile;
class TabContents;

namespace base {
class DictionaryValue;
}

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

class PrerenderCondition;
class PrerenderHistograms;
class PrerenderHistory;
class PrerenderTracker;

// PrerenderManager is responsible for initiating and keeping prerendered
// views of web pages. All methods must be called on the UI thread unless
// indicated otherwise.
class PrerenderManager : public base::SupportsWeakPtr<PrerenderManager>,
                         public base::NonThreadSafe,
                         public ProfileKeyedService {
 public:
  // PrerenderManagerMode is used in a UMA_HISTOGRAM, so please do not
  // add in the middle.
  enum PrerenderManagerMode {
    PRERENDER_MODE_DISABLED,
    PRERENDER_MODE_ENABLED,
    PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP,
    PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP,
    PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP,
    PRERENDER_MODE_MAX
  };

  // One or more of these flags must be passed to ClearData() to specify just
  // what data to clear.  See function declaration for more information.
  enum ClearFlags {
    CLEAR_PRERENDER_CONTENTS = 0x1 << 0,
    CLEAR_PRERENDER_HISTORY = 0x1 << 1,
    CLEAR_MAX = 0x1 << 2
  };

  // ID indicating that no experiment is active.
  static const uint8 kNoExperiment = 0;

  // Owned by a Profile object for the lifetime of the profile.
  PrerenderManager(Profile* profile, PrerenderTracker* prerender_tracker);

  virtual ~PrerenderManager();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Entry points for adding prerenders.

  // Adds a prerender for |url| if valid. |process_id| and |route_id| identify
  // the RenderViewHost that the prerender request came from and are used to
  // set the initial window size of the RenderViewHost used for prerendering.
  // Returns true if the URL was added, false if it was not.
  // If the RenderViewHost source is itself prerendering, the prerender is added
  // as a pending prerender.
  bool AddPrerenderFromLinkRelPrerender(int process_id, int route_id,
                                        const GURL& url,
                                        const content::Referrer& referrer);

  // Adds a prerender for |url| if valid. As the prerender request is coming
  // from a source without a RenderViewHost (i.e., the omnibox) we don't have a
  // child or route id, or a referrer. This method uses sensible values for
  // those. The |session_storage_namespace| matches the namespace of the active
  // tab at the time the prerender is generated from the omnibox.
  bool AddPrerenderFromOmnibox(
      const GURL& url,
      SessionStorageNamespace* session_storage_namespace);

  // Destroy all prerenders for the given child route id pair and assign a final
  // status to them.
  virtual void DestroyPrerenderForRenderView(int process_id, int view_id,
                                             FinalStatus final_status);

  // Cancels all active prerenders.
  void CancelAllPrerenders();

  // For a given TabContents that wants to navigate to the URL supplied,
  // determines whether a prerendered version of the URL can be used,
  // and substitutes the prerendered RVH into the TabContents. |opener_url| is
  // set to the window.opener url that the TabContents should have set and
  // will be empty if there is no opener set. Returns whether or not a
  // prerendered RVH could be used or not.
  bool MaybeUsePrerenderedPage(TabContents* tab_contents,
                               const GURL& url,
                               const GURL& opener_url);

  // Moves a PrerenderContents to the pending delete list from the list of
  // active prerenders when prerendering should be cancelled.
  void MoveEntryToPendingDelete(PrerenderContents* entry,
                                FinalStatus final_status);

  // Records the perceived page load time for a page - effectively the time from
  // when the user navigates to a page to when it finishes loading. The actual
  // load may have started prior to navigation due to prerender hints.
  // This must be called on the UI thread.
  static void RecordPerceivedPageLoadTime(
      base::TimeDelta perceived_page_load_time,
      TabContents* tab_contents,
      const GURL& url);

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
  static bool ActuallyPrerendering();
  static bool IsControlGroup();
  static bool IsNoUseGroup();

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
  bool IsOldRenderViewHost(const RenderViewHost* render_view_host) const;

  // Checks whether navigation to the provided URL has occurred in a visible
  // tab recently.
  bool HasRecentlyBeenNavigatedTo(const GURL& url);

  // Returns true if the method given is invalid for prerendering.
  static bool IsValidHttpMethod(const std::string& method);

  // Returns a Value object containing the active pages being prerendered, and
  // a history of pages which were prerendered. The caller is responsible for
  // deleting the return value.
  base::DictionaryValue* GetAsValue() const;

  // Clears the data indicated by which bits of clear_flags are set.
  //
  // If the CLEAR_PRERENDER_CONTENTS bit is set, all active prerenders are
  // cancelled and then deleted, and any TabContents queued for destruction are
  // destroyed as well.
  //
  // If the CLEAR_PRERENDER_HISTORY bit is set, the prerender history is
  // cleared, including any entries newly created by destroying them in
  // response to the CLEAR_PRERENDER_CONTENTS flag.
  //
  // Intended to be used when clearing the cache or history.
  void ClearData(int clear_flags);

  // Record a final status of a prerendered page in a histogram.
  void RecordFinalStatus(Origin origin,
                         uint8 experiment_id,
                         FinalStatus final_status) const;

  const Config& config() const { return config_; }
  Config& mutable_config() { return config_; }

  PrerenderTracker* prerender_tracker() { return prerender_tracker_; }

  // Adds a condition. This is owned by the PrerenderManager.
  void AddCondition(const PrerenderCondition* condition);

  bool IsTopSite(const GURL& url);

  bool IsPendingEntry(const GURL& url) const;

 protected:
  void SetPrerenderContentsFactory(
      PrerenderContents::Factory* prerender_contents_factory);

  // Utility method that is called from the virtual Shutdown method on this
  // class but is called directly from the TestPrerenderManager in the unit
  // tests.
  void DoShutdown();

 private:
  // Needs access to AddPrerender.
  friend class PrerenderContents;

  // Test that needs needs access to internal functions.
  friend class PrerenderBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, AliasURLTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, ClearTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, ControlGroup);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, DropOldestRequestTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, DropSecondRequestTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, ExpireTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, FoundTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, FragmentMatchesFragmentTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, FragmentMatchesPageTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, PageMatchesFragmentTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, PendingPrerenderTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, RateLimitInWindowTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, RateLimitOutsideWindowTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, SourceRenderViewClosed);
  FRIEND_TEST_ALL_PREFIXES(PrerenderManagerTest, TwoElementPrerenderTest);

  struct PrerenderContentsData;
  struct NavigationRecord;

  class OnCloseTabContentsDeleter;

  class MostVisitedSites;

  // Adds a prerender for |url| from referrer |referrer| initiated from the
  // RenderViewHost specified by |child_route_id_pair|. The |origin| specifies
  // how the prerender was added. If the |session_storage_namespace| is NULL,
  // it is discovered using the RenderViewHost specified by
  // |child_route_id_pair|.
  bool AddPrerender(
      Origin origin,
      const std::pair<int, int>& child_route_id_pair,
      const GURL& url,
      const content::Referrer& referrer,
      SessionStorageNamespace* session_storage_namespace);

  // Adds a pending preload issued by the prerendering RenderView identified by
  // |child_route_id_pair|.  If and when that prerendering RenderView is used,
  // the specified prerender will start.
  void AddPendingPrerender(Origin origin,
                           const std::pair<int, int>& child_route_id_pair,
                           const GURL& url,
                           const content::Referrer& referrer);

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
      const content::Referrer& referrer,
      Origin origin,
      uint8 experiment_id);

  // Checks if the PrerenderContents has been added to the pending delete list.
  bool IsPendingDelete(PrerenderContents* entry) const;

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
  // window - effectively, up to 30 seconds after a prerender tag has been
  // observed.
  bool WithinWindow() const;

  bool DoesRateLimitAllowPrerender() const;

  // Deletes old TabContents that have been replaced by prerendered ones.  This
  // is needed because they're replaced in a callback from the old TabContents,
  // so cannot immediately be deleted.
  void DeleteOldTabContents();

  // Cleans up old NavigationRecord's.
  void CleanUpOldNavigations();

  // Arrange for the given tab contents to be deleted asap. If deleter is not
  // NULL, deletes that as well.
  void ScheduleDeleteOldTabContents(TabContentsWrapper* tab,
                                    OnCloseTabContentsDeleter* deleter);

  // Adds to the history list.
  void AddToHistory(PrerenderContents* contents);

  // Records that some visible tab navigated (or was redirected) to the
  // provided URL.
  void RecordNavigation(const GURL& url);

  // Returns a new Value representing the pages currently being prerendered. The
  // caller is responsible for delete'ing the return value.
  base::Value* GetActivePrerendersAsValue() const;

  // Destroys all pending prerenders using FinalStatus.  Also deletes them as
  // well as any swapped out TabContents queued for destruction.
  // Used both on destruction, and when clearing the browsing history.
  void DestroyAllContents(FinalStatus final_status);

  // The configuration.
  Config config_;

  // Specifies whether prerendering is currently enabled for this
  // manager. The value can change dynamically during the lifetime
  // of the PrerenderManager.
  bool enabled_;

  // The profile that owns this PrerenderManager.
  Profile* profile_;

  PrerenderTracker* prerender_tracker_;

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

  scoped_ptr<PrerenderContents::Factory> prerender_contents_factory_;

  static PrerenderManagerMode mode_;

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
  base::WeakPtrFactory<PrerenderManager> weak_factory_;

  ScopedVector<OnCloseTabContentsDeleter> on_close_tab_contents_deleters_;

  scoped_ptr<PrerenderHistory> prerender_history_;

  std::list<const PrerenderCondition*> prerender_conditions_;

  scoped_ptr<PrerenderHistograms> histograms_;

  scoped_ptr<MostVisitedSites> most_visited_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderManager);
};

PrerenderManager* FindPrerenderManagerUsingRenderProcessId(
    int render_process_id);

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
