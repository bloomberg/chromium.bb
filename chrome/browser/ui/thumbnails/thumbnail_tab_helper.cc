// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/thumbnail_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/scrollbar_size.h"

ThumbnailTabHelper::ThumbnailTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents) {}

ThumbnailTabHelper::~ThumbnailTabHelper() = default;

void ThumbnailTabHelper::RenderWidgetHostVisibilityChanged(
    content::RenderWidgetHost* widget_host,
    bool became_visible) {
  if (!became_visible)
    TabHidden();
}

void ThumbnailTabHelper::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  observer_.Remove(widget_host);
}

void ThumbnailTabHelper::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  StartWatchingRenderViewHost(render_view_host);
}

void ThumbnailTabHelper::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  StopWatchingRenderViewHost(old_host);
  StartWatchingRenderViewHost(new_host);
}

void ThumbnailTabHelper::RenderViewDeleted(
    content::RenderViewHost* render_view_host) {
  StopWatchingRenderViewHost(render_view_host);
}

void ThumbnailTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // At this point, the new navigation has just been started, but the
  // WebContents still shows the previous page. Grab a thumbnail before it
  // goes away.
  StartThumbnailCaptureIfNecessary(TriggerReason::NAVIGATING_AWAY);

  // Now reset navigation-related state. It's important that this happens after
  // calling StartThumbnailCaptureIfNecessary.
  did_navigation_finish_ = false;
  has_received_document_since_navigation_finished_ = false;
  has_painted_since_document_received_ = false;
  // Reset the page transition to some uninteresting type, since the actual
  // type isn't available at this point. We'll get it in DidFinishNavigation
  // (if that happens, which isn't guaranteed).
  page_transition_ = ui::PAGE_TRANSITION_LINK;
}

void ThumbnailTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  did_navigation_finish_ = true;
  page_transition_ = navigation_handle->GetPageTransition();
}

void ThumbnailTabHelper::DocumentAvailableInMainFrame() {
  // If there's currently a screen capture going on, ignore its result.
  // Otherwise there's a risk that we'll get a picture of the wrong page.
  // Note: It *looks* like WebContentsObserver::DidFirstVisuallyNonEmptyPaint
  // would be a better signal for this, but it uses a weird heuristic to detect
  // "visually non empty" paints, so it might not be entirely safe.
  waiting_for_capture_ = false;

  // Mark that we got the document, unless we're in the middle of a navigation.
  // In that case, this refers to the previous document, but we're tracking the
  // state of the new one.
  if (did_navigation_finish_) {
    // From now on, we'll start watching for paint events.
    has_received_document_since_navigation_finished_ = true;
  }
}

void ThumbnailTabHelper::DocumentOnLoadCompletedInMainFrame() {
  // Usually, DocumentAvailableInMainFrame always gets called first, so this one
  // shouldn't be necessary. However, DocumentAvailableInMainFrame is not fired
  // for empty documents (i.e. about:blank), which are thus handled here.
  DocumentAvailableInMainFrame();
}

void ThumbnailTabHelper::DidFirstVisuallyNonEmptyPaint() {
  // If we haven't gotten the current document since navigating, then this paint
  // refers to the *previous* document, so ignore it.
  if (has_received_document_since_navigation_finished_) {
    has_painted_since_document_received_ = true;
  }
}

void ThumbnailTabHelper::DidStartLoading() {
  load_interrupted_ = false;
}

void ThumbnailTabHelper::NavigationStopped() {
  // This function gets called when the page loading is interrupted by the
  // stop button.
  load_interrupted_ = true;
}

void ThumbnailTabHelper::StartWatchingRenderViewHost(
    content::RenderViewHost* render_view_host) {
  // We get notified whenever a new RenderView is created, which does not
  // necessarily come with a new RenderViewHost, and there is no good way to get
  // notifications of new RenderViewHosts only. So just be tolerant of
  // re-registrations.
  content::RenderWidgetHost* render_widget_host = render_view_host->GetWidget();
  if (!observer_.IsObserving(render_widget_host))
    observer_.Add(render_widget_host);
}

void ThumbnailTabHelper::StopWatchingRenderViewHost(
    content::RenderViewHost* render_view_host) {
  if (!render_view_host) {
    return;
  }

  content::RenderWidgetHost* render_widget_host = render_view_host->GetWidget();
  if (observer_.IsObserving(render_widget_host))
    observer_.Remove(render_widget_host);
}

