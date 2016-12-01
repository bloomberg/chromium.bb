// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_history.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/common/prerender_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "net/http/http_request_headers.h"
#include "ui/gfx/geometry/rect.h"

using chrome_browser_net::NetworkPredictionStatus;
using content::BrowserThread;
using content::RenderViewHost;
using content::SessionStorageNamespace;
using content::WebContents;

namespace prerender {

namespace {

// Time interval at which periodic cleanups are performed.
const int kPeriodicCleanupIntervalMs = 1000;

// Length of prerender history, for display in chrome://net-internals
const int kHistoryLength = 100;

// Check if |extra_headers| requested via chrome::NavigateParams::extra_headers
// are the same as what the HTTP server saw when serving prerendered contents.
// PrerenderContents::StartPrerendering doesn't specify any extra headers when
// calling content::NavigationController::LoadURLWithParams, but in reality
// Blink will always add an Upgrade-Insecure-Requests http request header, so
// that HTTP request for prerendered contents always includes this header.
// Because of this, it is okay to show prerendered contents even if
// |extra_headers| contains "Upgrade-Insecure-Requests" header.
bool AreExtraHeadersCompatibleWithPrerenderContents(
    const std::string& extra_headers) {
  net::HttpRequestHeaders parsed_headers;
  parsed_headers.AddHeadersFromString(extra_headers);
  parsed_headers.RemoveHeader("upgrade-insecure-requests");
  return parsed_headers.IsEmpty();
}

}  // namespace

class PrerenderManager::OnCloseWebContentsDeleter
    : public content::WebContentsDelegate,
      public base::SupportsWeakPtr<
          PrerenderManager::OnCloseWebContentsDeleter> {
 public:
  OnCloseWebContentsDeleter(PrerenderManager* manager,
                            std::unique_ptr<WebContents> tab)
      : manager_(manager), tab_(std::move(tab)), suppressed_dialog_(false) {
    tab_->SetDelegate(this);
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&OnCloseWebContentsDeleter::ScheduleWebContentsForDeletion,
                   AsWeakPtr(), true),
        base::TimeDelta::FromSeconds(kDeleteWithExtremePrejudiceSeconds));
  }

  void CloseContents(WebContents* source) override {
    DCHECK_EQ(tab_.get(), source);
    ScheduleWebContentsForDeletion(false);
  }

  bool ShouldSuppressDialogs(WebContents* source) override {
    // Use this as a proxy for getting statistics on how often we fail to honor
    // the beforeunload event.
    DCHECK_EQ(tab_.get(), source);
    suppressed_dialog_ = true;
    return true;
  }

 private:
  static const int kDeleteWithExtremePrejudiceSeconds = 3;

  void ScheduleWebContentsForDeletion(bool timeout) {
    UMA_HISTOGRAM_BOOLEAN("Prerender.TabContentsDeleterTimeout", timeout);
    UMA_HISTOGRAM_BOOLEAN("Prerender.TabContentsDeleterSuppressedDialog",
                          suppressed_dialog_);
    tab_->SetDelegate(nullptr);
    manager_->ScheduleDeleteOldWebContents(std::move(tab_), this);
    // |this| is deleted at this point.
  }

  PrerenderManager* const manager_;
  std::unique_ptr<WebContents> tab_;
  bool suppressed_dialog_;

  DISALLOW_COPY_AND_ASSIGN(OnCloseWebContentsDeleter);
};

// static
int PrerenderManager::prerenders_per_session_count_ = 0;

// static
PrerenderManager::PrerenderManagerMode PrerenderManager::mode_ =
    PRERENDER_MODE_ENABLED;

struct PrerenderManager::NavigationRecord {
  NavigationRecord(const GURL& url, base::TimeTicks time, Origin origin)
      : url(url), time(time), origin(origin) {}

  GURL url;
  base::TimeTicks time;
  Origin origin;
};

PrerenderManager::PrerenderManager(Profile* profile)
    : profile_(profile),
      prerender_contents_factory_(PrerenderContents::CreateFactory()),
      prerender_history_(new PrerenderHistory(kHistoryLength)),
      histograms_(new PrerenderHistograms()),
      profile_network_bytes_(0),
      last_recorded_profile_network_bytes_(0),
      clock_(new base::DefaultClock()),
      tick_clock_(new base::DefaultTickClock()),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  last_prerender_start_time_ =
      GetCurrentTimeTicks() -
      base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs);

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

  for (auto* host : prerender_process_hosts_) {
    host->RemoveObserver(this);
  }
}

void PrerenderManager::Shutdown() {
  DestroyAllContents(FINAL_STATUS_MANAGER_SHUTDOWN);
  on_close_web_contents_deleters_.clear();
  profile_ = nullptr;

  DCHECK(active_prerenders_.empty());
}

std::unique_ptr<PrerenderHandle>
PrerenderManager::AddPrerenderFromLinkRelPrerender(
    int process_id,
    int route_id,
    const GURL& url,
    const uint32_t rel_types,
    const content::Referrer& referrer,
    const gfx::Size& size) {
  Origin origin = rel_types & PrerenderRelTypePrerender ?
                      ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN :
                      ORIGIN_LINK_REL_NEXT;
  SessionStorageNamespace* session_storage_namespace = nullptr;
  // Unit tests pass in a process_id == -1.
  if (process_id != -1) {
    RenderViewHost* source_render_view_host =
        RenderViewHost::FromID(process_id, route_id);
    if (!source_render_view_host)
      return nullptr;
    WebContents* source_web_contents =
        WebContents::FromRenderViewHost(source_render_view_host);
    if (!source_web_contents)
      return nullptr;
    if (origin == ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN &&
        source_web_contents->GetURL().host_piece() == url.host_piece()) {
      origin = ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN;
    }
    // TODO(ajwong): This does not correctly handle storage for isolated apps.
    session_storage_namespace =
        source_web_contents->GetController()
            .GetDefaultSessionStorageNamespace();
  }
  return AddPrerender(
      origin, url, referrer, gfx::Rect(size), session_storage_namespace);
}

