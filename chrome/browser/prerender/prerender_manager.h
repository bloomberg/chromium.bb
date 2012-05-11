// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#pragma once

#include <list>
#include <string>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
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

namespace base {
class DictionaryValue;
}

namespace content {
class WebContents;
}

namespace gfx {
class Size;
}

#if defined(COMPILER_GCC)

namespace BASE_HASH_NAMESPACE {
template <>
struct hash<content::WebContents*> {
  std::size_t operator()(content::WebContents* value) const {
    return reinterpret_cast<std::size_t>(value);
  }
};
}

#endif

namespace prerender {

class PrerenderCondition;
class PrerenderHistograms;
class PrerenderHistory;
class PrerenderLocalPredictor;
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
    PRERENDER_MODE_EXPERIMENT_5MIN_TTL_GROUP,
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
  // the RenderView that the prerender request came from.  The |size| may be
  // empty, and the current tab size will be used if it is. If the current
  // active tab size cannot be found, we use a default from PrerenderConfig.
  // Returns true if the URL was added, false if it was not.
  // If the launching RenderView is itself prerendering, the prerender is added
  // as a pending prerender.
  bool AddPrerenderFromLinkRelPrerender(
      int process_id,
      int route_id,
      const GURL& url,
      const content::Referrer& referrer,
      const gfx::Size& size);

  // Adds a prerender for |url| if valid. As the prerender request is coming
  // from a source without a RenderViewHost (i.e., the omnibox) we don't have a
  // child or route id, or a referrer. This method uses sensible values for
  // those. The |session_storage_namespace| matches the namespace of the active
  // tab at the time the prerender is generated from the omnibox.
  bool AddPrerenderFromOmnibox(
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace);

  // Request cancelation of a previously added prerender. If the |active_count_|
  // of the prerender is one, it will be canceled.  Otherwise, |active_count_|
  // will be decremented by one.
  void MaybeCancelPrerender(const GURL& url);

  // Destroy all prerenders for the given child route id pair and assign a final
  // status to them.
  virtual void DestroyPrerenderForRenderView(int process_id,
                                             int view_id,
                                             FinalStatus final_status);

  // Cancels all active prerenders.
  void CancelAllPrerenders();

  // Cancels all active prerenders with the ORIGIN_OMNIBOX origin.
  void CancelOmniboxPrerenders();

  // If |url| matches a valid prerendered page, try to swap it into
  // |web_contents| and merge browsing histories. Returns |true| if a
  // prerendered page is swapped in, |false| otherwise.
  bool MaybeUsePrerenderedPage(content::WebContents* web_contents,
                               const GURL& url);

  // Moves a PrerenderContents to the pending delete list from the list of
  // active prerenders when prerendering should be cancelled.
  void MoveEntryToPendingDelete(PrerenderContents* entry,
                                FinalStatus final_status);

  // Records the perceived page load time for a page - effectively the time from
  // when the user navigates to a page to when it finishes loading. The actual
  // load may have started prior to navigation due to prerender hints.
  // This must be called on the UI thread.
  // |fraction_plt_elapsed_at_swap_in| must either be in [0.0, 1.0], or a value
  // outside that range indicating that it doesn't apply.
  static void RecordPerceivedPageLoadTime(
      base::TimeDelta perceived_page_load_time,
      double fraction_plt_elapsed_at_swap_in,
      content::WebContents* web_contents,
      const GURL& url);

  // Set whether prerendering is currently enabled for this manager.
  // Must be called on the UI thread.
  // If |enabled| is false, existing prerendered pages will still persist until
  // they time out, but new ones will not be generated.
  void set_enabled(bool enabled);

  // Controls if we launch or squash prefetch requests as they arrive from
  // renderers.
  static bool IsPrefetchEnabled();
  static void SetIsPrefetchEnabled(bool enabled);

  static PrerenderManagerMode GetMode();
  static void SetMode(PrerenderManagerMode mode);
  static const char* GetModeString();
  static bool IsPrerenderingPossible();
  static bool ActuallyPrerendering();
  static bool IsControlGroup();
  static bool IsNoUseGroup();

  // Query the list of current prerender pages to see if the given web contents
  // is prerendering a page.
  bool IsWebContentsPrerendering(content::WebContents* web_contents) const;

  // Returns true if there is a prerendered page for the given URL and it has
  // finished loading. Only valid if called before MaybeUsePrerenderedPage.
  bool DidPrerenderFinishLoading(const GURL& url) const;

