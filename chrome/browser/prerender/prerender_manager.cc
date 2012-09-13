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
#include "base/stl_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
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
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/prerender_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::SessionStorageNamespace;
using content::WebContents;

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
      final_status != FINAL_STATUS_EVICTED &&
      final_status != FINAL_STATUS_MANAGER_SHUTDOWN &&
      final_status != FINAL_STATUS_APP_TERMINATING &&
      final_status != FINAL_STATUS_WINDOW_OPENER &&
      final_status != FINAL_STATUS_CACHE_OR_HISTORY_CLEARED &&
      final_status != FINAL_STATUS_CANCELLED &&
      final_status != FINAL_STATUS_DEVTOOLS_ATTACHED &&
      final_status != FINAL_STATUS_CROSS_SITE_NAVIGATION_PENDING;
}

}  // namespace

class PrerenderManager::OnCloseTabContentsDeleter
    : public content::WebContentsDelegate,
      public base::SupportsWeakPtr<
          PrerenderManager::OnCloseTabContentsDeleter> {
 public:
  OnCloseTabContentsDeleter(PrerenderManager* manager,
                            TabContents* tab)
      : manager_(manager),
        tab_(tab) {
    tab_->web_contents()->SetDelegate(this);
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        base::Bind(&OnCloseTabContentsDeleter::ScheduleTabContentsForDeletion,
                   this->AsWeakPtr(), true),
        base::TimeDelta::FromSeconds(kDeleteWithExtremePrejudiceSeconds));
  }

  virtual void CloseContents(WebContents* source) OVERRIDE {
    DCHECK_EQ(tab_->web_contents(), source);
    ScheduleTabContentsForDeletion(false);
  }

  virtual void SwappedOut(WebContents* source) OVERRIDE {
    DCHECK_EQ(tab_->web_contents(), source);
    ScheduleTabContentsForDeletion(false);
  }

  virtual bool ShouldSuppressDialogs() OVERRIDE {
    return true;
  }

 private:
  static const int kDeleteWithExtremePrejudiceSeconds = 3;

  void ScheduleTabContentsForDeletion(bool timeout) {
    tab_->web_contents()->SetDelegate(NULL);
    manager_->ScheduleDeleteOldTabContents(tab_.release(), this);
    UMA_HISTOGRAM_BOOLEAN("Prerender.TabContentsDeleterTimeout", timeout);
  }

  PrerenderManager* manager_;
  scoped_ptr<TabContents> tab_;

  DISALLOW_COPY_AND_ASSIGN(OnCloseTabContentsDeleter);
};

// static
bool PrerenderManager::is_prefetch_enabled_ = false;

// static
int PrerenderManager::prerenders_per_session_count_ = 0;

// static
PrerenderManager::PrerenderManagerMode PrerenderManager::mode_ =
    PRERENDER_MODE_ENABLED;

struct PrerenderManager::NavigationRecord {
  GURL url_;
  base::TimeTicks time_;
  NavigationRecord(const GURL& url, base::TimeTicks time)
      : url_(url),
        time_(time) {
  }
};

PrerenderManager::PrerenderManager(Profile* profile,
                                   PrerenderTracker* prerender_tracker)
    : enabled_(true),
      profile_(profile),
      prerender_tracker_(prerender_tracker),
      prerender_contents_factory_(PrerenderContents::CreateFactory()),
      last_prerender_start_time_(GetCurrentTimeTicks() -
          base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs)),
      weak_factory_(this),
      prerender_history_(new PrerenderHistory(kHistoryLength)),
      histograms_(new PrerenderHistograms()) {
  // There are some assumptions that the PrerenderManager is on the UI thread.
  // Any other checks simply make sure that the PrerenderManager is accessed on
  // the same thread that it was created on.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Certain experiments override our default config_ values.
  switch (PrerenderManager::GetMode()) {
    case PrerenderManager::PRERENDER_MODE_EXPERIMENT_MULTI_PRERENDER_GROUP:
      config_.max_concurrency = 3;
      break;
    case PrerenderManager::PRERENDER_MODE_EXPERIMENT_15MIN_TTL_GROUP:
      config_.time_to_live = base::TimeDelta::FromMinutes(15);
      break;
    default:
      break;
  }
}

PrerenderManager::~PrerenderManager() {
}

void PrerenderManager::Shutdown() {
  DoShutdown();
}

