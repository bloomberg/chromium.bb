// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/prerender/prerender_condition.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_history.h"
#include "chrome/browser/prerender/prerender_local_predictor.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/prerender_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::RenderFrameHost;
using content::SessionStorageNamespace;
using content::WebContents;
using predictors::LoggedInPredictorTable;

namespace prerender {

namespace {

// Time interval at which periodic cleanups are performed.
const int kPeriodicCleanupIntervalMs = 1000;

// Valid HTTP methods for prerendering.
const char* const kValidHttpMethods[] = {
  "GET",
  "HEAD",
  "OPTIONS",
  "POST",
  "TRACE",
};

// Length of prerender history, for display in chrome://net-internals
const int kHistoryLength = 100;

// Timeout, in ms, for a session storage namespace merge.
const int kSessionStorageNamespaceMergeTimeoutMs = 500;

// If true, all session storage merges hang indefinitely.
bool g_hang_session_storage_merges_for_testing = false;

// Indicates whether a Prerender has been cancelled such that we need
// a dummy replacement for the purpose of recording the correct PPLT for
// the Match Complete case.
// Traditionally, "Match" means that a prerendered page was actually visited &
// the prerender was used.  Our goal is to have "Match" cases line up in the
// control group & the experiment group, so that we can make meaningful
// comparisons of improvements.  However, in the control group, since we don't
// actually perform prerenders, many of the cancellation reasons cannot be
// detected.  Therefore, in the Prerender group, when we cancel for one of these
// reasons, we keep track of a dummy Prerender representing what we would
// have in the control group.  If that dummy prerender in the prerender group
// would then be swapped in (but isn't actually b/c it's a dummy), we record
// this as a MatchComplete.  This allows us to compare MatchComplete's
// across Prerender & Control group which ideally should be lining up.
// This ensures that there is no bias in terms of the page load times
// of the pages forming the difference between the two sets.

bool NeedMatchCompleteDummyForFinalStatus(FinalStatus final_status) {
  return final_status != FINAL_STATUS_USED &&
      final_status != FINAL_STATUS_TIMED_OUT &&
      final_status != FINAL_STATUS_MANAGER_SHUTDOWN &&
      final_status != FINAL_STATUS_PROFILE_DESTROYED &&
      final_status != FINAL_STATUS_APP_TERMINATING &&
      final_status != FINAL_STATUS_WINDOW_OPENER &&
      final_status != FINAL_STATUS_CACHE_OR_HISTORY_CLEARED &&
      final_status != FINAL_STATUS_CANCELLED &&
      final_status != FINAL_STATUS_DEVTOOLS_ATTACHED &&
      final_status != FINAL_STATUS_CROSS_SITE_NAVIGATION_PENDING &&
      final_status != FINAL_STATUS_PAGE_BEING_CAPTURED &&
      final_status != FINAL_STATUS_NAVIGATION_UNCOMMITTED &&
      final_status != FINAL_STATUS_NON_EMPTY_BROWSING_INSTANCE;
}

void CheckIfCookiesExistForDomainResultOnUIThread(
    const net::CookieMonster::HasCookiesForETLDP1Callback& callback,
    bool cookies_exist) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(cookies_exist);
}

void CheckIfCookiesExistForDomainResultOnIOThread(
    const net::CookieMonster::HasCookiesForETLDP1Callback& callback,
    bool cookies_exist) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CheckIfCookiesExistForDomainResultOnUIThread,
                 callback,
                 cookies_exist));
}

void CheckIfCookiesExistForDomainOnIOThread(
    net::URLRequestContextGetter* rq_context,
    const std::string& domain_key,
    const net::CookieMonster::HasCookiesForETLDP1Callback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieStore* cookie_store =
      rq_context->GetURLRequestContext()->cookie_store();
  cookie_store->GetCookieMonster()->HasCookiesForETLDP1Async(
      domain_key,
      base::Bind(&CheckIfCookiesExistForDomainResultOnIOThread, callback));
}

}  // namespace

class PrerenderManager::OnCloseWebContentsDeleter
    : public content::WebContentsDelegate,
      public base::SupportsWeakPtr<
          PrerenderManager::OnCloseWebContentsDeleter> {
 public:
  OnCloseWebContentsDeleter(PrerenderManager* manager,
                            WebContents* tab)
      : manager_(manager),
        tab_(tab),
        suppressed_dialog_(false) {
    tab_->SetDelegate(this);
    base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
        base::Bind(&OnCloseWebContentsDeleter::ScheduleWebContentsForDeletion,
                   AsWeakPtr(), true),
        base::TimeDelta::FromSeconds(kDeleteWithExtremePrejudiceSeconds));
  }

  virtual void CloseContents(WebContents* source) OVERRIDE {
    DCHECK_EQ(tab_, source);
    ScheduleWebContentsForDeletion(false);
  }

  virtual void SwappedOut(WebContents* source) OVERRIDE {
    DCHECK_EQ(tab_, source);
    ScheduleWebContentsForDeletion(false);
  }

  virtual bool ShouldSuppressDialogs() OVERRIDE {
    // Use this as a proxy for getting statistics on how often we fail to honor
    // the beforeunload event.
    suppressed_dialog_ = true;
    return true;
  }

 private:
  static const int kDeleteWithExtremePrejudiceSeconds = 3;

  void ScheduleWebContentsForDeletion(bool timeout) {
    UMA_HISTOGRAM_BOOLEAN("Prerender.TabContentsDeleterTimeout", timeout);
    UMA_HISTOGRAM_BOOLEAN("Prerender.TabContentsDeleterSuppressedDialog",
                          suppressed_dialog_);
    tab_->SetDelegate(NULL);
    manager_->ScheduleDeleteOldWebContents(tab_.release(), this);
    // |this| is deleted at this point.
  }

  PrerenderManager* manager_;
  scoped_ptr<WebContents> tab_;
  bool suppressed_dialog_;

  DISALLOW_COPY_AND_ASSIGN(OnCloseWebContentsDeleter);
};

// static
int PrerenderManager::prerenders_per_session_count_ = 0;

// static
PrerenderManager::PrerenderManagerMode PrerenderManager::mode_ =
    PRERENDER_MODE_ENABLED;

struct PrerenderManager::NavigationRecord {
  NavigationRecord(const GURL& url, base::TimeTicks time)
      : url(url),
        time(time) {
  }

  GURL url;
  base::TimeTicks time;
};

PrerenderManager::PrerenderManager(Profile* profile,
                                   PrerenderTracker* prerender_tracker)
    : enabled_(profile && profile->GetPrefs() &&
          profile->GetPrefs()->GetBoolean(prefs::kNetworkPredictionEnabled)),
      profile_(profile),
      prerender_tracker_(prerender_tracker),
      prerender_contents_factory_(PrerenderContents::CreateFactory()),
      last_prerender_start_time_(GetCurrentTimeTicks() -
          base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs)),
      prerender_history_(new PrerenderHistory(kHistoryLength)),
      histograms_(new PrerenderHistograms()),
      profile_network_bytes_(0),
      last_recorded_profile_network_bytes_(0),
      cookie_store_loaded_(false) {
  // There are some assumptions that the PrerenderManager is on the UI thread.
  // Any other checks simply make sure that the PrerenderManager is accessed on
  // the same thread that it was created on.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (IsLocalPredictorEnabled())
    local_predictor_.reset(new PrerenderLocalPredictor(this));

  if (IsLoggedInPredictorEnabled() && !profile_->IsOffTheRecord()) {
    predictors::PredictorDatabase* predictor_db =
        predictors::PredictorDatabaseFactory::GetForProfile(profile);
    if (predictor_db) {
      logged_in_predictor_table_ = predictor_db->logged_in_table();
      scoped_ptr<LoggedInStateMap> new_state_map(new LoggedInStateMap);
      LoggedInStateMap* new_state_map_ptr = new_state_map.get();
      BrowserThread::PostTaskAndReply(
          BrowserThread::DB, FROM_HERE,
          base::Bind(&LoggedInPredictorTable::GetAllData,
                     logged_in_predictor_table_,
                     new_state_map_ptr),
          base::Bind(&PrerenderManager::LoggedInPredictorDataReceived,
                     AsWeakPtr(),
                     base::Passed(&new_state_map)));
    }
  }

  // Certain experiments override our default config_ values.
  switch (PrerenderManager::GetMode()) {
    case PrerenderManager::PRERENDER_MODE_EXPERIMENT_MULTI_PRERENDER_GROUP:
      config_.max_link_concurrency = 4;
      config_.max_link_concurrency_per_launcher = 2;
      break;
    case PrerenderManager::PRERENDER_MODE_EXPERIMENT_15MIN_TTL_GROUP:
      config_.time_to_live = base::TimeDelta::FromMinutes(15);
      break;
    default:
      break;
  }

  notification_registrar_.Add(
      this, chrome::NOTIFICATION_COOKIE_CHANGED,
      content::NotificationService::AllBrowserContextsAndSources());

  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::Source<Profile>(profile_));

  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);
}