std::unique_ptr<PrerenderHandle> PrerenderManager::AddPrerenderFromOmnibox(
    const GURL& url,
    SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size) {
  if (!IsOmniboxEnabled(profile_))
    return nullptr;
  return AddPrerender(ORIGIN_OMNIBOX, url, content::Referrer(), gfx::Rect(size),
                      session_storage_namespace);
}

std::unique_ptr<PrerenderHandle>
PrerenderManager::AddPrerenderFromExternalRequest(
    const GURL& url,
    const content::Referrer& referrer,
    SessionStorageNamespace* session_storage_namespace,
    const gfx::Rect& bounds) {
  return AddPrerender(ORIGIN_EXTERNAL_REQUEST, url, referrer,
                      bounds, session_storage_namespace);
}

std::unique_ptr<PrerenderHandle>
PrerenderManager::AddPrerenderOnCellularFromExternalRequest(
    const GURL& url,
    const content::Referrer& referrer,
    SessionStorageNamespace* session_storage_namespace,
    const gfx::Rect& bounds) {
  return AddPrerender(ORIGIN_EXTERNAL_REQUEST_FORCED_CELLULAR,
                      url,
                      referrer,
                      bounds,
                      session_storage_namespace);
}

std::unique_ptr<PrerenderHandle> PrerenderManager::AddPrerenderForInstant(
    const GURL& url,
    content::SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size) {
  return AddPrerender(ORIGIN_INSTANT, url, content::Referrer(), gfx::Rect(size),
                      session_storage_namespace);
}

std::unique_ptr<PrerenderHandle> PrerenderManager::AddPrerenderForOffline(
    const GURL& url,
    content::SessionStorageNamespace* session_storage_namespace,
    const gfx::Size& size) {
  return AddPrerender(ORIGIN_OFFLINE, url, content::Referrer(), gfx::Rect(size),
                      session_storage_namespace);
}

void PrerenderManager::CancelAllPrerenders() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  while (!active_prerenders_.empty()) {
    PrerenderContents* prerender_contents =
        active_prerenders_.front()->contents();
    prerender_contents->Destroy(FINAL_STATUS_CANCELLED);
  }
}

bool PrerenderManager::MaybeUsePrerenderedPage(const GURL& url,
                                               chrome::NavigateParams* params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = params->target_contents;
  DCHECK(!IsWebContentsPrerendering(web_contents, nullptr));

  // Don't prerender if the navigation involves some special parameters that
  // are different from what was used by PrerenderContents::StartPrerendering
  // (which always uses GET method and doesn't specify any extra headers when
  // calling content::NavigationController::LoadURLWithParams).
  if (params->uses_post ||
      !AreExtraHeadersCompatibleWithPrerenderContents(params->extra_headers)) {
    return false;
  }

  DeleteOldEntries();
  to_delete_prerenders_.clear();

  // First, try to find prerender data with the correct session storage
  // namespace.
  // TODO(ajwong): This doesn't handle isolated apps correctly.
  PrerenderData* prerender_data = FindPrerenderData(
      url,
      web_contents->GetController().GetDefaultSessionStorageNamespace());
  if (!prerender_data)
    return false;
  DCHECK(prerender_data->contents());

  if (prerender_data->contents()->prerender_mode() != FULL_PRERENDER)
    return false;

  std::unique_ptr<WebContents> new_web_contents = SwapInternal(
      url, web_contents, prerender_data, params->should_replace_current_entry);
  if (!new_web_contents)
    return false;

  // Record the new target_contents for the callers.
  params->target_contents = new_web_contents.release();
  return true;
}

