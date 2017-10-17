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
      page_transition_(ui::PAGE_TRANSITION_LINK),
      load_interrupted_(false),
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

void ThumbnailTabHelper::RenderViewDeleted(
    content::RenderViewHost* render_view_host) {
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
    UpdateThumbnailIfNecessary();
  }
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
  page_transition_ = navigation_handle->GetPageTransition();
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

void ThumbnailTabHelper::UpdateThumbnailIfNecessary() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Ignore thumbnail update requests if one is already in progress.
  if (thumbnailing_context_) {
    return;
  }

  // Destroying a WebContents may trigger it to be hidden, prompting a snapshot
  // which would be unwise to attempt <http://crbug.com/130097>. If the
  // WebContents is in the middle of destruction, do not risk it.
  if (!web_contents() || web_contents()->IsBeingDestroyed()) {
    return;
  }

  // Note: Do *not* use GetLastVisibleURL - it might already have been updated
  // for a new pending navigation. The committed URL is the one corresponding
  // to the currently visible content.
  const GURL& url = web_contents()->GetLastCommittedURL();
  if (!url.is_valid()) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service =
      ThumbnailServiceFactory::GetForProfile(profile);

  // Skip if we don't need to update the thumbnail.
  if (!thumbnail_service ||
      !thumbnail_service->ShouldAcquirePageThumbnail(url, page_transition_)) {
    return;
  }

  AsyncProcessThumbnail(thumbnail_service);
}

void ThumbnailTabHelper::AsyncProcessThumbnail(
    scoped_refptr<thumbnails::ThumbnailService> thumbnail_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderWidgetHost* render_widget_host =
      web_contents()->GetRenderViewHost()->GetWidget();
  content::RenderWidgetHostView* view = render_widget_host->GetView();
  if (!view || !view->IsSurfaceAvailableForCopy()) {
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
  view->CopyFromSurface(copy_rect, thumbnailing_context_->requested_copy_size,
                        base::Bind(&ThumbnailTabHelper::ProcessCapturedBitmap,
                                   weak_factory_.GetWeakPtr()),
                        kN32_SkColorType);
}

void ThumbnailTabHelper::ProcessCapturedBitmap(
    const SkBitmap& bitmap,
    content::ReadbackResponse response) {
  base::TimeDelta copy_from_surface_time =
      base::TimeTicks::Now() - copy_from_surface_start_time_;
  UMA_HISTOGRAM_TIMES("Thumbnails.CopyFromSurfaceTime", copy_from_surface_time);

  if (response == content::READBACK_SUCCESS) {
    // On success, we must be on the UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    base::PostTaskWithTraitsAndReply(
        FROM_HERE,
        {base::TaskPriority::BACKGROUND,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::Bind(&ComputeThumbnailScore, bitmap, thumbnailing_context_),
        base::Bind(&ThumbnailTabHelper::UpdateThumbnail,
                   weak_factory_.GetWeakPtr(), bitmap));
  } else {
    // On failure because of shutdown we are not on the UI thread, so ensure
    // that cleanup happens on that thread.
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
    content::RenderViewHost* renderer) {
  // NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED is really a new
  // RenderView, not RenderViewHost, and there is no good way to get
  // notifications of RenderViewHosts. So just be tolerant of re-registrations.
  bool registered = registrar_.IsRegistered(
      this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::Source<content::RenderWidgetHost>(renderer->GetWidget()));
  if (!registered) {
    registrar_.Add(
        this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
        content::Source<content::RenderWidgetHost>(renderer->GetWidget()));
  }
}

void ThumbnailTabHelper::WidgetHidden(content::RenderWidgetHost* widget) {
  // Skip if a pending entry exists. WidgetHidden can be called while navigating
  // pages and this is not a time when thumbnails should be generated.
  if (!web_contents() || web_contents()->GetController().GetPendingEntry()) {
    return;
  }
  UpdateThumbnailIfNecessary();
}