PrerenderManager::~PrerenderManager() {
  MediaCaptureDevicesDispatcher::GetInstance()->RemoveObserver(this);

  // The earlier call to KeyedService::Shutdown() should have
  // emptied these vectors already.
  DCHECK(active_prerenders_.empty());
  DCHECK(to_delete_prerenders_.empty());

  for (PrerenderProcessSet::const_iterator it =
           prerender_process_hosts_.begin();
       it != prerender_process_hosts_.end();
       ++it) {
    (*it)->RemoveObserver(this);
  }
}

void PrerenderManager::Shutdown() {
  DestroyAllContents(FINAL_STATUS_MANAGER_SHUTDOWN);
  STLDeleteElements(&prerender_conditions_);
  on_close_web_contents_deleters_.clear();
  // Must happen before |profile_| is set to NULL as
  // |local_predictor_| accesses it.
  if (local_predictor_)
    local_predictor_->Shutdown();
  profile_ = NULL;

  DCHECK(active_prerenders_.empty());
}

PrerenderHandle* PrerenderManager::AddPrerenderFromLinkRelPrerender(
    int process_id,
    int route_id,
    const GURL& url,
    const uint32 rel_types,
    const content::Referrer& referrer,
    const gfx::Size& size) {
  Origin origin = rel_types & PrerenderRelTypePrerender ?
                      ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN :
                      ORIGIN_LINK_REL_NEXT;
  SessionStorageNamespace* session_storage_namespace = NULL;
  // Unit tests pass in a process_id == -1.
  if (process_id != -1) {
    RenderViewHost* source_render_view_host =
        RenderViewHost::FromID(process_id, route_id);
    if (!source_render_view_host)
      return NULL;
    WebContents* source_web_contents =
        WebContents::FromRenderViewHost(source_render_view_host);
    if (!source_web_contents)
      return NULL;
    if (origin == ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN &&
        source_web_contents->GetURL().host() == url.host()) {
      origin = ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN;
    }
    // TODO(ajwong): This does not correctly handle storage for isolated apps.
    session_storage_namespace =
        source_web_contents->GetController()
            .GetDefaultSessionStorageNamespace();
  }

  return AddPrerender(origin, process_id, url, referrer, size,
                      session_storage_namespace);
}

PrerenderHandle* PrerenderManager::AddPrerenderFromOmnibox(
    const GURL& url,
    SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size) {
  if (!IsOmniboxEnabled(profile_))
    return NULL;
  return AddPrerender(ORIGIN_OMNIBOX, -1, url, content::Referrer(), size,
                      session_storage_namespace);
}

PrerenderHandle* PrerenderManager::AddPrerenderFromLocalPredictor(
    const GURL& url,
    SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size) {
  return AddPrerender(ORIGIN_LOCAL_PREDICTOR, -1, url, content::Referrer(),
                      size, session_storage_namespace);
}

PrerenderHandle* PrerenderManager::AddPrerenderFromExternalRequest(
    const GURL& url,
    const content::Referrer& referrer,
    SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size) {
  return AddPrerender(ORIGIN_EXTERNAL_REQUEST, -1, url, referrer, size,
                      session_storage_namespace);
}

PrerenderHandle* PrerenderManager::AddPrerenderForInstant(
    const GURL& url,
    content::SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size) {
  DCHECK(chrome::ShouldPrefetchSearchResults());
  return AddPrerender(ORIGIN_INSTANT, -1, url, content::Referrer(), size,
                      session_storage_namespace);
}

void PrerenderManager::CancelAllPrerenders() {
  DCHECK(CalledOnValidThread());
  while (!active_prerenders_.empty()) {
    PrerenderContents* prerender_contents =
        active_prerenders_.front()->contents();
    prerender_contents->Destroy(FINAL_STATUS_CANCELLED);
  }
}

bool PrerenderManager::MaybeUsePrerenderedPage(const GURL& url,
                                               chrome::NavigateParams* params) {
  DCHECK(CalledOnValidThread());

  content::WebContents* web_contents = params->target_contents;
  DCHECK(!IsWebContentsPrerendering(web_contents, NULL));

  // Don't prerender if the navigation involves some special parameters.
  if (params->uses_post || !params->extra_headers.empty())
    return false;

  DeleteOldEntries();
  to_delete_prerenders_.clear();

  // First, try to find prerender data with the correct session storage
  // namespace.
  // TODO(ajwong): This doesn't handle isolated apps correctly.
  PrerenderData* prerender_data = FindPrerenderData(
      url,
      web_contents->GetController().GetDefaultSessionStorageNamespace());

  // If this failed, we may still find a prerender for the same URL, but a
  // different session storage namespace. If we do, we might have to perform
  // a merge.
  if (!prerender_data) {
    prerender_data = FindPrerenderData(url, NULL);
  } else {
    RecordEvent(prerender_data->contents(),
                PRERENDER_EVENT_SWAPIN_CANDIDATE_NAMESPACE_MATCHES);
  }

  if (!prerender_data)
    return false;
  RecordEvent(prerender_data->contents(), PRERENDER_EVENT_SWAPIN_CANDIDATE);
  DCHECK(prerender_data->contents());

  // If there is currently a merge pending for this prerender data, don't swap.
  if (prerender_data->pending_swap())
    return false;

  // Abort any existing pending swap on the target contents.
  PrerenderData* pending_swap =
      FindPrerenderDataForTargetContents(web_contents);
  if (pending_swap) {
    pending_swap->ClearPendingSwap();
    DCHECK(FindPrerenderDataForTargetContents(web_contents) == NULL);
  }

  RecordEvent(prerender_data->contents(),
              PRERENDER_EVENT_SWAPIN_NO_MERGE_PENDING);
  SessionStorageNamespace* target_namespace =
      web_contents->GetController().GetDefaultSessionStorageNamespace();
  SessionStorageNamespace* prerender_namespace =
      prerender_data->contents()->GetSessionStorageNamespace();
  // Only when actually prerendering is session storage namespace merging an
  // issue. For the control group, it will be assumed that the merge succeeded.
  if (prerender_namespace && prerender_namespace != target_namespace &&
      !prerender_namespace->IsAliasOf(target_namespace)) {
    if (!ShouldMergeSessionStorageNamespaces()) {
      RecordEvent(prerender_data->contents(),
                  PRERENDER_EVENT_SWAPIN_MERGING_DISABLED);
      return false;
    }
    RecordEvent(prerender_data->contents(),
                PRERENDER_EVENT_SWAPIN_ISSUING_MERGE);
    prerender_data->set_pending_swap(new PendingSwap(
        this, web_contents, prerender_data, url,
        params->should_replace_current_entry));
    prerender_data->pending_swap()->BeginSwap();
    // Although this returns false, creating a PendingSwap registers with
    // PrerenderTracker to throttle MAIN_FRAME navigations while the swap is
    // pending.
    return false;
  }

  // No need to merge; swap synchronously.
  WebContents* new_web_contents = SwapInternal(
      url, web_contents, prerender_data,
      params->should_replace_current_entry);
  if (!new_web_contents)
    return false;

  // Record the new target_contents for the callers.
  params->target_contents = new_web_contents;
  return true;
}