PrerenderHandle* PrerenderManager::AddPrerenderFromLinkRelPrerender(
    int process_id,
    int route_id,
    const GURL& url,
    const content::Referrer& referrer,
    const gfx::Size& size) {
#if defined(OS_ANDROID)
  // TODO(jcivelli): http://crbug.com/113322 We should have an option to disable
  //                link-prerender and enable omnibox-prerender only.
  return NULL;
#else
  DCHECK(!size.IsEmpty());
  if (PrerenderData* parent_prerender_data =
          FindPrerenderDataForChildAndRoute(process_id, route_id)) {
    // Instead of prerendering from inside of a running prerender, we will defer
    // this request until its launcher is made visible.
    if (PrerenderContents* contents = parent_prerender_data->contents_) {
      pending_prerender_list_.push_back(
          linked_ptr<PrerenderData>(new PrerenderData(this)));
      PrerenderHandle* prerender_handle =
          new PrerenderHandle(pending_prerender_list_.back().get());
      contents->AddPendingPrerender(
          prerender_handle->weak_ptr_factory_.GetWeakPtr(),
          url, referrer, size);
      return prerender_handle;
    }
  }

  // Unit tests pass in a process_id == -1.
  SessionStorageNamespace* session_storage_namespace = NULL;
  if (process_id != -1) {
    RenderViewHost* source_render_view_host =
        RenderViewHost::FromID(process_id, route_id);
    if (!source_render_view_host)
      return NULL;
    WebContents* source_web_contents =
        WebContents::FromRenderViewHost(source_render_view_host);
    if (!source_web_contents)
      return NULL;
    // TODO(ajwong): This does not correctly handle storage for isolated apps.
    session_storage_namespace =
        source_web_contents->GetController()
            .GetDefaultSessionStorageNamespace();
  }

  return AddPrerender(ORIGIN_LINK_REL_PRERENDER,
                      process_id, url, referrer, size,
                      session_storage_namespace);
#endif
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

void PrerenderManager::DestroyPrerenderForRenderView(
    int process_id, int view_id, FinalStatus final_status) {
  DCHECK(CalledOnValidThread());
  if (PrerenderData* prerender_data =
          FindPrerenderDataForChildAndRoute(process_id, view_id)) {
    prerender_data->contents_->Destroy(final_status);
  }
}

void PrerenderManager::CancelAllPrerenders() {
  DCHECK(CalledOnValidThread());
  while (!active_prerender_list_.empty()) {
    PrerenderContents* prerender_contents =
        active_prerender_list_.front()->contents();
    prerender_contents->Destroy(FINAL_STATUS_CANCELLED);
  }
}

bool PrerenderManager::MaybeUsePrerenderedPage(WebContents* web_contents,
                                               const GURL& url) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsWebContentsPrerendering(web_contents));

  DeleteOldEntries();
  DeletePendingDeleteEntries();
  // TODO(ajwong): This doesn't handle isolated apps correctly.
  PrerenderData* prerender_data = FindPrerenderData(
          url,
          web_contents->GetController().GetDefaultSessionStorageNamespace());
  if (!prerender_data)
    return false;
  DCHECK(prerender_data->contents_);
  if (IsNoSwapInExperiment(prerender_data->contents_->experiment_id()))
    return false;

  if (TabContents* new_tab_contents =
      prerender_data->contents_->prerender_contents()) {
    if (web_contents == new_tab_contents->web_contents())
      return false;  // Do not swap in to ourself.
  }

  scoped_ptr<PrerenderContents> prerender_contents(prerender_data->contents_);
  std::list<linked_ptr<PrerenderData> >::iterator to_erase =
      FindIteratorForPrerenderContents(prerender_contents.get());
  DCHECK(active_prerender_list_.end() != to_erase);
  DCHECK_EQ(prerender_data, to_erase->get());
  active_prerender_list_.erase(to_erase);

  // Do not use the prerendered version if there is an opener object.
  if (web_contents->HasOpener()) {
    prerender_contents.release()->Destroy(FINAL_STATUS_WINDOW_OPENER);
    return false;
  }

  // If we are just in the control group (which can be detected by noticing
  // that prerendering hasn't even started yet), record that |web_contents| now
  // would be showing a prerendered contents, but otherwise, don't do anything.
  if (!prerender_contents->prerendering_has_started()) {
    MarkWebContentsAsWouldBePrerendered(web_contents);
    prerender_contents.release()->Destroy(FINAL_STATUS_WOULD_HAVE_BEEN_USED);
    return false;
  }

  // Don't use prerendered pages if debugger is attached to the tab.
  // See http://crbug.com/98541
  if (content::DevToolsAgentHostRegistry::IsDebuggerAttached(web_contents)) {
    DestroyAndMarkMatchCompleteAsUsed(prerender_contents.release(),
                                      FINAL_STATUS_DEVTOOLS_ATTACHED);
    return false;
  }

  // If the prerendered page is in the middle of a cross-site navigation,
  // don't swap it in because there isn't a good way to merge histories.
  if (prerender_contents->IsCrossSiteNavigationPending()) {
    DestroyAndMarkMatchCompleteAsUsed(
        prerender_contents.release(),
        FINAL_STATUS_CROSS_SITE_NAVIGATION_PENDING);
    return false;
  }

  // For bookkeeping purposes, we need to mark this WebContents to
  // reflect that it would have been prerendered.
  if (GetMode() == PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP) {
    MarkWebContentsAsWouldBePrerendered(web_contents);
    prerender_contents.release()->Destroy(FINAL_STATUS_WOULD_HAVE_BEEN_USED);
    return false;
  }

  int child_id, route_id;
  CHECK(prerender_contents->GetChildId(&child_id));
  CHECK(prerender_contents->GetRouteId(&route_id));

  // Try to set the prerendered page as used, so any subsequent attempts to
  // cancel on other threads will fail.  If this fails because the prerender
  // was already cancelled, possibly on another thread, fail.
  if (!prerender_tracker_->TryUse(child_id, route_id))
    return false;

  // At this point, we've determined that we will use the prerender.

  if (!prerender_contents->load_start_time().is_null()) {
    histograms_->RecordTimeUntilUsed(
        GetCurrentTimeTicks() - prerender_contents->load_start_time(),
        config_.time_to_live);
  }

  histograms_->RecordPerSessionCount(++prerenders_per_session_count_);
  histograms_->RecordUsedPrerender(prerender_contents->origin());
  prerender_contents->set_final_status(FINAL_STATUS_USED);

  RenderViewHost* new_render_view_host =
      prerender_contents->prerender_contents()->web_contents()->
          GetRenderViewHost();
  new_render_view_host->Send(
      new PrerenderMsg_SetIsPrerendering(new_render_view_host->GetRoutingID(),
                                         false));

  // Start pending prerender requests from the PrerenderContents, if there are
  // any.
  prerender_contents->StartPendingPrerenders();

  TabContents* new_tab_contents =
      prerender_contents->ReleasePrerenderContents();
  TabContents* old_tab_contents = TabContents::FromWebContents(web_contents);
  DCHECK(new_tab_contents);
  DCHECK(old_tab_contents);

  MarkWebContentsAsPrerendered(new_tab_contents->web_contents());

  // Merge the browsing history.
  new_tab_contents->web_contents()->GetController().CopyStateFromAndPrune(
      &old_tab_contents->web_contents()->GetController());
  old_tab_contents->core_tab_helper()->delegate()->
      SwapTabContents(old_tab_contents, new_tab_contents);
  prerender_contents->CommitHistory(new_tab_contents);

  GURL icon_url = prerender_contents->icon_url();
  if (!icon_url.is_empty()) {
    std::vector<FaviconURL> urls;
    urls.push_back(FaviconURL(icon_url, FaviconURL::FAVICON));
    new_tab_contents->favicon_tab_helper()->OnUpdateFaviconURL(
        prerender_contents->page_id(),
        urls);
  }

  // Update PPLT metrics:
  // If the tab has finished loading, record a PPLT of 0.
  // If the tab is still loading, reset its start time to the current time.
  PrerenderTabHelper* prerender_tab_helper =
      new_tab_contents->prerender_tab_helper();
  DCHECK(prerender_tab_helper != NULL);
  prerender_tab_helper->PrerenderSwappedIn();

  if (old_tab_contents->web_contents()->NeedToFireBeforeUnload()) {
    // Schedule the delete to occur after the tab has run its unload handlers.
    on_close_tab_contents_deleters_.push_back(
        new OnCloseTabContentsDeleter(this, old_tab_contents));
    old_tab_contents->web_contents()->GetRenderViewHost()->
        FirePageBeforeUnload(false);
  } else {
    // No unload handler to run, so delete asap.
    ScheduleDeleteOldTabContents(old_tab_contents, NULL);
  }

  // TODO(cbentzel): Should prerender_contents move to the pending delete
  //                 list, instead of deleting directly here?
  AddToHistory(prerender_contents.get());
  RecordNavigation(url);
  return true;
}