std::unique_ptr<WebContents> PrerenderManager::SwapInternal(
    const GURL& url,
    WebContents* web_contents,
    PrerenderData* prerender_data,
    bool should_replace_current_entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!IsWebContentsPrerendering(web_contents, nullptr));

  // Only swap if the target WebContents has a CoreTabHelper delegate to swap
  // out of it. For a normal WebContents, this is if it is in a TabStripModel.
  CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(web_contents);
  if (!core_tab_helper || !core_tab_helper->delegate())
    return nullptr;

  PrerenderTabHelper* target_tab_helper =
      PrerenderTabHelper::FromWebContents(web_contents);
  if (!target_tab_helper) {
    NOTREACHED();
    return nullptr;
  }

  if (WebContents* new_web_contents =
      prerender_data->contents()->prerender_contents()) {
    if (web_contents == new_web_contents)
      return nullptr;  // Do not swap in to ourself.

    // We cannot swap in if there is no last committed entry, because we would
    // show a blank page under an existing entry from the current tab.  Even if
    // there is a pending entry, it may not commit.
    // TODO(creis): If there is a pending navigation and no last committed
    // entry, we might be able to transfer the network request instead.
    if (!new_web_contents->GetController().CanPruneAllButLastCommitted()) {
      // Abort this prerender so it is not used later. http://crbug.com/292121
      prerender_data->contents()->Destroy(FINAL_STATUS_NAVIGATION_UNCOMMITTED);
      return nullptr;
    }
  }

  // Do not swap if the target WebContents is not the only WebContents in its
  // current BrowsingInstance.
  if (web_contents->GetSiteInstance()->GetRelatedActiveContentsCount() != 1u) {
    DCHECK_GT(
        web_contents->GetSiteInstance()->GetRelatedActiveContentsCount(), 1u);
    prerender_data->contents()->Destroy(
        FINAL_STATUS_NON_EMPTY_BROWSING_INSTANCE);
    return nullptr;
  }

  // Do not use the prerendered version if there is an opener object.
  if (web_contents->HasOpener()) {
    prerender_data->contents()->Destroy(FINAL_STATUS_WINDOW_OPENER);
    return nullptr;
  }

  // Do not swap in the prerender if the current WebContents is being captured.
  if (web_contents->GetCapturerCount() > 0) {
    prerender_data->contents()->Destroy(FINAL_STATUS_PAGE_BEING_CAPTURED);
    return nullptr;
  }

  // If we are just in the control group (which can be detected by noticing
  // that prerendering hasn't even started yet), record that |web_contents| now
  // would be showing a prerendered contents, but otherwise, don't do anything.
  if (!prerender_data->contents()->prerendering_has_started()) {
    target_tab_helper->WouldHavePrerenderedNextLoad(
        prerender_data->contents()->origin());
    prerender_data->contents()->Destroy(FINAL_STATUS_WOULD_HAVE_BEEN_USED);
    return nullptr;
  }

  // Don't use prerendered pages if debugger is attached to the tab.
  // See http://crbug.com/98541
  if (content::DevToolsAgentHost::IsDebuggerAttached(web_contents)) {
    histograms_->RecordFinalStatus(prerender_data->contents()->origin(),
                                   FINAL_STATUS_DEVTOOLS_ATTACHED);
    prerender_data->contents()->Destroy(FINAL_STATUS_DEVTOOLS_ATTACHED);
    return nullptr;
  }

  // If the prerendered page is in the middle of a cross-site navigation,
  // don't swap it in because there isn't a good way to merge histories.
  if (prerender_data->contents()->IsCrossSiteNavigationPending()) {
    histograms_->RecordFinalStatus(prerender_data->contents()->origin(),
                                   FINAL_STATUS_CROSS_SITE_NAVIGATION_PENDING);
    prerender_data->contents()->Destroy(
        FINAL_STATUS_CROSS_SITE_NAVIGATION_PENDING);
    return nullptr;
  }

  // At this point, we've determined that we will use the prerender.
  content::RenderProcessHost* process_host =
      prerender_data->contents()->GetRenderViewHost()->GetProcess();
  process_host->RemoveObserver(this);
  prerender_process_hosts_.erase(process_host);
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

  PrerenderDataVector::iterator to_erase =
      FindIteratorForPrerenderContents(prerender_data->contents());
  DCHECK(active_prerenders_.end() != to_erase);
  DCHECK_EQ(prerender_data, to_erase->get());
  std::unique_ptr<PrerenderContents> prerender_contents(
      prerender_data->ReleaseContents());
  active_prerenders_.erase(to_erase);

  // Mark prerender as used.
  prerender_contents->PrepareForUse();

  std::unique_ptr<WebContents> new_web_contents =
      prerender_contents->ReleasePrerenderContents();
  std::unique_ptr<WebContents> old_web_contents(web_contents);
  DCHECK(new_web_contents);
  DCHECK(old_web_contents);

  // Merge the browsing history.
  new_web_contents->GetController().CopyStateFromAndPrune(
      &old_web_contents->GetController(),
      should_replace_current_entry);
  CoreTabHelper::FromWebContents(old_web_contents.get())
      ->delegate()
      ->SwapTabContents(old_web_contents.get(), new_web_contents.get(), true,
                        prerender_contents->has_finished_loading());
  prerender_contents->CommitHistory(new_web_contents.get());

  // Update PPLT metrics:
  // If the tab has finished loading, record a PPLT of 0.
  // If the tab is still loading, reset its start time to the current time.
  PrerenderTabHelper* prerender_tab_helper =
      PrerenderTabHelper::FromWebContents(new_web_contents.get());
  DCHECK(prerender_tab_helper);
  prerender_tab_helper->PrerenderSwappedIn();

  if (old_web_contents->NeedToFireBeforeUnload()) {
    // Schedule the delete to occur after the tab has run its unload handlers.
    // TODO(davidben): Honor the beforeunload event. http://crbug.com/304932
    WebContents* old_web_contents_ptr = old_web_contents.get();
    on_close_web_contents_deleters_.push_back(
        base::MakeUnique<OnCloseWebContentsDeleter>(
            this, std::move(old_web_contents)));
    old_web_contents_ptr->DispatchBeforeUnload();
  } else {
    // No unload handler to run, so delete asap.
    ScheduleDeleteOldWebContents(std::move(old_web_contents), nullptr);
  }

  // TODO(cbentzel): Should |prerender_contents| move to the pending delete
  //                 list, instead of deleting directly here?
  AddToHistory(prerender_contents.get());
  RecordNavigation(url);
  return new_web_contents;
}