WebContents* PrerenderManager::SwapInternal(
    const GURL& url,
    WebContents* web_contents,
    PrerenderData* prerender_data,
    bool should_replace_current_entry) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsWebContentsPrerendering(web_contents, NULL));

  // Only swap if the target WebContents has a CoreTabHelper delegate to swap
  // out of it. For a normal WebContents, this is if it is in a TabStripModel.
  CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(web_contents);
  if (!core_tab_helper || !core_tab_helper->delegate()) {
    RecordEvent(prerender_data->contents(), PRERENDER_EVENT_SWAPIN_NO_DELEGATE);
    return NULL;
  }

  PrerenderTabHelper* target_tab_helper =
      PrerenderTabHelper::FromWebContents(web_contents);
  if (!target_tab_helper) {
    NOTREACHED();
    return NULL;
  }

  if (IsNoSwapInExperiment(prerender_data->contents()->experiment_id()))
    return NULL;

  if (WebContents* new_web_contents =
      prerender_data->contents()->prerender_contents()) {
    if (web_contents == new_web_contents)
      return NULL;  // Do not swap in to ourself.

    // We cannot swap in if there is no last committed entry, because we would
    // show a blank page under an existing entry from the current tab.  Even if
    // there is a pending entry, it may not commit.
    // TODO(creis): If there is a pending navigation and no last committed
    // entry, we might be able to transfer the network request instead.
    if (!new_web_contents->GetController().CanPruneAllButLastCommitted()) {
      // Abort this prerender so it is not used later. http://crbug.com/292121
      prerender_data->contents()->Destroy(FINAL_STATUS_NAVIGATION_UNCOMMITTED);
      return NULL;
    }
  }

  // Do not swap if the target WebContents is not the only WebContents in its
  // current BrowsingInstance.
  if (web_contents->GetSiteInstance()->GetRelatedActiveContentsCount() != 1u) {
    DCHECK_GT(
        web_contents->GetSiteInstance()->GetRelatedActiveContentsCount(), 1u);
    prerender_data->contents()->Destroy(
        FINAL_STATUS_NON_EMPTY_BROWSING_INSTANCE);
    return NULL;
  }

  // Do not use the prerendered version if there is an opener object.
  if (web_contents->HasOpener()) {
    prerender_data->contents()->Destroy(FINAL_STATUS_WINDOW_OPENER);
    return NULL;
  }

  // Do not swap in the prerender if the current WebContents is being captured.
  if (web_contents->GetCapturerCount() > 0) {
    prerender_data->contents()->Destroy(FINAL_STATUS_PAGE_BEING_CAPTURED);
    return NULL;
  }

  // If we are just in the control group (which can be detected by noticing
  // that prerendering hasn't even started yet), record that |web_contents| now
  // would be showing a prerendered contents, but otherwise, don't do anything.
  if (!prerender_data->contents()->prerendering_has_started()) {
    target_tab_helper->WouldHavePrerenderedNextLoad(
        prerender_data->contents()->origin());
    prerender_data->contents()->Destroy(FINAL_STATUS_WOULD_HAVE_BEEN_USED);
    return NULL;
  }

  // Don't use prerendered pages if debugger is attached to the tab.
  // See http://crbug.com/98541
  if (content::DevToolsAgentHost::IsDebuggerAttached(web_contents)) {
    DestroyAndMarkMatchCompleteAsUsed(prerender_data->contents(),
                                      FINAL_STATUS_DEVTOOLS_ATTACHED);
    return NULL;
  }

  // If the prerendered page is in the middle of a cross-site navigation,
  // don't swap it in because there isn't a good way to merge histories.
  if (prerender_data->contents()->IsCrossSiteNavigationPending()) {
    DestroyAndMarkMatchCompleteAsUsed(
        prerender_data->contents(),
        FINAL_STATUS_CROSS_SITE_NAVIGATION_PENDING);
    return NULL;
  }

  // For bookkeeping purposes, we need to mark this WebContents to
  // reflect that it would have been prerendered.
  if (GetMode() == PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP) {
    target_tab_helper->WouldHavePrerenderedNextLoad(
        prerender_data->contents()->origin());
    prerender_data->contents()->Destroy(FINAL_STATUS_WOULD_HAVE_BEEN_USED);
    return NULL;
  }

  // At this point, we've determined that we will use the prerender.
  content::RenderProcessHost* process_host =
      prerender_data->contents()->GetRenderViewHost()->GetProcess();
  prerender_process_hosts_.erase(process_host);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PrerenderTracker::RemovePrerenderCookieStoreOnIOThread,
                 base::Unretained(prerender_tracker()), process_host->GetID(),
                 true));
  if (!prerender_data->contents()->load_start_time().is_null()) {
    histograms_->RecordTimeUntilUsed(
        prerender_data->contents()->origin(),
        GetCurrentTimeTicks() - prerender_data->contents()->load_start_time());
  }
  histograms_->RecordAbandonTimeUntilUsed(
      prerender_data->contents()->origin(),
      prerender_data->abandon_time().is_null() ?
          base::TimeDelta() :
          GetCurrentTimeTicks() - prerender_data->abandon_time());

  histograms_->RecordPerSessionCount(prerender_data->contents()->origin(),
                                     ++prerenders_per_session_count_);
  histograms_->RecordUsedPrerender(prerender_data->contents()->origin());

  if (prerender_data->pending_swap())
    prerender_data->pending_swap()->set_swap_successful(true);
  ScopedVector<PrerenderData>::iterator to_erase =
      FindIteratorForPrerenderContents(prerender_data->contents());
  DCHECK(active_prerenders_.end() != to_erase);
  DCHECK_EQ(prerender_data, *to_erase);
  scoped_ptr<PrerenderContents>
      prerender_contents(prerender_data->ReleaseContents());
  active_prerenders_.erase(to_erase);

  // Mark prerender as used.
  prerender_contents->PrepareForUse();

  WebContents* new_web_contents =
      prerender_contents->ReleasePrerenderContents();
  WebContents* old_web_contents = web_contents;
  DCHECK(new_web_contents);
  DCHECK(old_web_contents);

  // Merge the browsing history.
  new_web_contents->GetController().CopyStateFromAndPrune(
      &old_web_contents->GetController(),
      should_replace_current_entry);
  CoreTabHelper::FromWebContents(old_web_contents)->delegate()->
      SwapTabContents(old_web_contents,
                      new_web_contents,
                      true,
                      prerender_contents->has_finished_loading());
  prerender_contents->CommitHistory(new_web_contents);

  // Update PPLT metrics:
  // If the tab has finished loading, record a PPLT of 0.
  // If the tab is still loading, reset its start time to the current time.
  PrerenderTabHelper* prerender_tab_helper =
      PrerenderTabHelper::FromWebContents(new_web_contents);
  DCHECK(prerender_tab_helper != NULL);
  prerender_tab_helper->PrerenderSwappedIn();

  if (old_web_contents->NeedToFireBeforeUnload()) {
    // Schedule the delete to occur after the tab has run its unload handlers.
    // TODO(davidben): Honor the beforeunload event. http://crbug.com/304932
    on_close_web_contents_deleters_.push_back(
        new OnCloseWebContentsDeleter(this, old_web_contents));
    old_web_contents->DispatchBeforeUnload(false);
  } else {
    // No unload handler to run, so delete asap.
    ScheduleDeleteOldWebContents(old_web_contents, NULL);
  }

  // TODO(cbentzel): Should prerender_contents move to the pending delete
  //                 list, instead of deleting directly here?
  AddToHistory(prerender_contents.get());
  RecordNavigation(url);
  return new_web_contents;
}

void PrerenderManager::MoveEntryToPendingDelete(PrerenderContents* entry,
                                                FinalStatus final_status) {
  DCHECK(CalledOnValidThread());
  DCHECK(entry);

  ScopedVector<PrerenderData>::iterator it =
      FindIteratorForPrerenderContents(entry);
  DCHECK(it != active_prerenders_.end());

  // If this PrerenderContents is being deleted due to a cancellation any time
  // after the prerender has started then we need to create a dummy replacement
  // for PPLT accounting purposes for the Match Complete group. This is the case
  // if the cancellation is for any reason that would not occur in the control
  // group case.
  if (entry->prerendering_has_started() &&
      entry->match_complete_status() ==
          PrerenderContents::MATCH_COMPLETE_DEFAULT &&
      NeedMatchCompleteDummyForFinalStatus(final_status) &&
      ActuallyPrerendering()) {
    // TODO(tburkard): I'd like to DCHECK that we are actually prerendering.
    // However, what if new conditions are added and
    // NeedMatchCompleteDummyForFinalStatus is not being updated.  Not sure
    // what's the best thing to do here.  For now, I will just check whether
    // we are actually prerendering.
    (*it)->MakeIntoMatchCompleteReplacement();
  } else {
    to_delete_prerenders_.push_back(*it);
    (*it)->ClearPendingSwap();
    active_prerenders_.weak_erase(it);
  }

  // Destroy the old WebContents relatively promptly to reduce resource usage.
  PostCleanupTask();
}

