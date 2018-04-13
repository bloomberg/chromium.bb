// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_importance_signals.h"
#include "url/gurl.h"

namespace resource_coordinator {

TabLifecycleUnitSource::TabLifecycleUnit::TabLifecycleUnit(
    base::ObserverList<TabLifecycleObserver>* observers,
    content::WebContents* web_contents,
    TabStripModel* tab_strip_model)
    : content::WebContentsObserver(web_contents),
      observers_(observers),
      tab_strip_model_(tab_strip_model) {
  DCHECK(observers_);
  DCHECK(GetWebContents());
  DCHECK(tab_strip_model_);
}

TabLifecycleUnitSource::TabLifecycleUnit::~TabLifecycleUnit() {
  OnLifecycleUnitDestroyed();
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetTabStripModel(
    TabStripModel* tab_strip_model) {
  tab_strip_model_ = tab_strip_model;
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  Observe(web_contents);
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetFocused(bool focused) {
  const bool was_focused = last_focused_time_ == base::TimeTicks::Max();
  if (focused == was_focused)
    return;
  last_focused_time_ = focused ? base::TimeTicks::Max() : NowTicks();

  if (focused && GetState() == State::DISCARDED) {
    SetState(State::LOADED);
    // See comment in Discard() for an explanation of why "needs reload" is
    // false when a tab is discarded.
    // TODO(fdoray): Remove NavigationControllerImpl::needs_reload_ once session
    // restore is handled by LifecycleManager.
    GetWebContents()->GetController().SetNeedsReload();
    GetWebContents()->GetController().LoadIfNecessary();
    OnDiscardedStateChange();
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetRecentlyAudible(
    bool recently_audible) {
  if (recently_audible)
    recently_audible_time_ = base::TimeTicks::Max();
  else if (recently_audible_time_ == base::TimeTicks::Max())
    recently_audible_time_ = NowTicks();
}

TabLifecycleUnitExternal*
TabLifecycleUnitSource::TabLifecycleUnit::AsTabLifecycleUnitExternal() {
  return this;
}

base::string16 TabLifecycleUnitSource::TabLifecycleUnit::GetTitle() const {
  return GetWebContents()->GetTitle();
}

std::string TabLifecycleUnitSource::TabLifecycleUnit::GetIconURL() const {
  auto* last_committed_entry =
      GetWebContents()->GetController().GetLastCommittedEntry();
  if (!last_committed_entry)
    return std::string();
  const auto& favicon = last_committed_entry->GetFavicon();
  return favicon.valid ? favicon.url.spec() : std::string();
}

base::ProcessHandle TabLifecycleUnitSource::TabLifecycleUnit::GetProcessHandle()
    const {
  content::RenderFrameHost* main_frame = GetWebContents()->GetMainFrame();
  if (!main_frame)
    return base::ProcessHandle();
  content::RenderProcessHost* process = main_frame->GetProcess();
  if (!process)
    return base::ProcessHandle();
  return process->GetHandle();
}

LifecycleUnit::SortKey TabLifecycleUnitSource::TabLifecycleUnit::GetSortKey()
    const {
  return SortKey(last_focused_time_);
}

content::Visibility TabLifecycleUnitSource::TabLifecycleUnit::GetVisibility()
    const {
  return GetWebContents()->GetVisibility();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::Freeze() {
  // Can't freeze tabs that are already discarded or frozen.
  // TODO(fmeawad): Don't freeze already frozen tabs.
  if (GetState() != State::LOADED)
    return false;

  GetWebContents()->FreezePage();
  return true;
}

int TabLifecycleUnitSource::TabLifecycleUnit::
    GetEstimatedMemoryFreedOnDiscardKB() const {
#if defined(OS_CHROMEOS)
  std::unique_ptr<base::ProcessMetrics> process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(GetProcessHandle()));
  base::ProcessMetrics::TotalsSummary summary =
      process_metrics->GetTotalsSummary();
  return summary.private_clean_kb + summary.private_dirty_kb + summary.swap_kb;
#else
  // TODO(fdoray): Implement this. https://crbug.com/775644
  return 0;
#endif
}

bool TabLifecycleUnitSource::TabLifecycleUnit::CanPurge() const {
  // A renderer can be purged if it's not playing media.
  return !IsMediaTab();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::CanDiscard(
    DiscardReason reason) const {
  // Can't discard a tab that isn't in a TabStripModel.
  if (!tab_strip_model_)
    return false;

  // Can't discard a tab that is already discarded.
  if (IsDiscarded())
    return false;

  if (GetWebContents()->IsCrashed())
    return false;

#if defined(OS_CHROMEOS)
  if (GetWebContents()->GetVisibility() == content::Visibility::VISIBLE)
    return false;
#else
  // Do not discard the tab if it is currently active in its window.
  if (tab_strip_model_->GetActiveWebContents() == GetWebContents())
    return false;
#endif  // defined(OS_CHROMEOS)

  // Do not discard tabs that don't have a valid URL (most probably they have
  // just been opened and discarding them would lose the URL).
  // TODO(fdoray): Look into a workaround to be able to kill the tab without
  // losing the pending navigation.
  if (!GetWebContents()->GetLastCommittedURL().is_valid() ||
      GetWebContents()->GetLastCommittedURL().is_empty()) {
    return false;
  }

  // Do not discard tabs in which the user has entered text in a form.
  if (GetWebContents()->GetPageImportanceSignals().had_form_interaction)
    return false;

  // Do not discard media tabs as it's too distruptive to the user experience.
  if (IsMediaTab())
    return false;

  // Do not discard PDFs as they might contain entry that is not saved and they
  // don't remember their scrolling positions. See crbug.com/547286 and
  // crbug.com/65244.
  // TODO(fdoray): Remove this workaround when the bugs are fixed.
  if (GetWebContents()->GetContentsMimeType() == "application/pdf")
    return false;

  // Do not discard a tab that was explicitly disallowed to.
  if (!IsAutoDiscardable())
    return false;

#if defined(OS_CHROMEOS)
  // The following protections are ignored on ChromeOS during urgent discard,
  // because running out of memory would lead to a kernel panic.
  if (reason == DiscardReason::kUrgent)
    return true;
#endif  // defined(OS_CHROMEOS)

  // Do not discard a tab that has already been discarded.
  // TODO(fdoray): Allow tabs to be discarded more than once.
  // https://crbug.com/794622
  if (discard_count_ > 0)
    return false;

  return true;
}

bool TabLifecycleUnitSource::TabLifecycleUnit::Discard(
    DiscardReason discard_reason) {
  if (!tab_strip_model_ || IsDiscarded())
    return false;

  UMA_HISTOGRAM_BOOLEAN(
      "TabManager.Discarding.DiscardedTabHasBeforeUnloadHandler",
      GetWebContents()->NeedToFireBeforeUnload());

  content::WebContents* const old_contents = GetWebContents();
  content::WebContents::CreateParams create_params(tab_strip_model_->profile());
  // TODO(fdoray): Consider setting |initially_hidden| to true when the tab is
  // OCCLUDED. Will require checking that the tab reload correctly when it
  // becomes VISIBLE.
  create_params.initially_hidden =
      old_contents->GetVisibility() == content::Visibility::HIDDEN;
  content::WebContents* const null_contents =
      content::WebContents::Create(create_params);
  // Copy over the state from the navigation controller to preserve the
  // back/forward history and to continue to display the correct title/favicon.
  //
  // Set |needs_reload| to false so that the tab is not automatically reloaded
  // when activated. If it was true, there would be an immediate reload when the
  // active tab of a non-visible window is discarded. SetFocused() will take
  // care of reloading the tab when it becomes active in a focused window.
  null_contents->GetController().CopyStateFrom(old_contents->GetController(),
                                               /* needs_reload */ false);

  // Persist the last active time property.
  null_contents->SetLastActiveTime(old_contents->GetLastActiveTime());

  // First try to fast-kill the process, if it's just running a single tab.
  bool fast_shutdown_success =
      GetRenderProcessHost()->FastShutdownIfPossible(1u, false);

#if defined(OS_CHROMEOS)
  if (!fast_shutdown_success && discard_reason == DiscardReason::kUrgent) {
    content::RenderFrameHost* main_frame = old_contents->GetMainFrame();
    // We avoid fast shutdown on tabs with beforeunload handlers on the main
    // frame, as that is often an indication of unsaved user state.
    DCHECK(main_frame);
    if (!main_frame->GetSuddenTerminationDisablerState(
            blink::kBeforeUnloadHandler)) {
      fast_shutdown_success = GetRenderProcessHost()->FastShutdownIfPossible(
          1u, /* skip_unload_handlers */ true);
    }
    UMA_HISTOGRAM_BOOLEAN(
        "TabManager.Discarding.DiscardedTabCouldUnsafeFastShutdown",
        fast_shutdown_success);
  }
#endif
  UMA_HISTOGRAM_BOOLEAN("TabManager.Discarding.DiscardedTabCouldFastShutdown",
                        fast_shutdown_success);

  // Replace the discarded tab with the null version.
  const int index = tab_strip_model_->GetIndexOfWebContents(old_contents);
  DCHECK_NE(index, TabStripModel::kNoTab);
  tab_strip_model_->ReplaceWebContentsAt(index, null_contents);
  DCHECK_EQ(GetWebContents(), null_contents);

  // This ensures that on reload after discard, the document has
  // "wasDiscarded" set to true.
  null_contents->SetWasDiscarded(true);

  // Discard the old tab's renderer.
  // TODO(jamescook): This breaks script connections with other tabs. Find a
  // different approach that doesn't do that, perhaps based on
  // RenderFrameProxyHosts.
  delete old_contents;

  SetState(State::DISCARDED);
  ++discard_count_;
  OnDiscardedStateChange();

  return true;
}

content::WebContents* TabLifecycleUnitSource::TabLifecycleUnit::GetWebContents()
    const {
  return web_contents();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsMediaTab() const {
  // TODO(fdoray): Consider being notified of audible, capturing and mirrored
  // state changes via WebContentsDelegate::NavigationStateChanged().
  // https://crbug.com/775644

  if (recently_audible_time_ == base::TimeTicks::Max() ||
      (!recently_audible_time_.is_null() &&
       NowTicks() - recently_audible_time_ < kTabAudioProtectionTime)) {
    return true;
  }

  scoped_refptr<MediaStreamCaptureIndicator> media_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  if (media_indicator->IsCapturingUserMedia(GetWebContents()) ||
      media_indicator->IsBeingMirrored(GetWebContents())) {
    return true;
  }

  return false;
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsAutoDiscardable() const {
  return auto_discardable_;
}

void TabLifecycleUnitSource::TabLifecycleUnit::SetAutoDiscardable(
    bool auto_discardable) {
  if (auto_discardable_ == auto_discardable)
    return;
  auto_discardable_ = auto_discardable;
  for (auto& observer : *observers_)
    observer.OnAutoDiscardableStateChange(GetWebContents(), auto_discardable_);
}

bool TabLifecycleUnitSource::TabLifecycleUnit::DiscardTab() {
  return Discard(DiscardReason::kExternal);
}

bool TabLifecycleUnitSource::TabLifecycleUnit::FreezeTab() {
  return Freeze();
}

bool TabLifecycleUnitSource::TabLifecycleUnit::IsDiscarded() const {
  return GetState() == State::DISCARDED;
}

int TabLifecycleUnitSource::TabLifecycleUnit::GetDiscardCount() const {
  return discard_count_;
}

void TabLifecycleUnitSource::TabLifecycleUnit::OnDiscardedStateChange() {
  for (auto& observer : *observers_)
    observer.OnDiscardedStateChange(GetWebContents(), IsDiscarded());
}

content::RenderProcessHost*
TabLifecycleUnitSource::TabLifecycleUnit::GetRenderProcessHost() const {
  return GetWebContents()->GetMainFrame()->GetProcess();
}

void TabLifecycleUnitSource::TabLifecycleUnit::DidStartLoading() {
  if (GetState() == State::DISCARDED) {
    SetState(State::LOADED);
    OnDiscardedStateChange();
  }
}

void TabLifecycleUnitSource::TabLifecycleUnit::OnVisibilityChanged(
    content::Visibility visibility) {
  OnLifecycleUnitVisibilityChanged(visibility);
}

}  // namespace resource_coordinator