void PrerenderManager::MoveEntryToPendingDelete(PrerenderContents* entry,
                                                FinalStatus final_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(entry);

  PrerenderDataVector::iterator it = FindIteratorForPrerenderContents(entry);
  DCHECK(it != active_prerenders_.end());
  to_delete_prerenders_.push_back(std::move(*it));
  active_prerenders_.erase(it);
  // Destroy the old WebContents relatively promptly to reduce resource usage.
  PostCleanupTask();
}

void PrerenderManager::RecordPageLoadTimeNotSwappedIn(
    Origin origin,
    base::TimeDelta page_load_time,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  histograms_->RecordPageLoadTimeNotSwappedIn(origin, page_load_time, url);
}

void PrerenderManager::RecordPerceivedPageLoadTime(
    Origin origin,
    NavigationType navigation_type,
    base::TimeDelta perceived_page_load_time,
    double fraction_plt_elapsed_at_swap_in,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (GetPredictionStatusForOrigin(origin)
      != NetworkPredictionStatus::ENABLED) {
    return;
  }

  histograms_->RecordPerceivedPageLoadTime(
      origin, perceived_page_load_time, navigation_type, url);

  if (navigation_type == NAVIGATION_TYPE_PRERENDERED) {
    histograms_->RecordPercentLoadDoneAtSwapin(
        origin, fraction_plt_elapsed_at_swap_in);
  }
}

void PrerenderManager::RecordPrefetchResponseReceived(Origin origin,
                                                      bool is_main_resource,
                                                      bool is_redirect,
                                                      bool is_no_store) {
  histograms_->RecordPrefetchResponseReceived(origin, is_main_resource,
                                              is_redirect, is_no_store);
}

void PrerenderManager::RecordPrefetchRedirectCount(Origin origin,
                                                   bool is_main_resource,
                                                   int redirect_count) {
  histograms_->RecordPrefetchRedirectCount(origin, is_main_resource,
                                           redirect_count);
}

void PrerenderManager::RecordFirstContentfulPaint(const GURL& url,
                                                  bool is_no_store,
                                                  base::TimeDelta time) {
  CleanUpOldNavigations(&prefetches_, base::TimeDelta::FromMinutes(30));

  // Compute the prefetch age.
  base::TimeDelta prefetch_age;
  Origin origin = ORIGIN_NONE;
  for (auto it = prefetches_.crbegin(); it != prefetches_.crend(); ++it) {
    if (it->url == url) {
      prefetch_age = GetCurrentTimeTicks() - it->time;
      origin = it->origin;
      break;
    }
  }

  histograms_->RecordFirstContentfulPaint(origin, is_no_store, time,
                                          prefetch_age);

  // Loading a prefetched URL resets the revalidation bypass. Remove the url
  // from the prefetch list for more accurate metrics.
  prefetches_.erase(
      std::remove_if(prefetches_.begin(), prefetches_.end(),
                     [url](const NavigationRecord& r) { return r.url == url; }),
      prefetches_.end());
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
bool PrerenderManager::IsPrerenderingPossible() {
  return GetMode() != PRERENDER_MODE_DISABLED;
}

// static
bool PrerenderManager::IsNoStatePrefetch() {
  return GetMode() == PRERENDER_MODE_NOSTATE_PREFETCH;
}

// static
bool PrerenderManager::IsSimpleLoadExperiment() {
  return GetMode() == PRERENDER_MODE_SIMPLE_LOAD_EXPERIMENT;
}

bool PrerenderManager::IsWebContentsPrerendering(
    const WebContents* web_contents,
    Origin* origin) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrerenderContents* prerender_contents = GetPrerenderContents(web_contents);
  if (!prerender_contents)
    return false;

  if (origin)
    *origin = prerender_contents->origin();
  return true;
}

bool PrerenderManager::HasPrerenderedUrl(
    GURL url,
    content::WebContents* web_contents) const {
  content::SessionStorageNamespace* session_storage_namespace = web_contents->
      GetController().GetDefaultSessionStorageNamespace();

  for (const auto& prerender_data : active_prerenders_) {
    PrerenderContents* prerender_contents = prerender_data->contents();
    if (prerender_contents->Matches(url, session_storage_namespace))
      return true;
  }
  return false;
}

bool PrerenderManager::HasPrerenderedAndFinishedLoadingUrl(
    GURL url,
    content::WebContents* web_contents) const {
  content::SessionStorageNamespace* session_storage_namespace =
      web_contents->GetController().GetDefaultSessionStorageNamespace();

  for (const auto& prerender_data : active_prerenders_) {
    PrerenderContents* prerender_contents = prerender_data->contents();
    if (prerender_contents->Matches(url, session_storage_namespace) &&
        prerender_contents->has_finished_loading())
      return true;
  }
  return false;
}

PrerenderContents* PrerenderManager::GetPrerenderContents(
    const content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& prerender : active_prerenders_) {
    WebContents* prerender_web_contents =
        prerender->contents()->prerender_contents();
    if (prerender_web_contents == web_contents) {
      return prerender->contents();
    }
  }

  // Also check the pending-deletion list. If the prerender is in pending
  // delete, anyone with a handle on the WebContents needs to know.
  for (const auto& prerender : to_delete_prerenders_) {
    WebContents* prerender_web_contents =
        prerender->contents()->prerender_contents();
    if (prerender_web_contents == web_contents) {
      return prerender->contents();
    }
  }
  return nullptr;
}

PrerenderContents* PrerenderManager::GetPrerenderContentsForRoute(
    int child_id,
    int route_id) const {
  WebContents* web_contents = tab_util::GetWebContentsByID(child_id, route_id);
  return web_contents ? GetPrerenderContents(web_contents) : nullptr;
}