void PrerenderManager::RecordPageLoadTimeNotSwappedIn(
    Origin origin,
    base::TimeDelta page_load_time,
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  histograms_->RecordPageLoadTimeNotSwappedIn(origin, page_load_time, url);
}

void PrerenderManager::RecordPerceivedPageLoadTime(
    Origin origin,
    NavigationType navigation_type,
    base::TimeDelta perceived_page_load_time,
    double fraction_plt_elapsed_at_swap_in,
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!IsEnabled())
    return;

  histograms_->RecordPerceivedPageLoadTime(
      origin, perceived_page_load_time, navigation_type, url);

  if (navigation_type == NAVIGATION_TYPE_PRERENDERED) {
    histograms_->RecordPercentLoadDoneAtSwapin(
        origin, fraction_plt_elapsed_at_swap_in);
  }
  if (local_predictor_) {
    local_predictor_->OnPLTEventForURL(url, perceived_page_load_time);
  }
}

void PrerenderManager::set_enabled(bool enabled) {
  DCHECK(CalledOnValidThread());
  enabled_ = enabled;
}

// static
PrerenderManager::PrerenderManagerMode PrerenderManager::GetMode() {
  return mode_;
}

// static
void PrerenderManager::SetMode(PrerenderManagerMode mode) {
  mode_ = mode;
}

// static
const char* PrerenderManager::GetModeString() {
  switch (mode_) {
    case PRERENDER_MODE_DISABLED:
      return "_Disabled";
    case PRERENDER_MODE_ENABLED:
    case PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP:
      return "_Enabled";
    case PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP:
      return "_Control";
    case PRERENDER_MODE_EXPERIMENT_MULTI_PRERENDER_GROUP:
      return "_Multi";
    case PRERENDER_MODE_EXPERIMENT_15MIN_TTL_GROUP:
      return "_15MinTTL";
    case PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP:
      return "_NoUse";
    case PRERENDER_MODE_MAX:
    default:
      NOTREACHED() << "Invalid PrerenderManager mode.";
      break;
  }
  return "";
}

// static
bool PrerenderManager::IsPrerenderingPossible() {
  return GetMode() != PRERENDER_MODE_DISABLED;
}

// static
bool PrerenderManager::ActuallyPrerendering() {
  return IsPrerenderingPossible() && !IsControlGroup(kNoExperiment);
}

// static
bool PrerenderManager::IsControlGroup(uint8 experiment_id) {
  return GetMode() == PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP ||
      IsControlGroupExperiment(experiment_id);
}

// static
bool PrerenderManager::IsNoUseGroup() {
  return GetMode() == PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP;
}

bool PrerenderManager::IsWebContentsPrerendering(
    const WebContents* web_contents,
    Origin* origin) const {
  DCHECK(CalledOnValidThread());
  if (PrerenderContents* prerender_contents =
          GetPrerenderContents(web_contents)) {
    if (origin)
      *origin = prerender_contents->origin();
    return true;
  }
  return false;
}

bool PrerenderManager::HasPrerenderedUrl(
    GURL url,
    content::WebContents* web_contents) const {
  content::SessionStorageNamespace* session_storage_namespace = web_contents->
      GetController().GetDefaultSessionStorageNamespace();

  for (ScopedVector<PrerenderData>::const_iterator it =
           active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    PrerenderContents* prerender_contents = (*it)->contents();
    if (prerender_contents->Matches(url, session_storage_namespace)) {
      return true;
    }
  }
  return false;
}

PrerenderContents* PrerenderManager::GetPrerenderContents(
    const content::WebContents* web_contents) const {
  DCHECK(CalledOnValidThread());
  for (ScopedVector<PrerenderData>::const_iterator it =
           active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    WebContents* prerender_web_contents =
        (*it)->contents()->prerender_contents();
    if (prerender_web_contents == web_contents) {
      return (*it)->contents();
    }
  }

  // Also check the pending-deletion list. If the prerender is in pending
  // delete, anyone with a handle on the WebContents needs to know.
  for (ScopedVector<PrerenderData>::const_iterator it =
           to_delete_prerenders_.begin();
       it != to_delete_prerenders_.end(); ++it) {
    WebContents* prerender_web_contents =
        (*it)->contents()->prerender_contents();
    if (prerender_web_contents == web_contents) {
      return (*it)->contents();
    }
  }
  return NULL;
}

PrerenderContents* PrerenderManager::GetPrerenderContentsForRoute(
    int child_id,
    int route_id) const {
  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(child_id, route_id);
  if (web_contents == NULL)
    return NULL;
  return GetPrerenderContents(web_contents);
}

const std::vector<WebContents*>
PrerenderManager::GetAllPrerenderingContents() const {
  DCHECK(CalledOnValidThread());
  std::vector<WebContents*> result;

  for (ScopedVector<PrerenderData>::const_iterator it =
           active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    if (WebContents* contents = (*it)->contents()->prerender_contents())
      result.push_back(contents);
  }

  return result;
}

bool PrerenderManager::HasRecentlyBeenNavigatedTo(Origin origin,
                                                  const GURL& url) {
  DCHECK(CalledOnValidThread());

  CleanUpOldNavigations();
  std::list<NavigationRecord>::const_reverse_iterator end = navigations_.rend();
  for (std::list<NavigationRecord>::const_reverse_iterator it =
           navigations_.rbegin();
       it != end;
       ++it) {
    if (it->url == url) {
      base::TimeDelta delta = GetCurrentTimeTicks() - it->time;
      histograms_->RecordTimeSinceLastRecentVisit(origin, delta);
      return true;
    }
  }

  return false;
}

// static
bool PrerenderManager::IsValidHttpMethod(const std::string& method) {
  // method has been canonicalized to upper case at this point so we can just
  // compare them.
  DCHECK_EQ(method, StringToUpperASCII(method));
  for (size_t i = 0; i < arraysize(kValidHttpMethods); ++i) {
    if (method.compare(kValidHttpMethods[i]) == 0)
      return true;
  }

  return false;
}

// static
bool PrerenderManager::DoesURLHaveValidScheme(const GURL& url) {
  return (url.SchemeIsHTTPOrHTTPS() ||
          url.SchemeIs(extensions::kExtensionScheme) ||
          url.SchemeIs("data"));
}

// static
bool PrerenderManager::DoesSubresourceURLHaveValidScheme(const GURL& url) {
  return DoesURLHaveValidScheme(url) || url == GURL(url::kAboutBlankURL);
}

base::DictionaryValue* PrerenderManager::GetAsValue() const {
  DCHECK(CalledOnValidThread());
  base::DictionaryValue* dict_value = new base::DictionaryValue();
  dict_value->Set("history", prerender_history_->GetEntriesAsValue());
  dict_value->Set("active", GetActivePrerendersAsValue());
  dict_value->SetBoolean("enabled", enabled_);
  dict_value->SetBoolean("omnibox_enabled", IsOmniboxEnabled(profile_));
  // If prerender is disabled via a flag this method is not even called.
  std::string enabled_note;
  if (IsControlGroup(kNoExperiment))
    enabled_note += "(Control group: Not actually prerendering) ";
  if (IsNoUseGroup())
    enabled_note += "(No-use group: Not swapping in prerendered pages) ";
  if (GetMode() == PRERENDER_MODE_EXPERIMENT_15MIN_TTL_GROUP)
    enabled_note +=
        "(15 min TTL group: Extended prerender eviction to 15 mins) ";
  dict_value->SetString("enabled_note", enabled_note);
  return dict_value;
}

void PrerenderManager::ClearData(int clear_flags) {
  DCHECK_GE(clear_flags, 0);
  DCHECK_LT(clear_flags, CLEAR_MAX);
  if (clear_flags & CLEAR_PRERENDER_CONTENTS)
    DestroyAllContents(FINAL_STATUS_CACHE_OR_HISTORY_CLEARED);
  // This has to be second, since destroying prerenders can add to the history.
  if (clear_flags & CLEAR_PRERENDER_HISTORY)
    prerender_history_->Clear();
}

void PrerenderManager::RecordFinalStatusWithMatchCompleteStatus(
    Origin origin,
    uint8 experiment_id,
    PrerenderContents::MatchCompleteStatus mc_status,
    FinalStatus final_status) const {
  histograms_->RecordFinalStatus(origin,
                                 experiment_id,
                                 mc_status,
                                 final_status);
}

void PrerenderManager::AddCondition(const PrerenderCondition* condition) {
  prerender_conditions_.push_back(condition);
}

