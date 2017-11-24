// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/thumbnail_tab_helper.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/scrollbar_size.h"

using thumbnails::ThumbnailingContext;

namespace {

// The desired thumbnail size in DIP. Note that on 1x devices, we actually take
// thumbnails of twice that size.
const int kThumbnailWidth = 154;
const int kThumbnailHeight = 96;

void ComputeThumbnailScore(const SkBitmap& thumbnail,
                           scoped_refptr<ThumbnailingContext> context) {
  base::TimeTicks process_bitmap_start_time = base::TimeTicks::Now();

  context->score.boring_score = color_utils::CalculateBoringScore(thumbnail);

  context->score.good_clipping =
      thumbnails::IsGoodClipping(context->clip_result);

  base::TimeDelta process_bitmap_time =
      base::TimeTicks::Now() - process_bitmap_start_time;
  UMA_HISTOGRAM_TIMES("Thumbnails.ProcessBitmapTime", process_bitmap_time);
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ThumbnailTabHelper);

// Overview
// --------
// This class provides a service for updating thumbnails to be used in the
// "Most visited" section of the New Tab page. The process is started by
// UpdateThumbnailIfNecessary(), which updates the thumbnail for the current
// tab if needed. The heuristics to judge whether to update the thumbnail are
// implemented in ThumbnailService::ShouldAcquirePageThumbnail().
// There are several triggers that can start the process:
// - When a renderer is about to be hidden (this usually occurs when the current
//   tab is closed or another tab is clicked).
// - If features::kCaptureThumbnailOnNavigatingAway is enabled: Just before
//   navigating away from the current page.

ThumbnailTabHelper::ThumbnailTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      capture_on_navigating_away_(base::FeatureList::IsEnabled(
          features::kCaptureThumbnailOnNavigatingAway)),
      did_navigation_finish_(false),
      has_received_document_since_navigation_finished_(false),
      has_painted_since_document_received_(false),
      page_transition_(ui::PAGE_TRANSITION_LINK),
      load_interrupted_(false),
      waiting_for_capture_(false),
      weak_factory_(this) {
  // Even though we deal in RenderWidgetHosts, we only care about its
  // subclass, RenderViewHost when it is in a tab. We don't make thumbnails
  // for RenderViewHosts that aren't in tabs, or RenderWidgetHosts that
  // aren't views like select popups.
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
                 content::Source<content::WebContents>(contents));
}

ThumbnailTabHelper::~ThumbnailTabHelper() = default;

void ThumbnailTabHelper::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    // TODO(treib): We should probably just override
    // WebContentsObserver::RenderViewCreated instead of relying on this
    // notification.
    case content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED:
      RenderViewHostCreated(
          content::Details<content::RenderViewHost>(details).ptr());
      break;

    case content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED:
      if (!*content::Details<bool>(details).ptr())
        WidgetHidden(content::Source<content::RenderWidgetHost>(source).ptr());
      break;

    default:
      NOTREACHED() << "Unexpected notification type: " << type;
  }
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

  if (capture_on_navigating_away_) {
    // At this point, the new navigation has just been started, but the
    // WebContents still shows the previous page. Grab a thumbnail before it
    // goes away.
    UpdateThumbnailIfNecessary(TriggerReason::NAVIGATING_AWAY);
  }

  // Now reset navigation-related state. It's important that this happens after
  // calling UpdateThumbnailIfNecessary.
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
  // TODO(treib): We should probably track whether the load *finished* - as it
  // is, an in-progress load will count as not interrupted.
  load_interrupted_ = false;
}

void ThumbnailTabHelper::NavigationStopped() {
  // This function gets called when the page loading is interrupted by the
  // stop button.
  load_interrupted_ = true;
}

void ThumbnailTabHelper::StartWatchingRenderViewHost(
    content::RenderViewHost* render_view_host) {
  // NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED is really a new
  // RenderView, not RenderViewHost, and there is no good way to get
  // notifications of RenderViewHosts. So just be tolerant of re-registrations.
  bool registered = registrar_.IsRegistered(
      this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::Source<content::RenderWidgetHost>(
          render_view_host->GetWidget()));
  if (!registered) {
    registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
                   content::Source<content::RenderWidgetHost>(
                       render_view_host->GetWidget()));
  }
}

void ThumbnailTabHelper::StopWatchingRenderViewHost(
    content::RenderViewHost* render_view_host) {
  if (!render_view_host) {
    return;
  }

  bool registered = registrar_.IsRegistered(
      this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::Source<content::RenderWidgetHost>(
          render_view_host->GetWidget()));
  if (registered) {
    registrar_.Remove(this,
                      content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
                      content::Source<content::RenderWidgetHost>(
                          render_view_host->GetWidget()));
  }
}