void PrerenderManager::MoveEntryToPendingDelete(PrerenderContents* entry,
                                                FinalStatus final_status) {
  DCHECK(CalledOnValidThread());
  DCHECK(entry);
  // Confirm this entry has not already been moved to the pending delete list.
  DCHECK_EQ(0, std::count(pending_delete_list_.begin(),
                          pending_delete_list_.end(), entry));

  std::list<linked_ptr<PrerenderData> >::iterator it =
      FindIteratorForPrerenderContents(entry);

  // If this PrerenderContents is being deleted due to a cancellation,
  // we need to create a dummy replacement for PPLT accounting purposes
  // for the Match Complete group.
  // This is the case if the cancellation is for any reason that would not
  // occur in the control group case.
  if (it != active_prerender_list_.end()) {
    if (entry->match_complete_status() ==
            PrerenderContents::MATCH_COMPLETE_DEFAULT &&
        NeedMatchCompleteDummyForFinalStatus(final_status) &&
        ActuallyPrerendering()) {
      // TODO(tburkard): I'd like to DCHECK that we are actually prerendering.
      // However, what if new conditions are added and
      // NeedMatchCompleteDummyForFinalStatus, is not being updated.  Not sure
      // what's the best thing to do here.  For now, I will just check whether
      // we are actually prerendering.
      entry->set_match_complete_status(
          PrerenderContents::MATCH_COMPLETE_REPLACED);
      PrerenderContents* dummy_replacement_prerender_contents =
          CreatePrerenderContents(entry->prerender_url(), entry->referrer(),
                                  entry->origin(), entry->experiment_id());
      DCHECK(dummy_replacement_prerender_contents);
      dummy_replacement_prerender_contents->MakeIntoDummyReplacementOf(entry);

      dummy_replacement_prerender_contents->set_match_complete_status(
          PrerenderContents::MATCH_COMPLETE_REPLACEMENT_PENDING);
      DCHECK(dummy_replacement_prerender_contents->Init());
      dummy_replacement_prerender_contents->
          AddAliasURLsFromOtherPrerenderContents(entry);
      dummy_replacement_prerender_contents->set_match_complete_status(
          PrerenderContents::MATCH_COMPLETE_REPLACEMENT);

      it->get()->contents_ = dummy_replacement_prerender_contents;
    } else {
      active_prerender_list_.erase(it);
    }
  }

  AddToHistory(entry);
  pending_delete_list_.push_back(entry);

  // Destroy the old WebContents relatively promptly to reduce resource usage,
  // and in the case of HTML5 media, reduce the change of playing any sound.
  PostCleanupTask();
}