void PrerenderManager::RecordNavigation(const GURL& url) {
  DCHECK(CalledOnValidThread());

  navigations_.push_back(NavigationRecord(url, GetCurrentTimeTicks()));
  CleanUpOldNavigations();
}

// protected
struct PrerenderManager::PrerenderData::OrderByExpiryTime {
  bool operator()(const PrerenderData* a, const PrerenderData* b) const {
    return a->expiry_time() < b->expiry_time();
  }
};

PrerenderManager::PrerenderData::PrerenderData(PrerenderManager* manager,
                                               PrerenderContents* contents,
                                               base::TimeTicks expiry_time)
    : manager_(manager),
      contents_(contents),
      handle_count_(0),
      expiry_time_(expiry_time) {
  DCHECK_NE(static_cast<PrerenderContents*>(NULL), contents_);
}

PrerenderManager::PrerenderData::~PrerenderData() {
}

void PrerenderManager::PrerenderData::MakeIntoMatchCompleteReplacement() {
  DCHECK(contents_);
  contents_->set_match_complete_status(
      PrerenderContents::MATCH_COMPLETE_REPLACED);
  PrerenderData* to_delete = new PrerenderData(manager_, contents_.release(),
                                               expiry_time_);
  contents_.reset(to_delete->contents_->CreateMatchCompleteReplacement());
  manager_->to_delete_prerenders_.push_back(to_delete);
}

void PrerenderManager::PrerenderData::OnHandleCreated(PrerenderHandle* handle) {
  DCHECK_NE(static_cast<PrerenderContents*>(NULL), contents_);
  ++handle_count_;
  contents_->AddObserver(handle);
}

void PrerenderManager::PrerenderData::OnHandleNavigatedAway(
    PrerenderHandle* handle) {
  DCHECK_LT(0, handle_count_);
  DCHECK_NE(static_cast<PrerenderContents*>(NULL), contents_);
  if (abandon_time_.is_null())
    abandon_time_ = base::TimeTicks::Now();
  // We intentionally don't decrement the handle count here, so that the
  // prerender won't be canceled until it times out.
  manager_->SourceNavigatedAway(this);
}

void PrerenderManager::PrerenderData::OnHandleCanceled(
    PrerenderHandle* handle) {
  DCHECK_LT(0, handle_count_);
  DCHECK_NE(static_cast<PrerenderContents*>(NULL), contents_);

  if (--handle_count_ == 0) {
    // This will eventually remove this object from active_prerenders_.
    contents_->Destroy(FINAL_STATUS_CANCELLED);
  }
}

void PrerenderManager::PrerenderData::ClearPendingSwap() {
  pending_swap_.reset(NULL);
}

PrerenderContents* PrerenderManager::PrerenderData::ReleaseContents() {
  return contents_.release();
}

PrerenderManager::PendingSwap::PendingSwap(
    PrerenderManager* manager,
    content::WebContents* target_contents,
    PrerenderData* prerender_data,
    const GURL& url,
    bool should_replace_current_entry)
    : content::WebContentsObserver(target_contents),
      manager_(manager),
      prerender_data_(prerender_data),
      url_(url),
      should_replace_current_entry_(should_replace_current_entry),
      start_time_(base::TimeTicks::Now()),
      seen_target_route_id_(false),
      swap_successful_(false),
      weak_factory_(this) {
}

PrerenderManager::PendingSwap::~PendingSwap() {
  manager_->prerender_tracker()->RemovePrerenderPendingSwap(
      target_route_id_, swap_successful_);
}

WebContents* PrerenderManager::PendingSwap::target_contents() const {
  return web_contents();
}

void PrerenderManager::PendingSwap::BeginSwap() {
  if (g_hang_session_storage_merges_for_testing)
    return;

  SessionStorageNamespace* target_namespace =
      target_contents()->GetController().GetDefaultSessionStorageNamespace();
  SessionStorageNamespace* prerender_namespace =
      prerender_data_->contents()->GetSessionStorageNamespace();

  prerender_namespace->Merge(
      true, prerender_data_->contents()->child_id(),
      target_namespace,
      base::Bind(&PrerenderManager::PendingSwap::OnMergeCompleted,
                 weak_factory_.GetWeakPtr()));

  merge_timeout_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(
          kSessionStorageNamespaceMergeTimeoutMs),
      this, &PrerenderManager::PendingSwap::OnMergeTimeout);
}

void PrerenderManager::PendingSwap::AboutToNavigateRenderView(
    RenderViewHost* render_view_host) {
  if (seen_target_route_id_) {
    // A second navigation began browser-side.
    prerender_data_->ClearPendingSwap();
    return;
  }

  seen_target_route_id_ = true;
  target_route_id_ = PrerenderTracker::ChildRouteIdPair(
      render_view_host->GetMainFrame()->GetProcess()->GetID(),
      render_view_host->GetMainFrame()->GetRoutingID());
  manager_->prerender_tracker()->AddPrerenderPendingSwap(
      target_route_id_, url_);
}

void PrerenderManager::PendingSwap::ProvisionalChangeToMainFrameUrl(
        const GURL& url,
        content::RenderFrameHost* render_frame_host) {
  // We must only cancel the pending swap if the |url| navigated to is not
  // the URL being attempted to be swapped in. That's because in the normal
  // flow, a ProvisionalChangeToMainFrameUrl will happen for the URL attempted
  // to be swapped in immediately after the pending swap has issued its merge.
  if (url != url_)
    prerender_data_->ClearPendingSwap();
}

void PrerenderManager::PendingSwap::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    content::PageTransition transition_type) {
  if (render_frame_host->GetParent())
    return;
  prerender_data_->ClearPendingSwap();
}

void PrerenderManager::PendingSwap::DidFailProvisionalLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  if (render_frame_host->GetParent())
    return;
  prerender_data_->ClearPendingSwap();
}

void PrerenderManager::PendingSwap::WebContentsDestroyed() {
  prerender_data_->ClearPendingSwap();
}

void PrerenderManager::PendingSwap::RecordEvent(PrerenderEvent event) const {
  manager_->RecordEvent(prerender_data_->contents(), event);
}

void PrerenderManager::PendingSwap::OnMergeCompleted(
    SessionStorageNamespace::MergeResult result) {
  UMA_HISTOGRAM_TIMES("Prerender.SessionStorageNamespaceMergeTime",
                      base::TimeTicks::Now() - start_time_);
  RecordEvent(PRERENDER_EVENT_MERGE_RESULT_MERGE_DONE);

  // Log the exact merge result in a histogram.
  switch (result) {
    case SessionStorageNamespace::MERGE_RESULT_NAMESPACE_NOT_FOUND:
      RecordEvent(PRERENDER_EVENT_MERGE_RESULT_RESULT_NAMESPACE_NOT_FOUND);
      break;
    case SessionStorageNamespace::MERGE_RESULT_NAMESPACE_NOT_ALIAS:
      RecordEvent(PRERENDER_EVENT_MERGE_RESULT_RESULT_NAMESPACE_NOT_ALIAS);
      break;
    case SessionStorageNamespace::MERGE_RESULT_NOT_LOGGING:
      RecordEvent(PRERENDER_EVENT_MERGE_RESULT_RESULT_NOT_LOGGING);
      break;
    case SessionStorageNamespace::MERGE_RESULT_NO_TRANSACTIONS:
      RecordEvent(PRERENDER_EVENT_MERGE_RESULT_RESULT_NO_TRANSACTIONS);
      break;
    case SessionStorageNamespace::MERGE_RESULT_TOO_MANY_TRANSACTIONS:
      RecordEvent(PRERENDER_EVENT_MERGE_RESULT_RESULT_TOO_MANY_TRANSACTIONS);
      break;
    case SessionStorageNamespace::MERGE_RESULT_NOT_MERGEABLE:
      RecordEvent(PRERENDER_EVENT_MERGE_RESULT_RESULT_NOT_MERGEABLE);
      break;
    case SessionStorageNamespace::MERGE_RESULT_MERGEABLE:
      RecordEvent(PRERENDER_EVENT_MERGE_RESULT_RESULT_MERGEABLE);
      break;
    default:
      NOTREACHED();
  }

  if (result != SessionStorageNamespace::MERGE_RESULT_MERGEABLE &&
      result != SessionStorageNamespace::MERGE_RESULT_NO_TRANSACTIONS) {
    RecordEvent(PRERENDER_EVENT_MERGE_RESULT_MERGE_FAILED);
    prerender_data_->ClearPendingSwap();
    return;
  }

  RecordEvent(PRERENDER_EVENT_MERGE_RESULT_SWAPPING_IN);

  // Note that SwapInternal will, on success, delete |prerender_data_| and
  // |this|. It will also delete |this| in some failure cases. Pass in a new
  // GURL object rather than a reference to |url_|. Also hold on to |manager_|
  // and |prerender_data_|.
  //
  // TODO(davidben): Can we make this less fragile?
  PrerenderManager* manager = manager_;
  PrerenderData* prerender_data = prerender_data_;
  WebContents* new_web_contents = manager_->SwapInternal(
        GURL(url_), target_contents(), prerender_data_,
        should_replace_current_entry_);
  if (!new_web_contents) {
    manager->RecordEvent(prerender_data->contents(),
                         PRERENDER_EVENT_MERGE_RESULT_SWAPIN_FAILED);
    // Depending on whether SwapInternal called Destroy() or simply failed to
    // swap, |this| may or may not be deleted. Either way, if the swap failed,
    // |prerender_data| is deleted asynchronously, so this call is a no-op if
    // |this| is already gone.
    prerender_data->ClearPendingSwap();
  }
}