  // Maintaining and querying the set of WebContents belonging to this
  // PrerenderManager that are currently showing prerendered pages.
  void MarkWebContentsAsPrerendered(content::WebContents* web_contents);
  void MarkWebContentsAsWouldBePrerendered(content::WebContents* web_contents);
  void MarkWebContentsAsNotPrerendered(content::WebContents* web_contents);
  bool IsWebContentsPrerendered(content::WebContents* web_contents) const;
  bool WouldWebContentsBePrerendered(content::WebContents* web_contents) const;

  // Checks whether |url| has been recently navigated to.
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
  // cancelled and then deleted, and any WebContents queued for destruction are
  // destroyed as well.
  //
  // If the CLEAR_PRERENDER_HISTORY bit is set, the prerender history is
  // cleared, including any entries newly created by destroying them in
  // response to the CLEAR_PRERENDER_CONTENTS flag.
  //
  // Intended to be used when clearing the cache or history.
  void ClearData(int clear_flags);

  // Record a final status of a prerendered page in a histogram.
  // This variation allows specifying whether prerendering had been started
  // (necessary to flag MatchComplete dummies).
  void RecordFinalStatusWithMatchCompleteStatus(
      Origin origin,
      uint8 experiment_id,
      PrerenderContents::MatchCompleteStatus mc_status,
      FinalStatus final_status) const;

  const Config& config() const { return config_; }
  Config& mutable_config() { return config_; }

  PrerenderTracker* prerender_tracker() { return prerender_tracker_; }

  // Adds a condition. This is owned by the PrerenderManager.
  void AddCondition(const PrerenderCondition* condition);

  bool IsTopSite(const GURL& url);

  bool IsPendingEntry(const GURL& url) const;

  // Returns true if |url| matches any URLs being prerendered.
  bool IsPrerendering(const GURL& url) const;

  // Records that some visible tab navigated (or was redirected) to the
  // provided URL.
  void RecordNavigation(const GURL& url);

  Profile* profile() const { return profile_; }

 protected:
  void SetPrerenderContentsFactory(
      PrerenderContents::Factory* prerender_contents_factory);

  // Utility method that is called from the virtual Shutdown method on this
  // class but is called directly from the TestPrerenderManager in the unit
  // tests.
  void DoShutdown();