PrerenderContents* PrerenderManager::GetPrerenderContentsForProcess(
    int render_process_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto& prerender_data : active_prerenders_) {
    PrerenderContents* prerender_contents = prerender_data->contents();
    if (prerender_contents->GetRenderViewHost()->GetProcess()->GetID() ==
        render_process_id) {
      return prerender_contents;
    }
  }
  return nullptr;
}

std::vector<WebContents*> PrerenderManager::GetAllPrerenderingContents() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<WebContents*> result;

  for (const auto& prerender : active_prerenders_) {
    WebContents* contents = prerender->contents()->prerender_contents();
    if (contents &&
        prerender->contents()->prerender_mode() == FULL_PRERENDER) {
      result.push_back(contents);
    }
  }

  return result;
}

bool PrerenderManager::HasRecentlyBeenNavigatedTo(Origin origin,
                                                  const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CleanUpOldNavigations(&navigations_, base::TimeDelta::FromMilliseconds(
                                           kNavigationRecordWindowMs));
  for (auto it = navigations_.rbegin(); it != navigations_.rend(); ++it) {
    if (it->url == url) {
      base::TimeDelta delta = GetCurrentTimeTicks() - it->time;
      histograms_->RecordTimeSinceLastRecentVisit(origin, delta);
      return true;
    }
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
  return DoesURLHaveValidScheme(url) || url == url::kAboutBlankURL;
}

std::unique_ptr<base::DictionaryValue> PrerenderManager::GetAsValue() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto dict_value = base::MakeUnique<base::DictionaryValue>();
  dict_value->Set("history", prerender_history_->GetEntriesAsValue());
  dict_value->Set("active", GetActivePrerendersAsValue());
  dict_value->SetBoolean("enabled",
      GetPredictionStatus() == NetworkPredictionStatus::ENABLED);
  std::string disabled_note;
  if (GetPredictionStatus() == NetworkPredictionStatus::DISABLED_ALWAYS)
    disabled_note = "Disabled by user setting";
  if (GetPredictionStatus() == NetworkPredictionStatus::DISABLED_DUE_TO_NETWORK)
    disabled_note = "Disabled on cellular connection by default";
  dict_value->SetString("disabled_note", disabled_note);
  dict_value->SetBoolean("omnibox_enabled", IsOmniboxEnabled(profile_));
  // If prerender is disabled via a flag this method is not even called.
  std::string enabled_note;
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

void PrerenderManager::RecordFinalStatus(Origin origin,
                                         FinalStatus final_status) const {
  histograms_->RecordFinalStatus(origin, final_status);
}

void PrerenderManager::RecordNavigation(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  navigations_.emplace_back(url, GetCurrentTimeTicks(), ORIGIN_NONE);
  CleanUpOldNavigations(&navigations_, base::TimeDelta::FromMilliseconds(
                                           kNavigationRecordWindowMs));
}

struct PrerenderManager::PrerenderData::OrderByExpiryTime {
  bool operator()(const std::unique_ptr<PrerenderData>& a,
                  const std::unique_ptr<PrerenderData>& b) const {
    return a->expiry_time() < b->expiry_time();
  }
};

PrerenderManager::PrerenderData::PrerenderData(
    PrerenderManager* manager,
    std::unique_ptr<PrerenderContents> contents,
    base::TimeTicks expiry_time)
    : manager_(manager),
      contents_(std::move(contents)),
      handle_count_(0),
      expiry_time_(expiry_time) {
  DCHECK(contents_);
}

PrerenderManager::PrerenderData::~PrerenderData() {
}

void PrerenderManager::PrerenderData::OnHandleCreated(PrerenderHandle* handle) {
  DCHECK(contents_);
  ++handle_count_;
  contents_->AddObserver(handle);
}

void PrerenderManager::PrerenderData::OnHandleNavigatedAway(
    PrerenderHandle* handle) {
  DCHECK_LT(0, handle_count_);
  DCHECK(contents_);
  if (abandon_time_.is_null())
    abandon_time_ = base::TimeTicks::Now();
  // We intentionally don't decrement the handle count here, so that the
  // prerender won't be canceled until it times out.
  manager_->SourceNavigatedAway(this);
}

void PrerenderManager::PrerenderData::OnHandleCanceled(
    PrerenderHandle* handle) {
  DCHECK_LT(0, handle_count_);
  DCHECK(contents_);

  if (--handle_count_ == 0) {
    // This will eventually remove this object from |active_prerenders_|.
    contents_->Destroy(FINAL_STATUS_CANCELLED);
  }
}

std::unique_ptr<PrerenderContents>
PrerenderManager::PrerenderData::ReleaseContents() {
  return std::move(contents_);
}

void PrerenderManager::SourceNavigatedAway(PrerenderData* prerender_data) {
  // The expiry time of our prerender data will likely change because of
  // this navigation. This requires a re-sort of |active_prerenders_|.
  for (PrerenderDataVector::iterator it = active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    PrerenderData* data = it->get();
    if (data == prerender_data) {
      data->set_expiry_time(std::min(data->expiry_time(),
                                     GetExpiryTimeForNavigatedAwayPrerender()));
      SortActivePrerenders();
      return;
    }
  }
}

bool PrerenderManager::IsLowEndDevice() const {
  return base::SysInfo::IsLowEndDevice();
}

std::unique_ptr<PrerenderHandle> PrerenderManager::AddPrerender(
    Origin origin,
    const GURL& url_arg,
    const content::Referrer& referrer,
    const gfx::Rect& bounds,
    SessionStorageNamespace* session_storage_namespace) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Allow only Requests for offlining on low end devices, the lifetime of
  // those prerenders is managed by the offliner.
  if (IsLowEndDevice() && origin != ORIGIN_OFFLINE)
    return nullptr;

  if ((origin == ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN ||
       origin == ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN) &&
      IsGoogleSearchResultURL(referrer.url)) {
    origin = ORIGIN_GWS_PRERENDER;
  }

  if (IsPrerenderSilenceExperiment(origin))
    return nullptr;

  GURL url = url_arg;
  GURL alias_url;

  // From here on, we will record a FinalStatus so we need to register with the
  // histogram tracking.
  histograms_->RecordPrerender(origin, url_arg);

  if (profile_->GetPrefs()->GetBoolean(prefs::kBlockThirdPartyCookies) &&
      origin != ORIGIN_OFFLINE) {
    RecordFinalStatusWithoutCreatingPrerenderContents(
        url, origin, FINAL_STATUS_BLOCK_THIRD_PARTY_COOKIES);
    return nullptr;
  }

  NetworkPredictionStatus prerendering_status =
      GetPredictionStatusForOrigin(origin);
  if (prerendering_status != NetworkPredictionStatus::ENABLED) {
    FinalStatus final_status =
        prerendering_status == NetworkPredictionStatus::DISABLED_DUE_TO_NETWORK
            ? FINAL_STATUS_CELLULAR_NETWORK
            : FINAL_STATUS_PRERENDERING_DISABLED;
    RecordFinalStatusWithoutCreatingPrerenderContents(url, origin,
                                                      final_status);
    return nullptr;
  }

  if (PrerenderData* preexisting_prerender_data =
          FindPrerenderData(url, session_storage_namespace)) {
    RecordFinalStatusWithoutCreatingPrerenderContents(
        url, origin, FINAL_STATUS_DUPLICATE);
    return base::WrapUnique(new PrerenderHandle(preexisting_prerender_data));
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
        url, origin, FINAL_STATUS_TOO_MANY_PROCESSES);
    return nullptr;
  }

  // Check if enough time has passed since the last prerender.
  if (!DoesRateLimitAllowPrerender(origin)) {
    // Cancel the prerender. We could add it to the pending prerender list but
    // this doesn't make sense as the next prerender request will be triggered
    // by a navigation and is unlikely to be the same site.
    RecordFinalStatusWithoutCreatingPrerenderContents(
        url, origin, FINAL_STATUS_RATE_LIMIT_EXCEEDED);
    return nullptr;
  }

  // Record the URL in the prefetch list, even when in full prerender mode, to
  // enable metrics comparisons.
  prefetches_.emplace_back(url, GetCurrentTimeTicks(), origin);

  if (IsSimpleLoadExperiment()) {
    // Exit after adding the url to prefetches_, so that no prefetching occurs
    // but the page is still tracked as "would have been prefetched".
    return nullptr;
  }

  std::unique_ptr<PrerenderContents> prerender_contents =
      CreatePrerenderContents(url, referrer, origin);
  DCHECK(prerender_contents);
  PrerenderContents* prerender_contents_ptr = prerender_contents.get();
  if (IsNoStatePrefetch())
    prerender_contents_ptr->SetPrerenderMode(PREFETCH_ONLY);
  active_prerenders_.push_back(
      base::MakeUnique<PrerenderData>(this, std::move(prerender_contents),
                                      GetExpiryTimeForNewPrerender(origin)));
  if (!prerender_contents_ptr->Init()) {
    DCHECK(active_prerenders_.end() ==
           FindIteratorForPrerenderContents(prerender_contents_ptr));
    return nullptr;
  }

  histograms_->RecordPrerenderStarted(origin);
  DCHECK(!prerender_contents_ptr->prerendering_has_started());

  std::unique_ptr<PrerenderHandle> prerender_handle =
      base::WrapUnique(new PrerenderHandle(active_prerenders_.back().get()));
  SortActivePrerenders();

  last_prerender_start_time_ = GetCurrentTimeTicks();

  gfx::Rect contents_bounds =
      bounds.IsEmpty() ? config_.default_tab_bounds : bounds;

  prerender_contents_ptr->StartPrerendering(contents_bounds,
                                            session_storage_namespace);

  DCHECK(prerender_contents_ptr->prerendering_has_started());

  StartSchedulingPeriodicCleanups();
  return prerender_handle;
}