void PrerenderManager::PendingSwap::OnMergeTimeout() {
  RecordEvent(PRERENDER_EVENT_MERGE_RESULT_TIMED_OUT);
  prerender_data_->ClearPendingSwap();
}

void PrerenderManager::SetPrerenderContentsFactory(
    PrerenderContents::Factory* prerender_contents_factory) {
  DCHECK(CalledOnValidThread());
  prerender_contents_factory_.reset(prerender_contents_factory);
}

void PrerenderManager::SourceNavigatedAway(PrerenderData* prerender_data) {
  // The expiry time of our prerender data will likely change because of
  // this navigation. This requires a resort of active_prerenders_.
  ScopedVector<PrerenderData>::iterator it =
      std::find(active_prerenders_.begin(), active_prerenders_.end(),
                prerender_data);
  if (it == active_prerenders_.end())
    return;

  (*it)->set_expiry_time(
      std::min((*it)->expiry_time(),
               GetExpiryTimeForNavigatedAwayPrerender()));
  SortActivePrerenders();
}

net::URLRequestContextGetter* PrerenderManager::GetURLRequestContext() {
  return content::BrowserContext::GetDefaultStoragePartition(profile_)->
      GetURLRequestContext();
}


// private
PrerenderHandle* PrerenderManager::AddPrerender(
    Origin origin,
    int process_id,
    const GURL& url_arg,
    const content::Referrer& referrer,
    const gfx::Size& size,
    SessionStorageNamespace* session_storage_namespace) {
  DCHECK(CalledOnValidThread());

  if (!IsEnabled())
    return NULL;

  if ((origin == ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN ||
       origin == ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN) &&
      IsGoogleSearchResultURL(referrer.url)) {
    origin = ORIGIN_GWS_PRERENDER;
  }

  GURL url = url_arg;
  GURL alias_url;
  uint8 experiment = GetQueryStringBasedExperiment(url_arg);
  if (IsControlGroup(experiment) &&
      MaybeGetQueryStringBasedAliasURL(url, &alias_url)) {
    url = alias_url;
  }

  // From here on, we will record a FinalStatus so we need to register with the
  // histogram tracking.
  histograms_->RecordPrerender(origin, url_arg);

  if (PrerenderData* preexisting_prerender_data =
          FindPrerenderData(url, session_storage_namespace)) {
    RecordFinalStatusWithoutCreatingPrerenderContents(
        url, origin, experiment, FINAL_STATUS_DUPLICATE);
    return new PrerenderHandle(preexisting_prerender_data);
  }

  // Do not prerender if there are too many render processes, and we would
  // have to use an existing one.  We do not want prerendering to happen in
  // a shared process, so that we can always reliably lower the CPU
  // priority for prerendering.
  // In single-process mode, ShouldTryToUseExistingProcessHost() always returns
  // true, so that case needs to be explicitly checked for.
  // TODO(tburkard): Figure out how to cancel prerendering in the opposite
  // case, when a new tab is added to a process used for prerendering.
  // TODO(ppi): Check whether there are usually enough render processes
  // available on Android. If not, kill an existing renderers so that we can
  // create a new one.
  if (content::RenderProcessHost::ShouldTryToUseExistingProcessHost(
          profile_, url) &&
      !content::RenderProcessHost::run_renderer_in_process()) {
    RecordFinalStatusWithoutCreatingPrerenderContents(
        url, origin, experiment, FINAL_STATUS_TOO_MANY_PROCESSES);
    return NULL;
  }

  // Check if enough time has passed since the last prerender.
  if (!DoesRateLimitAllowPrerender(origin)) {
    // Cancel the prerender. We could add it to the pending prerender list but
    // this doesn't make sense as the next prerender request will be triggered
    // by a navigation and is unlikely to be the same site.
    RecordFinalStatusWithoutCreatingPrerenderContents(
        url, origin, experiment, FINAL_STATUS_RATE_LIMIT_EXCEEDED);
    return NULL;
  }

  if (IsPrerenderCookieStoreEnabled() && !cookie_store_loaded()) {
    // Only prerender if the cookie store for this profile has been loaded.
    // This is required by PrerenderCookieMonster.
    RecordFinalStatusWithoutCreatingPrerenderContents(
        url, origin, experiment, FINAL_STATUS_COOKIE_STORE_NOT_LOADED);
    return NULL;
  }

  PrerenderContents* prerender_contents = CreatePrerenderContents(
      url, referrer, origin, experiment);
  DCHECK(prerender_contents);
  active_prerenders_.push_back(
      new PrerenderData(this, prerender_contents,
                        GetExpiryTimeForNewPrerender(origin)));
  if (!prerender_contents->Init()) {
    DCHECK(active_prerenders_.end() ==
           FindIteratorForPrerenderContents(prerender_contents));
    return NULL;
  }

  histograms_->RecordPrerenderStarted(origin);
  DCHECK(!prerender_contents->prerendering_has_started());

  PrerenderHandle* prerender_handle =
      new PrerenderHandle(active_prerenders_.back());
  SortActivePrerenders();

  last_prerender_start_time_ = GetCurrentTimeTicks();

  gfx::Size contents_size =
      size.IsEmpty() ? config_.default_tab_bounds.size() : size;

  net::URLRequestContextGetter* request_context =
      (IsPrerenderCookieStoreEnabled() ? GetURLRequestContext() : NULL);

  prerender_contents->StartPrerendering(process_id, contents_size,
                                        session_storage_namespace,
                                        request_context);

  DCHECK(IsControlGroup(experiment) ||
         prerender_contents->prerendering_has_started() ||
         (origin == ORIGIN_LOCAL_PREDICTOR &&
          IsLocalPredictorPrerenderAlwaysControlEnabled()));

  if (GetMode() == PRERENDER_MODE_EXPERIMENT_MULTI_PRERENDER_GROUP)
    histograms_->RecordConcurrency(active_prerenders_.size());

  // Query the history to see if the URL being prerendered has ever been
  // visited before.
  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
  if (history_service) {
    history_service->QueryURL(
        url,
        false,
        base::Bind(&PrerenderManager::OnHistoryServiceDidQueryURL,
                   base::Unretained(this),
                   origin,
                   experiment),
        &query_url_tracker_);
  }

  StartSchedulingPeriodicCleanups();
  return prerender_handle;
}

void PrerenderManager::StartSchedulingPeriodicCleanups() {
  DCHECK(CalledOnValidThread());
  if (repeating_timer_.IsRunning())
    return;
  repeating_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kPeriodicCleanupIntervalMs),
      this,
      &PrerenderManager::PeriodicCleanup);
}

void PrerenderManager::StopSchedulingPeriodicCleanups() {
  DCHECK(CalledOnValidThread());
  repeating_timer_.Stop();
}

