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
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/predictors/logged_in_predictor_table.h"
#include "chrome/browser/prerender/prerender_config.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_events.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "url/gurl.h"

class Profile;
class InstantSearchPrerendererTest;
struct ChromeCookieDetails;

namespace base {
class DictionaryValue;
}

namespace chrome {
struct NavigateParams;
}

namespace content {
class WebContents;
}

namespace gfx {
class Size;
}

namespace net {
class URLRequestContextGetter;
}

namespace prerender {

class PrerenderCondition;
class PrerenderHandle;
class PrerenderHistory;
class PrerenderLocalPredictor;

// PrerenderManager is responsible for initiating and keeping prerendered
// views of web pages. All methods must be called on the UI thread unless
// indicated otherwise.
class PrerenderManager : public base::SupportsWeakPtr<PrerenderManager>,
                         public base::NonThreadSafe,
                         public content::NotificationObserver,
                         public content::RenderProcessHostObserver,
                         public KeyedService,
                         public MediaCaptureDevicesDispatcher::Observer {
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

  typedef predictors::LoggedInPredictorTable::LoggedInStateMap LoggedInStateMap;

  // ID indicating that no experiment is active.
  static const uint8 kNoExperiment = 0;

  // Owned by a Profile object for the lifetime of the profile.
  PrerenderManager(Profile* profile, PrerenderTracker* prerender_tracker);

  virtual ~PrerenderManager();

  // From KeyedService:
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
      uint32 rel_types,
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

  PrerenderHandle* AddPrerenderFromLocalPredictor(
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Size& size);

  PrerenderHandle* AddPrerenderFromExternalRequest(
      const GURL& url,
      const content::Referrer& referrer,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Size& size);

  // Adds a prerender for Instant Search |url| if valid. The
  // |session_storage_namespace| matches the namespace of the active tab at the
  // time the prerender is generated. Returns a caller-owned PrerenderHandle* or
  // NULL.
  PrerenderHandle* AddPrerenderForInstant(
      const GURL& url,
      content::SessionStorageNamespace* session_storage_namespace,
      const gfx::Size& size);

  // Cancels all active prerenders.
  void CancelAllPrerenders();

  // If |url| matches a valid prerendered page and |params| are compatible, try
  // to swap it and merge browsing histories. Returns |true| and updates
  // |params->target_contents| if a prerendered page is swapped in, |false|
  // otherwise.
  bool MaybeUsePrerenderedPage(const GURL& url,
                               chrome::NavigateParams* params);

  // Moves a PrerenderContents to the pending delete list from the list of
  // active prerenders when prerendering should be cancelled.
  virtual void MoveEntryToPendingDelete(PrerenderContents* entry,
                                        FinalStatus final_status);

  // Records the page load time for a prerender that wasn't swapped in.
  void RecordPageLoadTimeNotSwappedIn(Origin origin,
                                      base::TimeDelta page_load_time,
                                      const GURL& url);

  // Records the perceived page load time for a page - effectively the time from
  // when the user navigates to a page to when it finishes loading. The actual
  // load may have started prior to navigation due to prerender hints.
  // This must be called on the UI thread.
  // |fraction_plt_elapsed_at_swap_in| must either be in [0.0, 1.0], or a value
  // outside that range indicating that it doesn't apply.
  void RecordPerceivedPageLoadTime(
      Origin origin,
      NavigationType navigation_type,
      base::TimeDelta perceived_page_load_time,
      double fraction_plt_elapsed_at_swap_in,
      const GURL& url);

  // Set whether prerendering is currently enabled for this manager.
  // Must be called on the UI thread.
  // If |enabled| is false, existing prerendered pages will still persist until
  // they time out, but new ones will not be generated.
  void set_enabled(bool enabled);

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
  bool IsWebContentsPrerendering(const content::WebContents* web_contents,
                                 Origin* origin) const;

  // Whether the PrerenderManager has an active prerender with the given url and
  // SessionStorageNamespace associated with the given WebContens.
  bool HasPrerenderedUrl(GURL url, content::WebContents* web_contents) const;

  // Returns the PrerenderContents object for the given web_contents, otherwise
  // returns NULL. Note that the PrerenderContents may have been Destroy()ed,
  // but not yet deleted.
  PrerenderContents* GetPrerenderContents(
      const content::WebContents* web_contents) const;

  // Returns the PrerenderContents object for a given child_id, route_id pair,
  // otherwise returns NULL. Note that the PrerenderContents may have been
  // Destroy()ed, but not yet deleted.
  virtual PrerenderContents* GetPrerenderContentsForRoute(
      int child_id, int route_id) const;

  // Returns a list of all WebContents being prerendered.
  const std::vector<content::WebContents*> GetAllPrerenderingContents() const;

  // Checks whether |url| has been recently navigated to.
  bool HasRecentlyBeenNavigatedTo(Origin origin, const GURL& url);

  // Returns true iff the method given is valid for prerendering.
  static bool IsValidHttpMethod(const std::string& method);

  // Returns true iff the scheme of the URL given is valid for prerendering.
  static bool DoesURLHaveValidScheme(const GURL& url);

  // Returns true iff the scheme of the subresource URL given is valid for
  // prerendering.
  static bool DoesSubresourceURLHaveValidScheme(const GURL& url);

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

  // Record a cookie status histogram (see prerender_histograms.h).
  void RecordCookieStatus(Origin origin,
                          uint8 experiment_id,
                          int cookie_status) const;

  // Record a cookie send type histogram (see prerender_histograms.h).
  void RecordCookieSendType(Origin origin,
                            uint8 experiment_id,
                            int cookie_send_type) const;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // MediaCaptureDevicesDispatcher::Observer
  virtual void OnCreatingAudioStream(int render_process_id,
                                     int render_frame_id) OVERRIDE;

  const Config& config() const { return config_; }
  Config& mutable_config() { return config_; }

  PrerenderTracker* prerender_tracker() { return prerender_tracker_; }

  bool cookie_store_loaded() { return cookie_store_loaded_; }

  // Adds a condition. This is owned by the PrerenderManager.
  void AddCondition(const PrerenderCondition* condition);

  // Records that some visible tab navigated (or was redirected) to the
  // provided URL.
  void RecordNavigation(const GURL& url);

  // Updates the LoggedInPredictor state to reflect that a login has likely
  // on the URL provided.
  void RecordLikelyLoginOnURL(const GURL& url);

  // Checks if the LoggedInPredictor shows that the user is likely logged on
  // to the site for the URL provided.
  void CheckIfLikelyLoggedInOnURL(const GURL& url,
                                  bool* lookup_result,
                                  bool* database_was_present,
                                  const base::Closure& result_cb);

  void OnHistoryServiceDidQueryURL(Origin origin,
                                   uint8 experiment_id,
                                   bool success,
                                   const history::URLRow& url_row,
                                   const history::VisitVector& visits);

  Profile* profile() const { return profile_; }

  // Classes which will be tested in prerender unit browser tests should use
  // these methods to get times for comparison, so that the test framework can
  // mock advancing/retarding time.
  virtual base::Time GetCurrentTime() const;
  virtual base::TimeTicks GetCurrentTimeTicks() const;

  scoped_refptr<predictors::LoggedInPredictorTable>
  logged_in_predictor_table() {
    return logged_in_predictor_table_;
  }

  PrerenderLocalPredictor* local_predictor() {
    return local_predictor_.get();
  }

  // Notification that a cookie event happened on a render frame. Will record a
  // cookie event for a given render frame, if it is being prerendered.
  // If cookies were sent, all cookies must be supplied in |cookie_list|.
  static void RecordCookieEvent(int process_id,
                                int frame_id,
                                const GURL& url,
                                const GURL& frame_url,
                                bool is_for_blocking_resource,
                                PrerenderContents::CookieEvent event,
                                const net::CookieList* cookie_list);

  // Arranges for all session storage merges to hang indefinitely. This is used
  // to reliably test various swap abort cases.
  static void HangSessionStorageMergesForTesting();

  // Notification that a prerender has completed and its bytes should be
  // recorded.
  void RecordNetworkBytes(Origin origin, bool used, int64 prerender_bytes);

  // Returns whether prerendering is currently enabled for this manager.
  bool IsEnabled() const;

  // Add to the running tally of bytes transferred over the network for this
  // profile if prerendering is currently enabled.
  void AddProfileNetworkBytesIfEnabled(int64 bytes);

  // Registers a new ProcessHost performing a prerender. Called by
  // PrerenderContents.
  void AddPrerenderProcessHost(content::RenderProcessHost* process_host);

  // Returns whether or not |process_host| may be reused for new navigations
  // from a prerendering perspective. Currently, if Prerender Cookie Stores are
  // enabled, prerenders must be in their own processes that may not be shared.
  bool MayReuseProcessHost(content::RenderProcessHost* process_host);

  // content::RenderProcessHostObserver implementation.
  virtual void RenderProcessHostDestroyed(
      content::RenderProcessHost* host) OVERRIDE;

  // To be called once the cookie store for this profile has been loaded.
  void OnCookieStoreLoaded();

  // For testing purposes. Issues a callback once the cookie store has been
  // loaded.
  void set_on_cookie_store_loaded_cb_for_testing(base::Closure cb) {
    on_cookie_store_loaded_cb_for_testing_ = cb;
  }

 protected:
  class PendingSwap;
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

    base::TimeTicks abandon_time() const { return abandon_time_; }

    base::TimeTicks expiry_time() const { return expiry_time_; }
    void set_expiry_time(base::TimeTicks expiry_time) {
      expiry_time_ = expiry_time;
    }

    void ClearPendingSwap();

    PendingSwap* pending_swap() { return pending_swap_.get(); }
    void set_pending_swap(PendingSwap* pending_swap) {
      pending_swap_.reset(pending_swap);
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

    // The time when OnHandleNavigatedAway was called.
    base::TimeTicks abandon_time_;

    // After this time, this prerender is no longer fresh, and should be
    // removed.
    base::TimeTicks expiry_time_;

    // If a session storage namespace merge is in progress for this object,
    // we need to keep track of various state associated with it.
    scoped_ptr<PendingSwap> pending_swap_;

    DISALLOW_COPY_AND_ASSIGN(PrerenderData);
  };

  // When a swap can't happen immediately, due to a sesison storage namespace
  // merge, there will be a pending swap object while the merge is in
  // progress. It retains all the data needed to do the merge, maintains
  // throttles for the navigation in the target WebContents that needs to be
  // delayed, and handles all conditions which would cancel a pending swap.
  class PendingSwap : public content::WebContentsObserver {
   public:
    PendingSwap(PrerenderManager* manager,
                content::WebContents* target_contents,
                PrerenderData* prerender_data,
                const GURL& url,
                bool should_replace_current_entry);
    virtual ~PendingSwap();

    content::WebContents* target_contents() const;
    void set_swap_successful(bool swap_successful) {
      swap_successful_ = swap_successful;
    }

    void BeginSwap();

    // content::WebContentsObserver implementation.
    virtual void AboutToNavigateRenderView(
        content::RenderViewHost* render_view_host) OVERRIDE;
    virtual void ProvisionalChangeToMainFrameUrl(
        const GURL& url,
        content::RenderFrameHost* render_frame_host) OVERRIDE;
    virtual void DidCommitProvisionalLoadForFrame(
        content::RenderFrameHost* render_frame_host,
        const GURL& validated_url,
        content::PageTransition transition_type) OVERRIDE;
    virtual void DidFailProvisionalLoad(
        content::RenderFrameHost* render_frame_host,
        const GURL& validated_url,
        int error_code,
        const base::string16& error_description) OVERRIDE;
    virtual void WebContentsDestroyed() OVERRIDE;

   private:
    void RecordEvent(PrerenderEvent event) const;

    void OnMergeCompleted(content::SessionStorageNamespace::MergeResult result);
    void OnMergeTimeout();

    // Prerender parameters.
    PrerenderManager* manager_;
    PrerenderData* prerender_data_;
    GURL url_;
    bool should_replace_current_entry_;

    base::TimeTicks start_time_;
    PrerenderTracker::ChildRouteIdPair target_route_id_;
    bool seen_target_route_id_;
    base::OneShotTimer<PendingSwap> merge_timeout_;
    bool swap_successful_;

    base::WeakPtrFactory<PendingSwap> weak_factory_;
  };

  void SetPrerenderContentsFactory(
      PrerenderContents::Factory* prerender_contents_factory);

  // Called by a PrerenderData to signal that the launcher has navigated away
  // from the context that launched the prerender. A user may have clicked
  // a link in a page containing a <link rel=prerender> element, or the user
  // might have committed an omnibox navigation. This is used to possibly
  // shorten the TTL of the prerendered page.
  void SourceNavigatedAway(PrerenderData* prerender_data);

  // Gets the request context for the profile.
  // For unit tests, this will be overriden to return NULL, since it is not
  // needed.
  virtual net::URLRequestContextGetter* GetURLRequestContext();

 private:
  friend class ::InstantSearchPrerendererTest;
  friend class PrerenderBrowserTest;
  friend class PrerenderContents;
  friend class PrerenderHandle;
  friend class UnitTestPrerenderManager;

  class OnCloseWebContentsDeleter;
  struct NavigationRecord;

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

  base::TimeTicks GetExpiryTimeForNewPrerender(Origin origin) const;
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

  // Finds the active PrerenderData object currently in a PendingSwap for
  // |target_contents|. Otherwise, returns NULL.
  PrerenderData* FindPrerenderDataForTargetContents(
      content::WebContents* target_contents);

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

  // Records the final status a prerender in the case that a PrerenderContents
  // was never created, and also adds a PrerenderHistory entry.
  // This is a helper function which will ultimately call
  // RecordFinalStatusWthMatchCompleteStatus, using MATCH_COMPLETE_DEFAULT.
  void RecordFinalStatusWithoutCreatingPrerenderContents(
      const GURL& url, Origin origin, uint8 experiment_id,
      FinalStatus final_status) const;


  void CookieChanged(ChromeCookieDetails* details);
  void CookieChangedAnyCookiesLeftLookupResult(const std::string& domain_key,
                                               bool cookies_exist);
  void LoggedInPredictorDataReceived(scoped_ptr<LoggedInStateMap> new_map);

  void RecordEvent(PrerenderContents* contents, PrerenderEvent event) const;

  // Swaps a prerender |prerender_data| for |url| into the tab, replacing
  // |web_contents|.  Returns the new WebContents that was swapped in, or NULL
  // if a swap-in was not possible.  If |should_replace_current_entry| is true,
  // the current history entry in |web_contents| is replaced.
  content::WebContents* SwapInternal(const GURL& url,
                                     content::WebContents* web_contents,
                                     PrerenderData* prerender_data,
                                     bool should_replace_current_entry);

  // The configuration.
  Config config_;

  // Specifies whether prerendering is currently enabled for this
  // manager. The value can change dynamically during the lifetime
  // of the PrerenderManager.
  bool enabled_;

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

  ScopedVector<OnCloseWebContentsDeleter> on_close_web_contents_deleters_;

  scoped_ptr<PrerenderHistory> prerender_history_;

  std::list<const PrerenderCondition*> prerender_conditions_;

  scoped_ptr<PrerenderHistograms> histograms_;

  scoped_ptr<PrerenderLocalPredictor> local_predictor_;

  scoped_refptr<predictors::LoggedInPredictorTable> logged_in_predictor_table_;

  // Here, we keep the logged in predictor state, but potentially a superset
  // of its actual (database-backed) state, since we do not incorporate
  // browser data deletion. We do not use this for actual lookups, but only
  // to query cookie data for domains we know there was a login before.
  // This is required to avoid a large number of cookie lookups on bulk
  // deletion of cookies.
  scoped_ptr<LoggedInStateMap> logged_in_state_;

  content::NotificationRegistrar notification_registrar_;

  base::CancelableTaskTracker query_url_tracker_;

  // The number of bytes transferred over the network for the profile this
  // PrerenderManager is attached to.
  int64 profile_network_bytes_;

  // The value of profile_network_bytes_ that was last recorded.
  int64 last_recorded_profile_network_bytes_;

  // Set of process hosts being prerendered.
  typedef std::set<content::RenderProcessHost*> PrerenderProcessSet;
  PrerenderProcessSet prerender_process_hosts_;

  // Indicates whether the cookie store for this profile has fully loaded yet.
  bool cookie_store_loaded_;

  base::Closure on_cookie_store_loaded_cb_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderManager);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MANAGER_H_