void ThumbnailTabHelper::StartThumbnailCaptureIfNecessary(
    TriggerReason trigger) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Don't take a screenshot if we haven't painted anything since the last
  // navigation. This can happen when navigating away again very quickly.
  if (!has_painted_since_document_received_) {
    LogThumbnailingOutcome(trigger, Outcome::NOT_ATTEMPTED_NO_PAINT_YET);
    return;
  }

  // Ignore thumbnail update requests if one is already in progress.
  if (thumbnailing_in_progress_) {
    LogThumbnailingOutcome(trigger, Outcome::NOT_ATTEMPTED_IN_PROGRESS);
    return;
  }

  // Destroying a WebContents may trigger it to be hidden, prompting a snapshot
  // which would be unwise to attempt <http://crbug.com/130097>. If the
  // WebContents is in the middle of destruction, do not risk it.
  if (!web_contents() || web_contents()->IsBeingDestroyed()) {
    LogThumbnailingOutcome(trigger, Outcome::NOT_ATTEMPTED_NO_WEBCONTENTS);
    return;
  }

  // Note: Do *not* use GetLastVisibleURL - it might already have been updated
  // for a new pending navigation. The committed URL is the one corresponding
  // to the currently visible content.
  const GURL& url = web_contents()->GetLastCommittedURL();
  if (!url.is_valid()) {
    LogThumbnailingOutcome(trigger, Outcome::NOT_ATTEMPTED_NO_URL);
    return;
  }

  // Check if the thumbnail needs to be updated. If not, log and return.

  content::RenderWidgetHost* render_widget_host =
      web_contents()->GetRenderViewHost()->GetWidget();
  content::RenderWidgetHostView* view = render_widget_host->GetView();
  if (!view || !view->IsSurfaceAvailableForCopy()) {
    LogThumbnailingOutcome(trigger, Outcome::NOT_ATTEMPTED_VIEW_NOT_AVAILABLE);
    return;
  }

  // Note: this is the size in pixels on-screen, not the size in DIPs.
  gfx::Size source_size = view->GetViewBounds().size();
  // Clip the pixels that will commonly hold a scrollbar, which looks bad in
  // thumbnails.
  const float scale_factor = view->GetDeviceScaleFactor();
  const int scrollbar_size = gfx::scrollbar_size() * scale_factor;
  source_size.Enlarge(-scrollbar_size, -scrollbar_size);

  if (source_size.IsEmpty()) {
    LogThumbnailingOutcome(trigger, Outcome::NOT_ATTEMPTED_EMPTY_RECT);
    return;
  }

  thumbnailing_in_progress_ = true;

  const gfx::Size desired_size = TabStyle::GetPreviewImageSize();
  thumbnails::CanvasCopyInfo copy_info =
      thumbnails::GetCanvasCopyInfo(source_size, scale_factor, desired_size);
  copy_from_surface_start_time_ = base::TimeTicks::Now();
  waiting_for_capture_ = true;
  view->CopyFromSurface(
      copy_info.copy_rect, copy_info.target_size,
      base::BindOnce(&ThumbnailTabHelper::ProcessCapturedBitmap,
                     weak_factory_.GetWeakPtr(), trigger));
}

void ThumbnailTabHelper::ProcessCapturedBitmap(TriggerReason trigger,
                                               const SkBitmap& bitmap) {
  // If |waiting_for_capture_| is false, that means something happened in the
  // meantime which makes the captured image unsafe to use.
  bool was_canceled = !waiting_for_capture_;
  waiting_for_capture_ = false;

  base::TimeDelta copy_from_surface_time =
      base::TimeTicks::Now() - copy_from_surface_start_time_;
  UMA_HISTOGRAM_TIMES("Thumbnails.CopyFromSurfaceTime", copy_from_surface_time);

  if (!bitmap.drawsNothing() && !was_canceled) {
    // On success, we must be on the UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // From here on, nothing can fail, so log success.
    LogThumbnailingOutcome(trigger, Outcome::SUCCESS);
    thumbnail_ = ThumbnailImage::FromSkBitmap(bitmap);
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  } else {
    LogThumbnailingOutcome(
        trigger, was_canceled ? Outcome::CANCELED : Outcome::READBACK_FAILED);
  }
  thumbnailing_in_progress_ = false;
}

void ThumbnailTabHelper::TabHidden() {
  // Skip if a pending entry exists. TabHidden can be called while navigating
  // pages and this is not a time when thumbnails should be generated.
  if (!web_contents() || web_contents()->GetController().GetPendingEntry()) {
    LogThumbnailingOutcome(TriggerReason::TAB_HIDDEN,
                           Outcome::NOT_ATTEMPTED_PENDING_NAVIGATION);
    return;
  }
  StartThumbnailCaptureIfNecessary(TriggerReason::TAB_HIDDEN);
}

// static
void ThumbnailTabHelper::LogThumbnailingOutcome(TriggerReason trigger,
                                                Outcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Thumbnails.CaptureOutcome", outcome,
                            Outcome::COUNT);

  switch (trigger) {
    case TriggerReason::TAB_HIDDEN:
      UMA_HISTOGRAM_ENUMERATION("Thumbnails.CaptureOutcome.TabHidden", outcome,
                                Outcome::COUNT);
      break;
    case TriggerReason::NAVIGATING_AWAY:
      UMA_HISTOGRAM_ENUMERATION("Thumbnails.CaptureOutcome.NavigatingAway",
                                outcome, Outcome::COUNT);
      break;
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ThumbnailTabHelper)