void PrerenderManager::PeriodicCleanup() {
  DCHECK(CalledOnValidThread());

  base::ElapsedTimer resource_timer;

  // Grab a copy of the current PrerenderContents pointers, so that we
  // will not interfere with potential deletions of the list.
  std::vector<PrerenderContents*>
      prerender_contents(active_prerenders_.size());
  std::transform(active_prerenders_.begin(), active_prerenders_.end(),
                 prerender_contents.begin(),
                 std::mem_fun(&PrerenderData::contents));

  // And now check for prerenders using too much memory.
  std::for_each(prerender_contents.begin(), prerender_contents.end(),
                std::mem_fun(
                    &PrerenderContents::DestroyWhenUsingTooManyResources));

  // Measure how long the resource checks took. http://crbug.com/305419.
  UMA_HISTOGRAM_TIMES("Prerender.PeriodicCleanupResourceCheckTime",
                      resource_timer.Elapsed());

  base::ElapsedTimer cleanup_timer;

  // Perform deferred cleanup work.
  DeleteOldWebContents();
  DeleteOldEntries();
  if (active_prerenders_.empty())
    StopSchedulingPeriodicCleanups();

  to_delete_prerenders_.clear();

  // Measure how long a the various cleanup tasks took. http://crbug.com/305419.
  UMA_HISTOGRAM_TIMES("Prerender.PeriodicCleanupDeleteContentsTime",
                      cleanup_timer.Elapsed());
}

void PrerenderManager::PostCleanupTask() {
  DCHECK(CalledOnValidThread());
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PrerenderManager::PeriodicCleanup, AsWeakPtr()));
}

base::TimeTicks PrerenderManager::GetExpiryTimeForNewPrerender(
    Origin origin) const {
  base::TimeDelta ttl = config_.time_to_live;
  if (origin == ORIGIN_LOCAL_PREDICTOR)
    ttl = base::TimeDelta::FromSeconds(GetLocalPredictorTTLSeconds());
  return GetCurrentTimeTicks() + ttl;
}

base::TimeTicks PrerenderManager::GetExpiryTimeForNavigatedAwayPrerender()
    const {
  return GetCurrentTimeTicks() + config_.abandon_time_to_live;
}

void PrerenderManager::DeleteOldEntries() {
  DCHECK(CalledOnValidThread());
  while (!active_prerenders_.empty()) {
    PrerenderData* prerender_data = active_prerenders_.front();
    DCHECK(prerender_data);
    DCHECK(prerender_data->contents());

    if (prerender_data->expiry_time() > GetCurrentTimeTicks())
      return;
    prerender_data->contents()->Destroy(FINAL_STATUS_TIMED_OUT);
  }
}

base::Time PrerenderManager::GetCurrentTime() const {
  return base::Time::Now();
}

base::TimeTicks PrerenderManager::GetCurrentTimeTicks() const {
  return base::TimeTicks::Now();
}

PrerenderContents* PrerenderManager::CreatePrerenderContents(
    const GURL& url,
    const content::Referrer& referrer,
    Origin origin,
    uint8 experiment_id) {
  DCHECK(CalledOnValidThread());
  return prerender_contents_factory_->CreatePrerenderContents(
      this, profile_, url, referrer, origin, experiment_id);
}

void PrerenderManager::SortActivePrerenders() {
  std::sort(active_prerenders_.begin(), active_prerenders_.end(),
            PrerenderData::OrderByExpiryTime());
}

PrerenderManager::PrerenderData* PrerenderManager::FindPrerenderData(
    const GURL& url,
    const SessionStorageNamespace* session_storage_namespace) {
  for (ScopedVector<PrerenderData>::iterator it = active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    if ((*it)->contents()->Matches(url, session_storage_namespace))
      return *it;
  }
  return NULL;
}

PrerenderManager::PrerenderData*
PrerenderManager::FindPrerenderDataForTargetContents(
    WebContents* target_contents) {
  for (ScopedVector<PrerenderData>::iterator it = active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    if ((*it)->pending_swap() &&
        (*it)->pending_swap()->target_contents() == target_contents)
      return *it;
  }
  return NULL;
}

ScopedVector<PrerenderManager::PrerenderData>::iterator
PrerenderManager::FindIteratorForPrerenderContents(
    PrerenderContents* prerender_contents) {
  for (ScopedVector<PrerenderData>::iterator it = active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    if (prerender_contents == (*it)->contents())
      return it;
  }
  return active_prerenders_.end();
}

bool PrerenderManager::DoesRateLimitAllowPrerender(Origin origin) const {
  DCHECK(CalledOnValidThread());
  base::TimeDelta elapsed_time =
      GetCurrentTimeTicks() - last_prerender_start_time_;
  histograms_->RecordTimeBetweenPrerenderRequests(origin, elapsed_time);
  if (!config_.rate_limit_enabled)
    return true;
  // The LocalPredictor may issue multiple prerenders simultaneously (if so
  // configured), so no throttling.
  if (origin == ORIGIN_LOCAL_PREDICTOR)
    return true;
  return elapsed_time >=
      base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs);
}

void PrerenderManager::DeleteOldWebContents() {
  while (!old_web_contents_list_.empty()) {
    WebContents* web_contents = old_web_contents_list_.front();
    old_web_contents_list_.pop_front();
    // TODO(dominich): should we use Instant Unload Handler here?
    delete web_contents;
  }
}

void PrerenderManager::CleanUpOldNavigations() {
  DCHECK(CalledOnValidThread());

  // Cutoff.  Navigations before this cutoff can be discarded.
  base::TimeTicks cutoff = GetCurrentTimeTicks() -
      base::TimeDelta::FromMilliseconds(kNavigationRecordWindowMs);
  while (!navigations_.empty()) {
    if (navigations_.front().time > cutoff)
      break;
    navigations_.pop_front();
  }
}

void PrerenderManager::ScheduleDeleteOldWebContents(
    WebContents* tab,
    OnCloseWebContentsDeleter* deleter) {
  old_web_contents_list_.push_back(tab);
  PostCleanupTask();

  if (deleter) {
    ScopedVector<OnCloseWebContentsDeleter>::iterator i = std::find(
        on_close_web_contents_deleters_.begin(),
        on_close_web_contents_deleters_.end(),
        deleter);
    DCHECK(i != on_close_web_contents_deleters_.end());
    on_close_web_contents_deleters_.erase(i);
  }
}

void PrerenderManager::AddToHistory(PrerenderContents* contents) {
  PrerenderHistory::Entry entry(contents->prerender_url(),
                                contents->final_status(),
                                contents->origin(),
                                base::Time::Now());
  prerender_history_->AddEntry(entry);
}

base::Value* PrerenderManager::GetActivePrerendersAsValue() const {
  base::ListValue* list_value = new base::ListValue();
  for (ScopedVector<PrerenderData>::const_iterator it =
           active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    if (base::Value* prerender_value = (*it)->contents()->GetAsValue())
      list_value->Append(prerender_value);
  }
  return list_value;
}

void PrerenderManager::DestroyAllContents(FinalStatus final_status) {
  DeleteOldWebContents();
  while (!active_prerenders_.empty()) {
    PrerenderContents* contents = active_prerenders_.front()->contents();
    contents->Destroy(final_status);
  }
  to_delete_prerenders_.clear();
}

void PrerenderManager::DestroyAndMarkMatchCompleteAsUsed(
    PrerenderContents* prerender_contents,
    FinalStatus final_status) {
  prerender_contents->set_match_complete_status(
      PrerenderContents::MATCH_COMPLETE_REPLACED);
  histograms_->RecordFinalStatus(prerender_contents->origin(),
                                 prerender_contents->experiment_id(),
                                 PrerenderContents::MATCH_COMPLETE_REPLACEMENT,
                                 FINAL_STATUS_WOULD_HAVE_BEEN_USED);
  prerender_contents->Destroy(final_status);
}

void PrerenderManager::RecordFinalStatusWithoutCreatingPrerenderContents(
    const GURL& url, Origin origin, uint8 experiment_id,
    FinalStatus final_status) const {
  PrerenderHistory::Entry entry(url, final_status, origin, base::Time::Now());
  prerender_history_->AddEntry(entry);
  RecordFinalStatusWithMatchCompleteStatus(
      origin, experiment_id,
      PrerenderContents::MATCH_COMPLETE_DEFAULT,
      final_status);
}

void PrerenderManager::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_COOKIE_CHANGED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile || !profile_->IsSameProfile(profile) ||
          profile->IsOffTheRecord()) {
        return;
      }
      CookieChanged(content::Details<ChromeCookieDetails>(details).ptr());
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
      DestroyAllContents(FINAL_STATUS_PROFILE_DESTROYED);
      on_close_web_contents_deleters_.clear();
      break;
    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}