void PrerenderManager::StartSchedulingPeriodicCleanups() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (repeating_timer_.IsRunning())
    return;
  repeating_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kPeriodicCleanupIntervalMs),
      this,
      &PrerenderManager::PeriodicCleanup);
}

void PrerenderManager::StopSchedulingPeriodicCleanups() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  repeating_timer_.Stop();
}

void PrerenderManager::PeriodicCleanup() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::ElapsedTimer resource_timer;

  // Grab a copy of the current PrerenderContents pointers, so that we
  // will not interfere with potential deletions of the list.
  std::vector<PrerenderContents*> prerender_contents;
  prerender_contents.reserve(active_prerenders_.size());
  for (auto& prerender : active_prerenders_)
    prerender_contents.push_back(prerender->contents());

  // And now check for prerenders using too much memory.
  for (auto* contents : prerender_contents)
    contents->DestroyWhenUsingTooManyResources();

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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&PrerenderManager::PeriodicCleanup,
                            weak_factory_.GetWeakPtr()));
}

base::TimeTicks PrerenderManager::GetExpiryTimeForNewPrerender(
    Origin origin) const {
  return GetCurrentTimeTicks() + config_.time_to_live;
}

base::TimeTicks PrerenderManager::GetExpiryTimeForNavigatedAwayPrerender()
    const {
  return GetCurrentTimeTicks() + config_.abandon_time_to_live;
}

