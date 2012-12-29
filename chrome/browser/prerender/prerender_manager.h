// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

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

}  // namespace BASE_HASH_NAMESPACE

#endif

namespace prerender {

class PrerenderCondition;
class PrerenderHandle;
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
  // NOTE: New values need to be appended, since they are used in histograms.
  enum PrerenderManagerMode {
    PRERENDER_MODE_DISABLED = 0,
    PRERENDER_MODE_ENABLED = 1,
    PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP = 2,
    PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP = 3,
    // Obsolete: PRERENDER_MODE_EXPERIMENT_5MIN_TTL_GROUP = 4,
    PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP = 5,
    PRERENDER_MODE_EXPERIMENT_MULTI_PRERENDER_GROUP = 6,
    PRERENDER_MODE_EXPERIMENT_15MIN_TTL_GROUP = 7,
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

  // From ProfileKeyedService:
  virtual void Shutdown() OVERRIDE;

  // Entry points for adding prerenders.

  // Adds a prerender for |url| if valid. |process_id| and |route_id| identify
  // the RenderView that the prerender request came from. If |size| is empty, a
  // default from the PrerenderConfig is used. Returns a caller-owned
  // PrerenderHandle* if the URL was added, NULL if it was not. If the launching
  // RenderView is itself prerendering, the prerender is added as a pending
  // prerender.
  PrerenderHandle* AddPrerenderFromLinkRelPrerender(
      int process_id,
      int route_id,
      const GURL& url,
      const content::Referrer& referrer,
      const gfx::Size& size);

  // Adds a prerender for |url| if valid. As the prerender request is coming
  // from a source without a RenderViewHost (i.e., the omnibox) we don't have a
  // child or route id, or a referrer. This method uses sensible values for
  // those. The |session_storage_namespace| matches the namespace of the active
  // tab at the time the prerender is generated from the omnibox. Returns a
  // caller-owned PrerenderHandle*, or NULL.
  PrerenderHandle* AddPrerenderFromOmnibox(
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Size& size);

  // If |process_id| and |view_id| refer to a running prerender, destroy
  // it with |final_status|.
  virtual void DestroyPrerenderForRenderView(int process_id,
                                             int view_id,
                                             FinalStatus final_status);

  // Cancels all active prerenders.
  void CancelAllPrerenders();

  // If |url| matches a valid prerendered page, try to swap it into
  // |web_contents| and merge browsing histories. Returns |true| if a
  // prerendered page is swapped in, |false| otherwise.
  bool MaybeUsePrerenderedPage(content::WebContents* web_contents,
                               const GURL& url);