 private:
  // Test that needs needs access to internal functions.
  friend class PrerenderBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, AliasURLTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, CancelAllTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest,
                           CancelOmniboxRemovesOmniboxTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest,
                           CancelOmniboxDoesNotRemoveLinkTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, ClearTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, ControlGroup);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, DropOldestRequestTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, DropSecondRequestTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, ExpireTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, FoundTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, FragmentMatchesFragmentTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, FragmentMatchesPageTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, LinkManagerAbandon);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, LinkManagerAddTwiceAbandonTwice);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, LinkManagerAddTwiceCancelTwice);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest,
                           LinkManagerAddTwiceCancelTwiceThenAbandonTwice);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, LinkManagerCancel);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, LinkManagerCancelThenAbandon);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, LinkManagerCancelThenAddAgain);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, LinkManagerCancelTwice);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, LinkManagerExpireThenAddAgain);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, LinkManagerExpireThenCancel);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, PageMatchesFragmentTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, PendingPrerenderTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, RateLimitInWindowTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, RateLimitOutsideWindowTest);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, SourceRenderViewClosed);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTest, TwoElementPrerenderTest);

  struct PrerenderContentsData;
  struct NavigationRecord;

  class OnCloseTabContentsDeleter;

  class MostVisitedSites;

  typedef std::list<PrerenderContentsData> PrerenderContentsDataList;

  // Adds a prerender for |url| from referrer |referrer| initiated from the
  // child process specified by |child_id|. The |origin| specifies how the
  // prerender was added. If the |size| is empty, then
  // PrerenderContents::StartPrerendering will instead use the size of the
  // currently active tab. If the current active tab size cannot be found, it
  // then uses a default from PrerenderConfig.
  bool AddPrerender(
      Origin origin,
      int child_id,
      const GURL& url,
      const content::Referrer& referrer,
      const gfx::Size& size,
      content::SessionStorageNamespace* session_storage_namespace);

  // Retrieves the PrerenderContents object for the specified URL, if it
  // has been prerendered.  The caller will then have ownership of the
  // PrerenderContents object and is responsible for freeing it.
  // Returns NULL if the specified URL has not been prerendered.
  PrerenderContents* GetEntry(const GURL& url);

  // Identical to GetEntry, with one exception:
  // The WebContents specified indicates the WC in which to swap the
  // prerendering into.  If the WebContents specified is the one
  // to doing the prerendered itself, will return NULL.
  PrerenderContents* GetEntryButNotSpecifiedWC(const GURL& url,
                                               content::WebContents* wc);

  // Starts scheduling periodic cleanups.
  void StartSchedulingPeriodicCleanups();
  // Stops scheduling periodic cleanups if they're no longer needed.
  void MaybeStopSchedulingPeriodicCleanups();

  // Deletes stale and cancelled prerendered PrerenderContents, as well as
  // WebContents that have been replaced by prerendered WebContents.
  // Also identifies and kills PrerenderContents that use too much
  // resources.
  void PeriodicCleanup();

  // Posts a task to call PeriodicCleanup.  Results in quicker destruction of
  // objects.  If |this| is deleted before the task is run, the task will
  // automatically be cancelled.
  void PostCleanupTask();

  base::TimeDelta GetMaxAge() const;
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

  // Finds the specified PrerenderContentsData/PrerenderContents and returns it,
  // if it exists. Returns NULL otherwise.  Unlike GetEntry, the
  // PrerenderManager maintains ownership of the PrerenderContents.
  PrerenderContentsData* FindEntryData(const GURL& url);
  PrerenderContents* FindEntry(const GURL& url) const;

  // Returns the iterator to the PrerenderContentsData entry that is being
  // prerendered from the given child route id pair.
  PrerenderContentsDataList::iterator
      FindPrerenderContentsForChildRouteIdPair(
          const std::pair<int, int>& child_route_id_pair);

  PrerenderContentsDataList::iterator
      FindPrerenderContentsForURL(const GURL& url);

  bool DoesRateLimitAllowPrerender() const;

  // Deletes old WebContents that have been replaced by prerendered ones.  This
  // is needed because they're replaced in a callback from the old WebContents,
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

  // Returns a new Value representing the pages currently being prerendered. The
  // caller is responsible for delete'ing the return value.
  base::Value* GetActivePrerendersAsValue() const;

  // Destroys all pending prerenders using FinalStatus.  Also deletes them as
  // well as any swapped out WebContents queued for destruction.
  // Used both on destruction, and when clearing the browsing history.
  void DestroyAllContents(FinalStatus final_status);

  // Helper function to destroy a PrerenderContents with the specified
  // final_status, while at the same time recording that for the MatchComplete
  // case, that this prerender would have been used.
  void DestroyAndMarkMatchCompleteAsUsed(PrerenderContents* prerender_contents,
                                         FinalStatus final_status);

  // Record a final status of a prerendered page in a histogram.
  // This is a helper function which will ultimately call
  // RecordFinalStatusWthMatchCompleteStatus, using MATCH_COMPLETE_DEFAULT.
  void RecordFinalStatus(Origin origin,
                         uint8 experiment_id,
                         FinalStatus final_status) const;

  // Returns whether prerendering is currently enabled for this manager.
  // Must be called on the UI thread.
  bool IsEnabled() const;

  // The configuration.
  Config config_;

  // Specifies whether prerendering is currently enabled for this
  // manager. The value can change dynamically during the lifetime
  // of the PrerenderManager.
  bool enabled_;

  static bool is_prefetch_enabled_;

  // The profile that owns this PrerenderManager.
  Profile* profile_;

  PrerenderTracker* prerender_tracker_;

  // List of prerendered elements.
  PrerenderContentsDataList prerender_list_;

  // List of recent navigations in this profile, sorted by ascending
  // navigate_time_.
  std::list<NavigationRecord> navigations_;

  // List of prerender elements to be deleted
  std::list<PrerenderContents*> pending_delete_list_;

  // Set of WebContents which are currently displaying a prerendered page.
  base::hash_set<content::WebContents*> prerendered_tab_contents_set_;

  // Set of WebContents which would be displaying a prerendered page
  // (for the control group).
  base::hash_set<content::WebContents*> would_be_prerendered_tab_contents_set_;

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

  scoped_ptr<PrerenderLocalPredictor> local_predictor_;

  scoped_ptr<MostVisitedSites> most_visited_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderManager);
};

PrerenderManager* FindPrerenderManagerUsingRenderProcessId(
    int render_process_id);

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