void PrerenderManager::DeleteOldEntries() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  while (!active_prerenders_.empty()) {
    auto& prerender_data = active_prerenders_.front();
    DCHECK(prerender_data);
    DCHECK(prerender_data->contents());

    if (prerender_data->expiry_time() > GetCurrentTimeTicks())
      return;
    prerender_data->contents()->Destroy(FINAL_STATUS_TIMED_OUT);
  }
}

base::Time PrerenderManager::GetCurrentTime() const {
  return clock_->Now();
}

base::TimeTicks PrerenderManager::GetCurrentTimeTicks() const {
  return tick_clock_->NowTicks();
}

void PrerenderManager::SetClockForTesting(
    std::unique_ptr<base::SimpleTestClock> clock) {
  clock_ = std::move(clock);
}

void PrerenderManager::SetTickClockForTesting(
    std::unique_ptr<base::SimpleTestTickClock> tick_clock) {
  tick_clock_ = std::move(tick_clock);
}

std::unique_ptr<PrerenderContents> PrerenderManager::CreatePrerenderContents(
    const GURL& url,
    const content::Referrer& referrer,
    Origin origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::WrapUnique(prerender_contents_factory_->CreatePrerenderContents(
      this, profile_, url, referrer, origin));
}

void PrerenderManager::SortActivePrerenders() {
  std::sort(active_prerenders_.begin(), active_prerenders_.end(),
            PrerenderData::OrderByExpiryTime());
}

PrerenderManager::PrerenderData* PrerenderManager::FindPrerenderData(
    const GURL& url,
    const SessionStorageNamespace* session_storage_namespace) {
  for (const auto& prerender : active_prerenders_) {
    PrerenderContents* contents = prerender->contents();
    if (contents->Matches(url, session_storage_namespace)) {
      return contents->origin() != ORIGIN_OFFLINE ? prerender.get() : nullptr;
    }
  }
  return nullptr;
}

PrerenderManager::PrerenderDataVector::iterator
PrerenderManager::FindIteratorForPrerenderContents(
    PrerenderContents* prerender_contents) {
  for (PrerenderDataVector::iterator it = active_prerenders_.begin();
       it != active_prerenders_.end(); ++it) {
    if ((*it)->contents() == prerender_contents)
      return it;
  }
  return active_prerenders_.end();
}

bool PrerenderManager::DoesRateLimitAllowPrerender(Origin origin) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::TimeDelta elapsed_time =
      GetCurrentTimeTicks() - last_prerender_start_time_;
  histograms_->RecordTimeBetweenPrerenderRequests(origin, elapsed_time);
  // TODO(gabadie,pasko): Re-implement missing tests for
  // FINAL_STATUS_RATE_LIMIT_EXCEEDED that where removed by:
  //    http://crrev.com/a2439eeab37f7cb7a118493fb55ec0cb07f93b49.
  if (origin == ORIGIN_OFFLINE)
    return true;
  if (!config_.rate_limit_enabled)
    return true;
  return elapsed_time >=
      base::TimeDelta::FromMilliseconds(kMinTimeBetweenPrerendersMs);
}

void PrerenderManager::DeleteOldWebContents() {
  for (WebContents* web_contents : old_web_contents_list_) {
    // TODO(dominich): should we use Instant Unload Handler here?
    // Or should |old_web_contents_list_| contain unique_ptrs?
    delete web_contents;
  }
  old_web_contents_list_.clear();
}

void PrerenderManager::CleanUpOldNavigations(
    std::vector<NavigationRecord>* navigations,
    base::TimeDelta max_age) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Cutoff. Navigations before this cutoff can be discarded.
  base::TimeTicks cutoff = GetCurrentTimeTicks() - max_age;
  auto it = navigations->begin();
  for (; it != navigations->end(); ++it) {
    if (it->time > cutoff)
      break;
  }
  navigations->erase(navigations->begin(), it);
}