// static
void PrerenderManager::RecordPerceivedPageLoadTime(
    base::TimeDelta perceived_page_load_time,
    double fraction_plt_elapsed_at_swap_in,
    WebContents* web_contents,
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!prerender_manager)
    return;
  if (!prerender_manager->IsEnabled())
    return;
  bool was_prerender =
      prerender_manager->IsWebContentsPrerendered(web_contents);
  bool was_complete_prerender = was_prerender ||
      prerender_manager->WouldWebContentsBePrerendered(web_contents);
  if (prerender_manager->IsWebContentsPrerendering(web_contents)) {
    prerender_manager->histograms_->RecordPageLoadTimeNotSwappedIn(
        perceived_page_load_time, url);
  } else {
    prerender_manager->histograms_->RecordPerceivedPageLoadTime(
        perceived_page_load_time, was_prerender, was_complete_prerender, url);
    prerender_manager->histograms_->RecordPercentLoadDoneAtSwapin(
        fraction_plt_elapsed_at_swap_in);
    if (prerender_manager->local_predictor_.get()) {
      prerender_manager->local_predictor_->
          OnPLTEventForURL(url, perceived_page_load_time);
    }
  }
}

void PrerenderManager::set_enabled(bool enabled) {
  DCHECK(CalledOnValidThread());
  enabled_ = enabled;
}

// static
bool PrerenderManager::IsPrefetchEnabled() {
  return is_prefetch_enabled_;
}

// static
void PrerenderManager::SetIsPrefetchEnabled(bool value) {
  is_prefetch_enabled_ = value;
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
  };
  return "";
}

// static
bool PrerenderManager::IsPrerenderingPossible() {
  return GetMode() != PRERENDER_MODE_DISABLED;
}

// static
bool PrerenderManager::ActuallyPrerendering() {
  return IsPrerenderingPossible() && !IsControlGroup();
}

// static
bool PrerenderManager::IsControlGroup() {
  return GetMode() == PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP;
}

// static
bool PrerenderManager::IsNoUseGroup() {
  return GetMode() == PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP;
}

// static
bool PrerenderManager::IsWebContentsPrerendering(
    WebContents* web_contents) const {
  DCHECK(CalledOnValidThread());
  for (std::list<linked_ptr<PrerenderData> >::const_iterator it =
           active_prerender_list_.begin();
       it != active_prerender_list_.end();
       ++it) {
    TabContents* prerender_tab_contents =
        it->get()->contents_->prerender_contents();
    if (prerender_tab_contents &&
        prerender_tab_contents->web_contents() == web_contents) {
      return true;
    }
  }

  // Also look through the pending-deletion list.
  for (std::list<PrerenderContents*>::const_iterator it =
           pending_delete_list_.begin();
       it != pending_delete_list_.end();
       ++it) {
    TabContents* prerender_tab_contents = (*it)->prerender_contents();
    if (prerender_tab_contents &&
        prerender_tab_contents->web_contents() == web_contents)
      return true;
  }

  return false;
}