void ThumbnailTabHelper::UpdateThumbnailIfNecessary(TriggerReason trigger) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Don't take a screenshot if we haven't painted anything since the last
  // navigation. This can happen when navigating away again very quickly.
  if (!has_painted_since_document_received_) {
    LogThumbnailingOutcome(trigger, Outcome::NOT_ATTEMPTED_NO_PAINT_YET);
    return;
  }

  // Ignore thumbnail update requests if one is already in progress.
  if (thumbnailing_context_) {
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
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service =
      ThumbnailServiceFactory::GetForProfile(profile);

  // Skip if we don't need to update the thumbnail.
  if (!thumbnail_service ||
      !thumbnail_service->ShouldAcquirePageThumbnail(url, page_transition_)) {
    LogThumbnailingOutcome(trigger, Outcome::NOT_ATTEMPTED_SHOULD_NOT_ACQUIRE);
    return;
  }

  content::RenderWidgetHost* render_widget_host =
      web_contents()->GetRenderViewHost()->GetWidget();
  content::RenderWidgetHostView* view = render_widget_host->GetView();
  if (!view || !view->IsSurfaceAvailableForCopy()) {
    LogThumbnailingOutcome(trigger, Outcome::NOT_ATTEMPTED_VIEW_NOT_AVAILABLE);
    return;
  }

  // TODO(miu): This is the wrong size. It's the size of the view on-screen, and
  // not the rendering size of the view. This will be replaced with the view's
  // actual rendering size in a later change. http://crbug.com/73362
  gfx::Rect copy_rect = gfx::Rect(view->GetViewBounds().size());
  // Clip the pixels that will commonly hold a scrollbar, which looks bad in
  // thumbnails.
  int scrollbar_size = gfx::scrollbar_size();
  copy_rect.Inset(0, 0, scrollbar_size, scrollbar_size);

  if (copy_rect.IsEmpty()) {
    LogThumbnailingOutcome(trigger, Outcome::NOT_ATTEMPTED_EMPTY_RECT);
    return;
  }

  thumbnailing_context_ = new ThumbnailingContext(web_contents(),
                                                  thumbnail_service.get(),
                                                  load_interrupted_);

  ui::ScaleFactor scale_factor =
      ui::GetSupportedScaleFactor(
          ui::GetScaleFactorForNativeView(view->GetNativeView()));
  thumbnailing_context_->clip_result = thumbnails::GetCanvasCopyInfo(
      copy_rect.size(), scale_factor,
      gfx::Size(kThumbnailWidth, kThumbnailHeight), &copy_rect,
      &thumbnailing_context_->requested_copy_size);
  copy_from_surface_start_time_ = base::TimeTicks::Now();
  waiting_for_capture_ = true;
  view->CopyFromSurface(copy_rect, thumbnailing_context_->requested_copy_size,
                        base::Bind(&ThumbnailTabHelper::ProcessCapturedBitmap,
                                   weak_factory_.GetWeakPtr(), trigger),
                        kN32_SkColorType);
}

void ThumbnailTabHelper::ProcessCapturedBitmap(
    TriggerReason trigger,
    const SkBitmap& bitmap,
    content::ReadbackResponse response) {
  // If |waiting_for_capture_| is false, that means something happened in the
  // meantime which makes the captured image unsafe to use.
  bool was_canceled = !waiting_for_capture_;
  waiting_for_capture_ = false;

  base::TimeDelta copy_from_surface_time =
      base::TimeTicks::Now() - copy_from_surface_start_time_;
  UMA_HISTOGRAM_TIMES("Thumbnails.CopyFromSurfaceTime", copy_from_surface_time);

  if (response == content::READBACK_SUCCESS && !was_canceled) {
    // On success, we must be on the UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // From here on, nothing can fail, so log success.
    LogThumbnailingOutcome(trigger, Outcome::SUCCESS);
    base::PostTaskWithTraitsAndReply(
        FROM_HERE,
        {base::TaskPriority::BACKGROUND,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::Bind(&ComputeThumbnailScore, bitmap, thumbnailing_context_),
        base::Bind(&ThumbnailTabHelper::UpdateThumbnail,
                   weak_factory_.GetWeakPtr(), bitmap));
  } else {
    LogThumbnailingOutcome(
        trigger, was_canceled ? Outcome::CANCELED : Outcome::READBACK_FAILED);
    // On failure because of shutdown we are not on the UI thread, so ensure
    // that cleanup happens on that thread.
    // TODO(treib): Figure out whether it actually happen that we get called
    // back on something other than the UI thread.
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ThumbnailTabHelper::CleanUpFromThumbnailGeneration,
                   weak_factory_.GetWeakPtr()));
  }
}

void ThumbnailTabHelper::UpdateThumbnail(const SkBitmap& thumbnail) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Feed the constructed thumbnail to the thumbnail service.
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(thumbnail);
  thumbnailing_context_->service->SetPageThumbnail(*thumbnailing_context_,
                                                   image);
  DVLOG(1) << "Thumbnail taken for " << thumbnailing_context_->url << ": "
           << thumbnailing_context_->score.ToString();

  CleanUpFromThumbnailGeneration();
}

void ThumbnailTabHelper::CleanUpFromThumbnailGeneration() {
  // Make a note that thumbnail generation is complete.
  thumbnailing_context_ = nullptr;
}

void ThumbnailTabHelper::RenderViewHostCreated(
    content::RenderViewHost* render_view_host) {
  StartWatchingRenderViewHost(render_view_host);
}

void ThumbnailTabHelper::WidgetHidden(content::RenderWidgetHost* widget) {
  // Skip if a pending entry exists. WidgetHidden can be called while navigating
  // pages and this is not a time when thumbnails should be generated.
  if (!web_contents() || web_contents()->GetController().GetPendingEntry()) {
    LogThumbnailingOutcome(TriggerReason::TAB_HIDDEN,
                           Outcome::NOT_ATTEMPTED_PENDING_NAVIGATION);
    return;
  }
  UpdateThumbnailIfNecessary(TriggerReason::TAB_HIDDEN);
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