void PrerenderManager::ScheduleDeleteOldWebContents(
    std::unique_ptr<WebContents> tab,
    OnCloseWebContentsDeleter* deleter) {
  old_web_contents_list_.push_back(tab.release());
  PostCleanupTask();

  if (!deleter)
    return;

  for (auto it = on_close_web_contents_deleters_.begin();
       it != on_close_web_contents_deleters_.end(); ++it) {
    if (it->get() == deleter) {
      on_close_web_contents_deleters_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

void PrerenderManager::AddToHistory(PrerenderContents* contents) {
  PrerenderHistory::Entry entry(contents->prerender_url(),
                                contents->final_status(),
                                contents->origin(),
                                base::Time::Now());
  prerender_history_->AddEntry(entry);
}

std::unique_ptr<base::ListValue> PrerenderManager::GetActivePrerendersAsValue()
    const {
  auto list_value = base::MakeUnique<base::ListValue>();
  for (const auto& prerender : active_prerenders_) {
    auto prerender_value = prerender->contents()->GetAsValue();
    if (prerender_value)
      list_value->Append(std::move(prerender_value));
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

void PrerenderManager::RecordFinalStatusWithoutCreatingPrerenderContents(
    const GURL& url, Origin origin, FinalStatus final_status) const {
  PrerenderHistory::Entry entry(url, final_status, origin, base::Time::Now());
  prerender_history_->AddEntry(entry);
  histograms_->RecordFinalStatus(origin, final_status);
}

void PrerenderManager::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);

  DestroyAllContents(FINAL_STATUS_PROFILE_DESTROYED);
  on_close_web_contents_deleters_.clear();
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

  prerender_contents->Destroy(FINAL_STATUS_CREATING_AUDIO_STREAM);
}

void PrerenderManager::RecordNetworkBytes(Origin origin,
                                          bool used,
                                          int64_t prerender_bytes) {
  if (!IsPrerenderingPossible())
    return;
  int64_t recent_profile_bytes =
      profile_network_bytes_ - last_recorded_profile_network_bytes_;
  last_recorded_profile_network_bytes_ = profile_network_bytes_;
  DCHECK_GE(recent_profile_bytes, 0);
  histograms_->RecordNetworkBytes(
      origin, used, prerender_bytes, recent_profile_bytes);
}

bool PrerenderManager::IsPrerenderSilenceExperiment(Origin origin) const {
  if (origin == ORIGIN_OFFLINE ||
      origin == ORIGIN_EXTERNAL_REQUEST_FORCED_CELLULAR) {
    return false;
  }

  // The group name should contain expiration time formatted as:
  //   "ExperimentYes_expires_YYYY-MM-DDTHH:MM:SSZ".
  std::string group_name =
      base::FieldTrialList::FindFullName("PrerenderSilence");
  const char kExperimentPrefix[] = "ExperimentYes";
  if (!base::StartsWith(group_name, kExperimentPrefix,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    // The experiment group was not set, use 2016-12-14 PST as the day of the
    // experiment.
    base::Time experiment_start;
    if (!base::Time::FromString("2016-12-14-08:00:00Z", &experiment_start))
      NOTREACHED();
    base::Time current_time = GetCurrentTime();
    if ((experiment_start <= current_time) &&
        (current_time < experiment_start + base::TimeDelta::FromDays(1))) {
      return true;
    }
    return false;
  }
  const char kExperimentPrefixWithExpiration[] = "ExperimentYes_expires_";
  if (!base::StartsWith(group_name, kExperimentPrefixWithExpiration,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    // Without expiration day in the group name, behave as a normal experiment,
    // i.e. sticky to the Chrome session.
    return true;
  }
  base::Time expiration_time;
  if (!base::Time::FromString(
          group_name.c_str() + (arraysize(kExperimentPrefixWithExpiration) - 1),
          &expiration_time)) {
    DLOG(ERROR) << "Could not parse expiration date in group: " << group_name;
    return false;
  }
  return GetCurrentTime() < expiration_time;
}

NetworkPredictionStatus PrerenderManager::GetPredictionStatus() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return chrome_browser_net::CanPrefetchAndPrerenderUI(profile_->GetPrefs());
}

NetworkPredictionStatus PrerenderManager::GetPredictionStatusForOrigin(
    Origin origin) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // <link rel=prerender> origins ignore the network state and the privacy
  // settings. Web developers should be able prefetch with all possible privacy
  // settings and with all possible network types. This would avoid web devs
  // coming up with creative ways to prefetch in cases they are not allowed to
  // do so.
  //
  // Offline originated prerenders also ignore the network state and privacy
  // settings because they are controlled by the offliner logic via
  // PrerenderHandle.
  if (origin == ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN ||
      origin == ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN ||
      origin == ORIGIN_OFFLINE) {
    return NetworkPredictionStatus::ENABLED;
  }

  // Prerendering forced for cellular networks still prevents navigation with
  // the DISABLED_ALWAYS selected via privacy settings.
  NetworkPredictionStatus prediction_status =
      chrome_browser_net::CanPrefetchAndPrerenderUI(profile_->GetPrefs());
  if (origin == ORIGIN_EXTERNAL_REQUEST_FORCED_CELLULAR &&
      prediction_status == NetworkPredictionStatus::DISABLED_DUE_TO_NETWORK) {
    return NetworkPredictionStatus::ENABLED;
  }
  return prediction_status;
}

void PrerenderManager::AddProfileNetworkBytesIfEnabled(int64_t bytes) {
  DCHECK_GE(bytes, 0);
  if (GetPredictionStatus() == NetworkPredictionStatus::ENABLED &&
      IsPrerenderingPossible())
    profile_network_bytes_ += bytes;
}

void PrerenderManager::AddPrerenderProcessHost(
    content::RenderProcessHost* process_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(prerender_process_hosts_.find(process_host) ==
         prerender_process_hosts_.end());
  prerender_process_hosts_.insert(process_host);
  process_host->AddObserver(this);
}

bool PrerenderManager::MayReuseProcessHost(
    content::RenderProcessHost* process_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Isolate prerender processes to make the resource monitoring check more
  // accurate.
  return (prerender_process_hosts_.find(process_host) ==
          prerender_process_hosts_.end());
}

void PrerenderManager::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  size_t erased = prerender_process_hosts_.erase(host);
  DCHECK_EQ(1u, erased);
}

base::WeakPtr<PrerenderManager> PrerenderManager::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PrerenderManager::SetPrerenderContentsFactoryForTest(
    PrerenderContents::Factory* prerender_contents_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  prerender_contents_factory_.reset(prerender_contents_factory);
}

}  // namespace prerender