void PrerenderManager::MarkWebContentsAsPrerendered(WebContents* web_contents) {
  DCHECK(CalledOnValidThread());
  prerendered_tab_contents_set_.insert(web_contents);
}

void PrerenderManager::MarkWebContentsAsWouldBePrerendered(
    WebContents* web_contents) {
  DCHECK(CalledOnValidThread());
  would_be_prerendered_map_[web_contents] = true;
}

void PrerenderManager::MarkWebContentsAsNotPrerendered(
    WebContents* web_contents) {
  DCHECK(CalledOnValidThread());
  prerendered_tab_contents_set_.erase(web_contents);
  WouldBePrerenderedMap::iterator it =
      would_be_prerendered_map_.find(web_contents);
  if (it != would_be_prerendered_map_.end()) {
    bool first_time = it->second;
    if (first_time) {
      it->second = false;
    } else {
      would_be_prerendered_map_.erase(it);
    }
  }
}

bool PrerenderManager::IsWebContentsPrerendered(
    content::WebContents* web_contents) const {
  DCHECK(CalledOnValidThread());
  return prerendered_tab_contents_set_.count(web_contents) > 0;
}

bool PrerenderManager::WouldWebContentsBePrerendered(
    WebContents* web_contents) const {
  DCHECK(CalledOnValidThread());
  return would_be_prerendered_map_.count(web_contents) > 0;
}