void PrerenderManager::OnCreatingAudioStream(int render_process_id,
                                             int render_frame_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  WebContents* tab = WebContents::FromRenderFrameHost(render_frame_host);
  if (!tab)
    return;

  PrerenderContents* prerender_contents = GetPrerenderContents(tab);
  if (!prerender_contents)
    return;

  prerender_contents->Destroy(prerender::FINAL_STATUS_CREATING_AUDIO_STREAM);
}

void PrerenderManager::RecordLikelyLoginOnURL(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!url.SchemeIsHTTPOrHTTPS())
    return;
  if (logged_in_predictor_table_.get()) {
    BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        base::Bind(&LoggedInPredictorTable::AddDomainFromURL,
                   logged_in_predictor_table_,
                   url));
  }
  std::string key = LoggedInPredictorTable::GetKey(url);
  if (!logged_in_state_.get())
    return;
  if (logged_in_state_->count(key))
    return;
  (*logged_in_state_)[key] = base::Time::Now().ToInternalValue();
}

void PrerenderManager::CheckIfLikelyLoggedInOnURL(
    const GURL& url,
    bool* lookup_result,
    bool* database_was_present,
    const base::Closure& result_cb) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!logged_in_predictor_table_.get()) {
    *database_was_present = false;
    *lookup_result = false;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, result_cb);
    return;
  }
  BrowserThread::PostTaskAndReply(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&LoggedInPredictorTable::HasUserLoggedIn,
                 logged_in_predictor_table_,
                 url,
                 lookup_result,
                 database_was_present),
      result_cb);
}


void PrerenderManager::CookieChanged(ChromeCookieDetails* details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!logged_in_predictor_table_.get())
    return;

  // We only care when a cookie has been removed.
  if (!details->removed)
    return;

  std::string domain_key =
      LoggedInPredictorTable::GetKeyFromDomain(details->cookie->Domain());

  // If we have no record of this domain as a potentially logged in domain,
  // nothing to do here.
  if (logged_in_state_.get() && logged_in_state_->count(domain_key) < 1)
    return;

  net::URLRequestContextGetter* rq_context = profile_->GetRequestContext();
  if (!rq_context)
    return;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CheckIfCookiesExistForDomainOnIOThread,
                 base::Unretained(rq_context),
                 domain_key,
                 base::Bind(
                     &PrerenderManager::CookieChangedAnyCookiesLeftLookupResult,
                     AsWeakPtr(),
                     domain_key)
                 ));
}

void PrerenderManager::CookieChangedAnyCookiesLeftLookupResult(
    const std::string& domain_key,
    bool cookies_exist) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (cookies_exist)
    return;

  if (logged_in_predictor_table_.get()) {
    BrowserThread::PostTask(BrowserThread::DB,
                            FROM_HERE,
                            base::Bind(&LoggedInPredictorTable::DeleteDomain,
                                       logged_in_predictor_table_,
                                       domain_key));
  }

  if (logged_in_state_.get())
    logged_in_state_->erase(domain_key);
}

void PrerenderManager::LoggedInPredictorDataReceived(
    scoped_ptr<LoggedInStateMap> new_map) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  logged_in_state_.swap(new_map);
}

void PrerenderManager::RecordEvent(PrerenderContents* contents,
                                   PrerenderEvent event) const {
  if (!contents)
    histograms_->RecordEvent(ORIGIN_NONE, kNoExperiment, event);
  else
    histograms_->RecordEvent(contents->origin(), contents->experiment_id(),
                             event);
}

// static
void PrerenderManager::RecordCookieEvent(int process_id,
                                         int frame_id,
                                         const GURL& url,
                                         const GURL& frame_url,
                                         bool is_for_blocking_resource,
                                         PrerenderContents::CookieEvent event,
                                         const net::CookieList* cookie_list) {
  RenderFrameHost* rfh = RenderFrameHost::FromID(process_id, frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;

  bool is_main_frame = (rfh == web_contents->GetMainFrame());

  bool is_third_party_cookie =
    (!frame_url.is_empty() &&
     !net::registry_controlled_domains::SameDomainOrHost(
         url, frame_url,
         net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));

  PrerenderContents* prerender_contents =
      PrerenderContents::FromWebContents(web_contents);

  if (!prerender_contents)
    return;

  base::Time earliest_create_date;
  if (event == PrerenderContents::COOKIE_EVENT_SEND) {
    if (!cookie_list || cookie_list->empty())
      return;
    for (size_t i = 0; i < cookie_list->size(); i++) {
      if (earliest_create_date.is_null() ||
          (*cookie_list)[i].CreationDate() < earliest_create_date) {
        earliest_create_date = (*cookie_list)[i].CreationDate();
      }
    }
  }

  prerender_contents->RecordCookieEvent(event,
                                        is_main_frame && url == frame_url,
                                        is_third_party_cookie,
                                        is_for_blocking_resource,
                                        earliest_create_date);
}

void PrerenderManager::RecordCookieStatus(Origin origin,
                                          uint8 experiment_id,
                                          int cookie_status) const {
  histograms_->RecordCookieStatus(origin, experiment_id, cookie_status);
}

void PrerenderManager::RecordCookieSendType(Origin origin,
                                            uint8 experiment_id,
                                            int cookie_send_type) const {
  histograms_->RecordCookieSendType(origin, experiment_id, cookie_send_type);
}

void PrerenderManager::OnHistoryServiceDidQueryURL(
    Origin origin,
    uint8 experiment_id,
    bool success,
    const history::URLRow& url_row,
    const history::VisitVector& /*visits*/) {
  histograms_->RecordPrerenderPageVisitedStatus(origin, experiment_id, success);
}

// static
void PrerenderManager::HangSessionStorageMergesForTesting() {
  g_hang_session_storage_merges_for_testing = true;
}

void PrerenderManager::RecordNetworkBytes(Origin origin,
                                          bool used,
                                          int64 prerender_bytes) {
  if (!ActuallyPrerendering())
    return;
  int64 recent_profile_bytes =
      profile_network_bytes_ - last_recorded_profile_network_bytes_;
  last_recorded_profile_network_bytes_ = profile_network_bytes_;
  DCHECK_GE(recent_profile_bytes, 0);
  histograms_->RecordNetworkBytes(
      origin, used, prerender_bytes, recent_profile_bytes);
}

bool PrerenderManager::IsEnabled() const {
  DCHECK(CalledOnValidThread());

  // TODO(bnc): remove conditional as per crbug.com/334602.
  if (profile_ && profile_->GetPrefs() &&
        profile_->GetPrefs()->GetInteger(prefs::kNetworkPredictionOptions) !=
        chrome_browser_net::NETWORK_PREDICTION_UNSET) {
    return chrome_browser_net::CanPredictNetworkActionsUI(profile_->GetPrefs());
  }
  // TODO(bnc): remove rest of method as per crbug.com/334602.
  if (!enabled_)
    return false;
  for (std::list<const PrerenderCondition*>::const_iterator it =
           prerender_conditions_.begin();
       it != prerender_conditions_.end();
       ++it) {
    const PrerenderCondition* condition = *it;
    if (!condition->CanPrerender())
      return false;
  }
  return true;
}

void PrerenderManager::AddProfileNetworkBytesIfEnabled(int64 bytes) {
  DCHECK_GE(bytes, 0);
  if (IsEnabled() && ActuallyPrerendering())
    profile_network_bytes_ += bytes;
}

void PrerenderManager::OnCookieStoreLoaded() {
  cookie_store_loaded_ = true;
  if (!on_cookie_store_loaded_cb_for_testing_.is_null())
    on_cookie_store_loaded_cb_for_testing_.Run();
}

void PrerenderManager::AddPrerenderProcessHost(
    content::RenderProcessHost* process_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prerender_process_hosts_.find(process_host) ==
         prerender_process_hosts_.end());
  prerender_process_hosts_.insert(process_host);
  process_host->AddObserver(this);
}

bool PrerenderManager::MayReuseProcessHost(
    content::RenderProcessHost* process_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If prerender cookie stores are disabled, there is no need to require
  // isolated prerender processes.
  if (!IsPrerenderCookieStoreEnabled())
    return true;
  return (prerender_process_hosts_.find(process_host) ==
          prerender_process_hosts_.end());
}

void PrerenderManager::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prerender_process_hosts_.erase(host);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PrerenderTracker::RemovePrerenderCookieStoreOnIOThread,
                 base::Unretained(prerender_tracker()), host->GetID(), false));
}

}  // namespace prerender