  // Moves a PrerenderContents to the pending delete list from the list of
  // active prerenders when prerendering should be cancelled.
  virtual void MoveEntryToPendingDelete(PrerenderContents* entry,
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

  // Records the percentage of pixels of the final page in place at swap-in.
  void RecordFractionPixelsFinalAtSwapin(
      content::WebContents* web_contents,
      double fraction);

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
  static bool IsControlGroup(uint8 experiment_id);
  static bool IsNoUseGroup();

  // Query the list of current prerender pages to see if the given web contents
  // is prerendering a page. The optional parameter |origin| is an output
  // parameter which, if a prerender is found, is set to the Origin of the
  // prerender |web_contents|.
  bool IsWebContentsPrerendering(content::WebContents* web_contents,
                                 Origin* origin) const;

  // Returns the PrerenderContents object for the given web_contents if it's
  // used for an active prerender page, otherwise returns NULL.
  PrerenderContents* GetPrerenderContents(
      content::WebContents* web_contents) const;

  // Returns a list of all WebContents being prerendered.
  const std::vector<content::WebContents*> GetAllPrerenderingContents() const;

  // Maintaining and querying the set of WebContents belonging to this
  // PrerenderManager that are currently showing prerendered pages.
  void MarkWebContentsAsPrerendered(content::WebContents* web_contents,
                                    Origin origin);
  void MarkWebContentsAsWouldBePrerendered(content::WebContents* web_contents,
                                           Origin origin);
  void MarkWebContentsAsNotPrerendered(content::WebContents* web_contents);

  // Returns true if |web_contents| was originally a prerender that has since
  // been swapped in. The optional parameter |origin| is an output parameter
  // which, if a prerender is found, is set to the Origin of the prerender of
  // |web_contents|.
  bool IsWebContentsPrerendered(content::WebContents* web_contents,
                                Origin* origin) const;
  bool WouldWebContentsBePrerendered(content::WebContents* web_contents,
                                     Origin* origin) const;

  // Checks whether |url| has been recently navigated to.
  bool HasRecentlyBeenNavigatedTo(Origin origin, const GURL& url);

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

  // Records that some visible tab navigated (or was redirected) to the
  // provided URL.
  void RecordNavigation(const GURL& url);

  Profile* profile() const { return profile_; }

  // Classes which will be tested in prerender unit browser tests should use
  // these methods to get times for comparison, so that the test framework can
  // mock advancing/retarding time.
  virtual base::Time GetCurrentTime() const;
  virtual base::TimeTicks GetCurrentTimeTicks() const;

 protected:
  class PrerenderData : public base::SupportsWeakPtr<PrerenderData> {
   public:
    struct OrderByExpiryTime;

    PrerenderData(PrerenderManager* manager,
                  PrerenderContents* contents,
                  base::TimeTicks expiry_time);

    ~PrerenderData();

    // Turn this PrerenderData into a Match Complete replacement for itself,
    // placing the current prerender contents into |to_delete_prerenders_|.
    void MakeIntoMatchCompleteReplacement();

    // A new PrerenderHandle has been created for this PrerenderData.
    void OnHandleCreated(PrerenderHandle* prerender_handle);

    // The launcher associated with a handle is navigating away from the context
    // that launched this prerender. If the prerender is active, it may stay
    // alive briefly though, in case we we going through a redirect chain that
    // will eventually land at it.
    void OnHandleNavigatedAway(PrerenderHandle* prerender_handle);

    // The launcher associated with a handle has taken explicit action to cancel
    // this prerender. We may well destroy the prerender in this case if no
    // other handles continue to track it.
    void OnHandleCanceled(PrerenderHandle* prerender_handle);

    PrerenderContents* contents() { return contents_.get(); }

    PrerenderContents* ReleaseContents();

    int handle_count() const { return handle_count_; }

    base::TimeTicks expiry_time() const { return expiry_time_; }
    void set_expiry_time(base::TimeTicks expiry_time) {
      expiry_time_ = expiry_time;
    }

   private:
    PrerenderManager* manager_;
    scoped_ptr<PrerenderContents> contents_;

    // The number of distinct PrerenderHandles created for |this|, including
    // ones that have called PrerenderData::OnHandleNavigatedAway(), but not
    // counting the ones that have called PrerenderData::OnHandleCanceled(). For
    // pending prerenders, this will always be 1, since the PrerenderManager
    // only merges handles of running prerenders.
    int handle_count_;

    // After this time, this prerender is no longer fresh, and should be
    // removed.
    base::TimeTicks expiry_time_;

    DISALLOW_COPY_AND_ASSIGN(PrerenderData);
  };

  void SetPrerenderContentsFactory(
      PrerenderContents::Factory* prerender_contents_factory);

  // Adds prerenders from the pending Prerenders, called by
  // PrerenderContents::StartPendingPrerenders.
  void StartPendingPrerenders(
      int process_id,
      ScopedVector<PrerenderContents::PendingPrerenderInfo>* pending_prerenders,
      content::SessionStorageNamespace* session_storage_namespace);

  // Called by a PrerenderData to signal that the launcher has navigated away
  // from the context that launched the prerender. A user may have clicked
  // a link in a page containing a <link rel=prerender> element, or the user
  // might have committed an omnibox navigation. This is used to possibly
  // shorten the TTL of the prerendered page.
  void SourceNavigatedAway(PrerenderData* prerender_data);

 private:
  friend class PrerenderBrowserTest;
  friend class PrerenderContents;
  friend class PrerenderHandle;
  friend class UnitTestPrerenderManager;

  class OnCloseWebContentsDeleter;
  struct NavigationRecord;

  // For each WebContents that is swapped in, we store a
  // PrerenderedWebContentsData so that we can track the origin of the
  // prerender.
  struct PrerenderedWebContentsData {
    explicit PrerenderedWebContentsData(Origin origin);

    Origin origin;
  };

  // In the control group experimental group for each WebContents "not swapped
  // in" we create a WouldBePrerenderedWebContentsData to the origin of the
  // "prerender" we did not launch. We also track a state machine to ensure
  // the histogram reporting tracks what histograms would have done.
  struct WouldBePrerenderedWebContentsData {
    // When the WebContents gets a provisional load, we'd like to remove the
    // WebContents from  the map since the new navigation would not have swapped
    // in a prerender. But the first provisional load after the control
    // prerender is not "swapped in" is actually to the prerendered location! So
    // we don't remove the item from the map on the first provisional load, but
    // we do for subsequent loads.
    enum State {
      WAITING_FOR_PROVISIONAL_LOAD,
      SEEN_PROVISIONAL_LOAD,
    };

    explicit WouldBePrerenderedWebContentsData(Origin origin);

    Origin origin;
    State state;
  };

  // Time interval before a new prerender is allowed.
  static const int kMinTimeBetweenPrerendersMs = 500;

  // Time window for which we record old navigations, in milliseconds.
  static const int kNavigationRecordWindowMs = 5000;

  void OnCancelPrerenderHandle(PrerenderData* prerender_data);

  // Adds a prerender for |url| from |referrer| initiated from the process
  // |child_id|. The |origin| specifies how the prerender was added. If |size|
  // is empty, then PrerenderContents::StartPrerendering will instead use a
  // default from PrerenderConfig. Returns a PrerenderHandle*, owned by the
  // caller, or NULL.
  PrerenderHandle* AddPrerender(
      Origin origin,
      int child_id,
      const GURL& url,
      const content::Referrer& referrer,
      const gfx::Size& size,
      content::SessionStorageNamespace* session_storage_namespace);

  void StartSchedulingPeriodicCleanups();
  void StopSchedulingPeriodicCleanups();

  void EvictOldestPrerendersIfNecessary();

  // Deletes stale and cancelled prerendered PrerenderContents, as well as
  // WebContents that have been replaced by prerendered WebContents.
  // Also identifies and kills PrerenderContents that use too much
  // resources.
  void PeriodicCleanup();

  // Posts a task to call PeriodicCleanup.  Results in quicker destruction of
  // objects.  If |this| is deleted before the task is run, the task will
  // automatically be cancelled.
  void PostCleanupTask();

  base::TimeTicks GetExpiryTimeForNewPrerender() const;
  base::TimeTicks GetExpiryTimeForNavigatedAwayPrerender() const;

  void DeleteOldEntries();
  virtual PrerenderContents* CreatePrerenderContents(
      const GURL& url,
      const content::Referrer& referrer,
      Origin origin,
      uint8 experiment_id);

  // Insures the |active_prerenders_| are sorted by increasing expiry time. Call
  // after every mutation of active_prerenders_ that can possibly make it
  // unsorted (e.g. an insert, or changing an expiry time).
  void SortActivePrerenders();

  // Finds the active PrerenderData object for a running prerender matching
  // |url| and |session_storage_namespace|.
  PrerenderData* FindPrerenderData(
      const GURL& url,
      const content::SessionStorageNamespace* session_storage_namespace);

  // If |child_id| and |route_id| correspond to a RenderView that is an active
  // prerender, returns the PrerenderData object for that prerender. Otherwise,
  // returns NULL.
  PrerenderData* FindPrerenderDataForChildAndRoute(int child_id, int route_id);

  // Given the |prerender_contents|, find the iterator in active_prerenders_
  // correponding to the given prerender.
  ScopedVector<PrerenderData>::iterator
      FindIteratorForPrerenderContents(PrerenderContents* prerender_contents);

  bool DoesRateLimitAllowPrerender(Origin origin) const;

  // Deletes old WebContents that have been replaced by prerendered ones.  This
  // is needed because they're replaced in a callback from the old WebContents,
  // so cannot immediately be deleted.
  void DeleteOldWebContents();

  // Cleans up old NavigationRecord's.
  void CleanUpOldNavigations();

  // Arrange for the given WebContents to be deleted asap. If deleter is not
  // NULL, deletes that as well.
  void ScheduleDeleteOldWebContents(content::WebContents* tab,
                                    OnCloseWebContentsDeleter* deleter);

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

  // All running prerenders. Sorted by expiry time, in ascending order.
  ScopedVector<PrerenderData> active_prerenders_;

  // Prerenders awaiting deletion.
  ScopedVector<PrerenderData> to_delete_prerenders_;

  // List of recent navigations in this profile, sorted by ascending
  // navigate_time_.
  std::list<NavigationRecord> navigations_;

  // This map is from all WebContents which are currently displaying a
  // prerendered page which has already been swapped in to a
  // PrerenderedWebContentsData for tracking full lifetime information
  // on prerenders.
  base::hash_map<content::WebContents*, PrerenderedWebContentsData>
      prerendered_web_contents_data_;

  // WebContents that would have been swapped out for a prerendered WebContents
  // if the user was not part of the control group for measurement. When the
  // WebContents gets a provisional load, the WebContents is removed from
  // the map since the new navigation would not have swapped in a prerender.
  // However, one complication exists because the first provisional load after
  // the WebContents is marked as "Would Have Been Prerendered" is actually to
  // the prerendered location. So, we need to keep a state around that does
  // not clear the item from the map on the first provisional load, but does
  // for subsequent loads.
  base::hash_map<content::WebContents*, WouldBePrerenderedWebContentsData>
      would_be_prerendered_map_;

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

  std::list<content::WebContents*> old_web_contents_list_;

  // Cancels pending tasks on deletion.
  base::WeakPtrFactory<PrerenderManager> weak_factory_;

  ScopedVector<OnCloseWebContentsDeleter> on_close_web_contents_deleters_;

  scoped_ptr<PrerenderHistory> prerender_history_;

  std::list<const PrerenderCondition*> prerender_conditions_;

  scoped_ptr<PrerenderHistograms> histograms_;

  scoped_ptr<PrerenderLocalPredictor> local_predictor_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderManager);
};

PrerenderManager* FindPrerenderManagerUsingRenderProcessId(
    int render_process_id);

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