bool PrerenderManager::HasRecentlyBeenNavigatedTo(const GURL& url) {
  DCHECK(CalledOnValidThread());

  CleanUpOldNavigations();
  std::list<NavigationRecord>::const_reverse_iterator end = navigations_.rend();
  for (std::list<NavigationRecord>::const_reverse_iterator it =
           navigations_.rbegin();
       it != end;
       ++it) {
    if (it->url_ == url) {
      base::TimeDelta delta = GetCurrentTimeTicks() - it->time_;
      histograms_->RecordTimeSinceLastRecentVisit(delta);
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

DictionaryValue* PrerenderManager::GetAsValue() const {
  DCHECK(CalledOnValidThread());
  DictionaryValue* dict_value = new DictionaryValue();
  dict_value->Set("history", prerender_history_->GetEntriesAsValue());
  dict_value->Set("active", GetActivePrerendersAsValue());
  dict_value->SetBoolean("enabled", enabled_);
  dict_value->SetBoolean("omnibox_enabled", IsOmniboxEnabled(profile_));
  // If prerender is disabled via a flag this method is not even called.
  std::string enabled_note;
  if (IsControlGroup())
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
PrerenderManager::PrerenderData::PrerenderData(PrerenderManager* manager)
    : manager_(manager), contents_(NULL), handle_count_(0) {
}

PrerenderManager::PrerenderData::PrerenderData(PrerenderManager* manager,
                                               PrerenderContents* contents,
                                               base::TimeTicks expiry_time)
    : manager_(manager),
      contents_(contents),
      handle_count_(0),
      expiry_time_(expiry_time) {
}

void PrerenderManager::PrerenderData::OnNewHandle() {
  DCHECK(contents_ || handle_count_ == 0) <<
      "Cannot create multiple handles to a pending prerender.";
  ++handle_count_;
}

void PrerenderManager::PrerenderData::OnNavigateAwayByHandle() {
  if (!contents_) {
    DCHECK_EQ(1, handle_count_);
    // Pending prerenders are not maintained in the active_prerender_list_, so
    // they will not get normal expiry. Since this prerender hasn't even been
    // launched yet, and it's held by a page that is being prerendered, we will
    // just delete it.
    manager_->DestroyPendingPrerenderData(this);
  } else {
    DCHECK_LE(0, handle_count_);
    // We intentionally don't decrement the handle count here, so that the
    // prerender won't be canceled until it times out.
    manager_->SourceNavigatedAway(this);
  }
}

void PrerenderManager::PrerenderData::OnCancelByHandle() {
  DCHECK_LE(1, handle_count_);
  DCHECK(contents_ || handle_count_ == 1);

  if (--handle_count_ == 0) {
    if (contents_) {
      // This will eventually remove this object from active_prerender_list_,
      // triggering the linked_ptr auto deletion.
      contents_->Destroy(FINAL_STATUS_CANCELLED);
    } else {
      manager_->DestroyPendingPrerenderData(this);
    }
  }
}

PrerenderManager::PrerenderData::~PrerenderData() {
}

void PrerenderManager::SetPrerenderContentsFactory(
    PrerenderContents::Factory* prerender_contents_factory) {
  DCHECK(CalledOnValidThread());
  prerender_contents_factory_.reset(prerender_contents_factory);
}

void PrerenderManager::StartPendingPrerender(
    PrerenderHandle* existing_prerender_handle,
    Origin origin,
    int process_id,
    const GURL& url,
    const content::Referrer& referrer,
    const gfx::Size& size,
    content::SessionStorageNamespace* session_storage_namespace) {
  DCHECK(existing_prerender_handle);
  DCHECK(existing_prerender_handle->IsValid());
  DCHECK(existing_prerender_handle->IsPending());

  DVLOG(6) << "StartPendingPrerender";
  DVLOG(6) << "existing_prerender_handle->handle_count_ = " <<
      existing_prerender_handle->prerender_data_->handle_count_;

  DCHECK(process_id == -1 || session_storage_namespace);

  scoped_ptr<PrerenderHandle> swap_prerender_handle(AddPrerender(
      origin, process_id, url, referrer, size, session_storage_namespace));
  if (swap_prerender_handle.get()) {
    // AddPrerender has returned a new prerender handle to us. We want to make
    // |existing_prerender_handle| active, so swap the underlying PrerenderData
    // between the two handles, and delete our old handle (which will release
    // our entry in the pending_prerender_list_).
    existing_prerender_handle->SwapPrerenderDataWith(
        swap_prerender_handle.get());
    swap_prerender_handle->OnCancel();
    return;
  }

  // We could not start our Prerender. Canceling the existing handle will make
  // it return false for PrerenderHandle::IsPending(), and will release the
  // PrerenderData from pending_prerender_list_.
  existing_prerender_handle->OnCancel();
}

void PrerenderManager::SourceNavigatedAway(PrerenderData* prerender_data) {
  // The expiry time of our prerender data will likely change because of
  // this navigation. Since active_prerender_list_ is maintained in sorted
  // order, we remove the affected element from the list, and reinsert it to
  // maintain ordering.
  linked_ptr<PrerenderData> linked_prerender_data;
  for (std::list<linked_ptr<PrerenderData> >::iterator
           it = active_prerender_list_.begin();
       it != active_prerender_list_.end();
       ++it) {
    if (it->get() == prerender_data) {
      linked_prerender_data = *it;
      active_prerender_list_.erase(it);
      // The iterator it is now invalidated; let's clear out of this scope
      // inside of the loop before continuing.
      break;
    }
  }

  DCHECK(linked_prerender_data.get());

  linked_prerender_data->expiry_time_ =
      std::min(linked_prerender_data->expiry_time(),
               GetExpiryTimeForNavigatedAwayPrerender());
  InsertActivePrerenderData(linked_prerender_data);
}

void PrerenderManager::DestroyPendingPrerenderData(
    PrerenderData* pending_prerender_data) {
  for (std::list<linked_ptr<PrerenderData> >::iterator
           it = pending_prerender_list_.begin();
       it != pending_prerender_list_.end();
       ++it) {
    if (it->get() == pending_prerender_data) {
      DCHECK_GE(1, it->get()->handle_count_);
      pending_prerender_list_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

void PrerenderManager::DoShutdown() {
  DestroyAllContents(FINAL_STATUS_MANAGER_SHUTDOWN);
  STLDeleteElements(&prerender_conditions_);
  on_close_tab_contents_deleters_.clear();
  profile_ = NULL;

  DCHECK(active_prerender_list_.empty());
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

  if (origin == ORIGIN_LINK_REL_PRERENDER &&
      IsGoogleSearchResultURL(referrer.url)) {
    origin = ORIGIN_GWS_PRERENDER;
  }

  DeleteOldEntries();
  DeletePendingDeleteEntries();

  GURL url = url_arg;
  GURL alias_url;
  uint8 experiment = GetQueryStringBasedExperiment(url_arg);
  bool control_group_behavior =
      IsControlGroup() || IsControlGroupExperiment(experiment);
  if (control_group_behavior &&
      MaybeGetQueryStringBasedAliasURL(url, &alias_url)) {
    url = alias_url;
  }

  // From here on, we will record a FinalStatus so we need to register with the
  // histogram tracking.
  histograms_->RecordPrerender(origin, url_arg);

  if (PrerenderData* preexisting_prerender_data =
          FindPrerenderData(url, session_storage_namespace)) {
    RecordFinalStatus(origin, experiment, FINAL_STATUS_DUPLICATE);
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
  // On Android we do reuse processes as we have a limited number of them and we
  // still want the benefits of prerendering even when several tabs are open.
#if !defined(OS_ANDROID)
  if (content::RenderProcessHost::ShouldTryToUseExistingProcessHost(
          profile_, url) &&
      !content::RenderProcessHost::run_renderer_in_process()) {
    RecordFinalStatus(origin, experiment, FINAL_STATUS_TOO_MANY_PROCESSES);
    return NULL;
  }
#endif

  // Check if enough time has passed since the last prerender.
  if (!DoesRateLimitAllowPrerender()) {
    // Cancel the prerender. We could add it to the pending prerender list but
    // this doesn't make sense as the next prerender request will be triggered
    // by a navigation and is unlikely to be the same site.
    RecordFinalStatus(origin, experiment, FINAL_STATUS_RATE_LIMIT_EXCEEDED);
    return NULL;
  }

  PrerenderContents* prerender_contents = CreatePrerenderContents(
      url, referrer, origin, experiment);
  if (!prerender_contents || !prerender_contents->Init())
    return NULL;

  histograms_->RecordPrerenderStarted(origin);

  // TODO(cbentzel): Move invalid checks here instead of PrerenderContents?
  InsertActivePrerenderData(linked_ptr<PrerenderData>(new PrerenderData(
      this, prerender_contents, GetExpiryTimeForNewPrerender())));

  PrerenderHandle* prerender_handle =
      new PrerenderHandle(active_prerender_list_.back().get());

  last_prerender_start_time_ = GetCurrentTimeTicks();

  gfx::Size contents_size =
      size.IsEmpty() ? config_.default_tab_bounds.size() : size;

  prerender_contents->StartPrerendering(process_id, contents_size,
                                        session_storage_namespace,
                                        control_group_behavior);

  while (active_prerender_list_.size() > config_.max_concurrency) {
    prerender_contents = active_prerender_list_.front()->contents_;
    DCHECK(prerender_contents);
    prerender_contents->Destroy(FINAL_STATUS_EVICTED);
  }

  histograms_->RecordConcurrency(active_prerender_list_.size(),
                                 config_.max_concurrency);

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
  DeleteOldTabContents();
  DeleteOldEntries();
  if (active_prerender_list_.empty())
    StopSchedulingPeriodicCleanups();

  // Grab a copy of the current PrerenderContents pointers, so that we
  // will not interfere with potential deletions of the list.
  std::vector<PrerenderContents*> prerender_contents;
  prerender_contents.reserve(active_prerender_list_.size());
  for (std::list<linked_ptr<PrerenderData> >::iterator
           it = active_prerender_list_.begin();
      it != active_prerender_list_.end();
      ++it) {
    prerender_contents.push_back(it->get()->contents_);
  }
  std::for_each(prerender_contents.begin(), prerender_contents.end(),
                std::mem_fun(
                    &PrerenderContents::DestroyWhenUsingTooManyResources));

  DeletePendingDeleteEntries();
}

void PrerenderManager::PostCleanupTask() {
  DCHECK(CalledOnValidThread());
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PrerenderManager::PeriodicCleanup,
                 weak_factory_.GetWeakPtr()));
}

base::TimeTicks PrerenderManager::GetExpiryTimeForNewPrerender() const {
  return GetCurrentTimeTicks() + config_.time_to_live;
}

base::TimeTicks PrerenderManager::GetExpiryTimeForNavigatedAwayPrerender()
    const {
  return GetCurrentTimeTicks() + config_.abandon_time_to_live;
}

void PrerenderManager::DeleteOldEntries() {
  DCHECK(CalledOnValidThread());
  while (!active_prerender_list_.empty()) {
    PrerenderData* prerender_data = active_prerender_list_.front().get();
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
      this, prerender_tracker_, profile_, url,
      referrer, origin, experiment_id);
}

void PrerenderManager::DeletePendingDeleteEntries() {
  while (!pending_delete_list_.empty()) {
    PrerenderContents* contents = pending_delete_list_.front();
    pending_delete_list_.pop_front();
    delete contents;
  }
}

void PrerenderManager::InsertActivePrerenderData(
    linked_ptr<PrerenderData> linked_prerender_data) {
  for (std::list<linked_ptr<PrerenderData> >::iterator
           it = active_prerender_list_.begin();
       it != active_prerender_list_.end();
       ++it) {
    if (it->get()->expiry_time() > linked_prerender_data->expiry_time()) {
      active_prerender_list_.insert(it, linked_prerender_data);
      return;
    }
  }
  active_prerender_list_.push_back(linked_prerender_data);
}

PrerenderManager::PrerenderData* PrerenderManager::FindPrerenderData(
    const GURL& url,
    const SessionStorageNamespace* session_storage_namespace) {
  for (std::list<linked_ptr<PrerenderData> >::iterator
           it = active_prerender_list_.begin();
       it != active_prerender_list_.end();
       ++it) {
    PrerenderContents* prerender_contents = it->get()->contents_;
    if (prerender_contents->Matches(url, session_storage_namespace))
      return it->get();
  }
  return NULL;
}

PrerenderManager::PrerenderData*
PrerenderManager::FindPrerenderDataForChildAndRoute(
    const int child_id, const int route_id) {
  for (std::list<linked_ptr<PrerenderData> >::iterator
           it = active_prerender_list_.begin();
       it != active_prerender_list_.end();
       ++it) {
    PrerenderContents* prerender_contents = it->get()->contents_;

    int contents_child_id;
    if (!prerender_contents->GetChildId(&contents_child_id))
      continue;
    int contents_route_id;
    if (!prerender_contents->GetRouteId(&contents_route_id))
      continue;

    if (contents_child_id == child_id && contents_route_id == route_id)
      return it->get();
  }
  return NULL;
}

std::list<linked_ptr<PrerenderManager::PrerenderData> >::iterator
PrerenderManager::FindIteratorForPrerenderContents(
    PrerenderContents* prerender_contents) {
  for (std::list<linked_ptr<PrerenderData> >::iterator
           it = active_prerender_list_.begin();
       it != active_prerender_list_.end();
       ++it) {
    if (prerender_contents == it->get()->contents_)
      return it;
  }
  return active_prerender_list_.end();
}

bool PrerenderManager::DoesRateLimitAllowPrerender() const {
  DCHECK(CalledOnValidThread());
  base::TimeDelta elapsed_time =
      GetCurrentTimeTicks() - last_prerender_start_time_;
  histograms_->RecordTimeBetweenPrerenderRequests(elapsed_time);
  if (!config_.rate_limit_enabled)
    return true;
  return elapsed_time >
      base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs);
}

void PrerenderManager::DeleteOldTabContents() {
  while (!old_tab_contents_list_.empty()) {
    TabContents* tab_contents = old_tab_contents_list_.front();
    old_tab_contents_list_.pop_front();
    // TODO(dominich): should we use Instant Unload Handler here?
    delete tab_contents;
  }
}

void PrerenderManager::CleanUpOldNavigations() {
  DCHECK(CalledOnValidThread());

  // Cutoff.  Navigations before this cutoff can be discarded.
  base::TimeTicks cutoff = GetCurrentTimeTicks() -
      base::TimeDelta::FromMilliseconds(kNavigationRecordWindowMs);
  while (!navigations_.empty()) {
    if (navigations_.front().time_ > cutoff)
      break;
    navigations_.pop_front();
  }
}

void PrerenderManager::ScheduleDeleteOldTabContents(
    TabContents* tab,
    OnCloseTabContentsDeleter* deleter) {
  old_tab_contents_list_.push_back(tab);
  PostCleanupTask();

  if (deleter) {
    ScopedVector<OnCloseTabContentsDeleter>::iterator i = std::find(
        on_close_tab_contents_deleters_.begin(),
        on_close_tab_contents_deleters_.end(),
        deleter);
    DCHECK(i != on_close_tab_contents_deleters_.end());
    on_close_tab_contents_deleters_.erase(i);
  }
}

void PrerenderManager::AddToHistory(PrerenderContents* contents) {
  PrerenderHistory::Entry entry(contents->prerender_url(),
                                contents->final_status(),
                                contents->origin(),
                                base::Time::Now());
  prerender_history_->AddEntry(entry);
}

Value* PrerenderManager::GetActivePrerendersAsValue() const {
  ListValue* list_value = new ListValue();
  for (std::list<linked_ptr<PrerenderData> >::const_iterator it =
           active_prerender_list_.begin();
       it != active_prerender_list_.end();
       ++it) {
    if (Value* prerender_value = it->get()->contents_->GetAsValue())
      list_value->Append(prerender_value);
  }
  return list_value;
}

void PrerenderManager::DestroyAllContents(FinalStatus final_status) {
  DeleteOldTabContents();
  while (!active_prerender_list_.empty()) {
    PrerenderContents* contents = active_prerender_list_.front()->contents_;
    contents->Destroy(final_status);
  }
  DeletePendingDeleteEntries();
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

void PrerenderManager::RecordFinalStatus(Origin origin,
                                         uint8 experiment_id,
                                         FinalStatus final_status) const {
  RecordFinalStatusWithMatchCompleteStatus(
      origin, experiment_id,
      PrerenderContents::MATCH_COMPLETE_DEFAULT,
      final_status);
}

bool PrerenderManager::IsEnabled() const {
  DCHECK(CalledOnValidThread());
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

PrerenderManager* FindPrerenderManagerUsingRenderProcessId(
    int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_id);
  // Each render process is guaranteed to only hold RenderViews owned by the
  // same BrowserContext. This is enforced by
  // RenderProcessHost::GetExistingProcessHost.
  if (!render_process_host || !render_process_host->GetBrowserContext())
    return NULL;
  Profile* profile = Profile::FromBrowserContext(
      render_process_host->GetBrowserContext());
  if (!profile)
    return NULL;
  return PrerenderManagerFactory::GetInstance()->GetForProfile(profile);
}

}  // namespace prerender
