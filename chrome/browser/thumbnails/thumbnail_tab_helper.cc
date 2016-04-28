// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/thumbnail_tab_helper.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/thumbnails/thumbnailing_algorithm.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/skbitmap_operations.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ThumbnailTabHelper);

class SkBitmap;

// Overview
// --------
// This class provides a service for updating thumbnails to be used in
// "Most visited" section of the new tab page. The service can be started
// by StartThumbnailing(). The current algorithm of the service is as
// simple as follows:
//
//    When a renderer is about to be hidden (this usually occurs when the
//    current tab is closed or another tab is clicked), update the
//    thumbnail for the tab rendered by the renderer, if needed. The
//    heuristics to judge whether or not to update the thumbnail is
//    implemented in ShouldUpdateThumbnail().

using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;

using thumbnails::ClipResult;
using thumbnails::ThumbnailingContext;
using thumbnails::ThumbnailingAlgorithm;

ThumbnailTabHelper::ThumbnailTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      load_interrupted_(false),
      weak_factory_(this) {
  // Even though we deal in RenderWidgetHosts, we only care about its
  // subclass, RenderViewHost when it is in a tab. We don't make thumbnails
  // for RenderViewHosts that aren't in tabs, or RenderWidgetHosts that
  // aren't views like select popups.
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
                 content::Source<WebContents>(contents));
}

ThumbnailTabHelper::~ThumbnailTabHelper() {
}

void ThumbnailTabHelper::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED:
      RenderViewHostCreated(content::Details<RenderViewHost>(details).ptr());
      break;

    case content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED:
      if (!*content::Details<bool>(details).ptr())
        WidgetHidden(content::Source<RenderWidgetHost>(source).ptr());
      break;

    default:
      NOTREACHED() << "Unexpected notification type: " << type;
  }
}

void ThumbnailTabHelper::RenderViewDeleted(
    content::RenderViewHost* render_view_host) {
  bool registered = registrar_.IsRegistered(
      this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::Source<RenderWidgetHost>(render_view_host->GetWidget()));
  if (registered) {
    registrar_.Remove(
        this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
        content::Source<RenderWidgetHost>(render_view_host->GetWidget()));
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

void ThumbnailTabHelper::UpdateThumbnailIfNecessary() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Ignore thumbnail update requests if one is already in progress. This can
  // happen at the end of thumbnail generation when
  // CleanUpFromThumbnailGeneration() calls DecrementCapturerCount(), triggering
  // a call to content::WebContentsImpl::WasHidden() which eventually calls
  // ThumbnailTabHelper::UpdateThumbnailIfNecessary().
  if (thumbnailing_context_) {
    return;
  }

  // Destroying a WebContents may trigger it to be hidden, prompting a snapshot
  // which would be unwise to attempt <http://crbug.com/130097>. If the
  // WebContents is in the middle of destruction, do not risk it.
  if (!web_contents() || web_contents()->IsBeingDestroyed())
    return;
  // Skip if a pending entry exists. WidgetHidden can be called while navigating
  // pages and this is not a time when thumbnails should be generated.
  if (web_contents()->GetController().GetPendingEntry())
    return;
  const GURL& url = web_contents()->GetURL();
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service =
      ThumbnailServiceFactory::GetForProfile(profile);

  // Skip if we don't need to update the thumbnail.
  if (thumbnail_service.get() == NULL ||
      !thumbnail_service->ShouldAcquirePageThumbnail(url)) {
    return;
  }

  // Prevent the web contents from disappearing before the async thumbnail
  // generation code executes. See https://crbug.com/530707 .
  web_contents()->IncrementCapturerCount(gfx::Size());

  AsyncProcessThumbnail(thumbnail_service);
}

void ThumbnailTabHelper::AsyncProcessThumbnail(
    scoped_refptr<thumbnails::ThumbnailService> thumbnail_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RenderWidgetHost* render_widget_host =
      web_contents()->GetRenderViewHost()->GetWidget();
  content::RenderWidgetHostView* view = render_widget_host->GetView();
  if (!view || !view->IsSurfaceAvailableForCopy()) {
    return;
  }

  gfx::Rect copy_rect = gfx::Rect(view->GetViewBounds().size());
  // Clip the pixels that will commonly hold a scrollbar, which looks bad in
  // thumbnails.
  int scrollbar_size = gfx::scrollbar_size();
  gfx::Size copy_size;
  copy_rect.Inset(0, 0, scrollbar_size, scrollbar_size);

  if (copy_rect.IsEmpty()) {
    return;
  }

  scoped_refptr<thumbnails::ThumbnailingAlgorithm> algorithm(
      thumbnail_service->GetThumbnailingAlgorithm());

  thumbnailing_context_ = new ThumbnailingContext(web_contents(),
                                                  thumbnail_service.get(),
                                                  load_interrupted_);

  ui::ScaleFactor scale_factor =
      ui::GetSupportedScaleFactor(
          ui::GetScaleFactorForNativeView(view->GetNativeView()));
  thumbnailing_context_->clip_result = algorithm->GetCanvasCopyInfo(
      copy_rect.size(),
      scale_factor,
      &copy_rect,
      &thumbnailing_context_->requested_copy_size);
  render_widget_host->CopyFromBackingStore(
      copy_rect,
      thumbnailing_context_->requested_copy_size,
      base::Bind(&ThumbnailTabHelper::ProcessCapturedBitmap,
                 weak_factory_.GetWeakPtr(),
                 algorithm),
      kN32_SkColorType);
}

void ThumbnailTabHelper::ProcessCapturedBitmap(
    scoped_refptr<thumbnails::ThumbnailingAlgorithm> algorithm,
    const SkBitmap& bitmap,
    content::ReadbackResponse response) {
  if (response == content::READBACK_SUCCESS) {
    // On success, we must be on the UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    algorithm->ProcessBitmap(thumbnailing_context_,
                             base::Bind(&ThumbnailTabHelper::UpdateThumbnail,
                                        weak_factory_.GetWeakPtr()),
                             bitmap);
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

void ThumbnailTabHelper::CleanUpFromThumbnailGeneration() {
  if (web_contents()) {
    // Balance the call to IncrementCapturerCount() made in
    // UpdateThumbnailIfNecessary().
    web_contents()->DecrementCapturerCount();
  }

  // Make a note that thumbnail generation is complete.
  thumbnailing_context_ = nullptr;
}

void ThumbnailTabHelper::UpdateThumbnail(
    const thumbnails::ThumbnailingContext& context,
    const SkBitmap& thumbnail) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Feed the constructed thumbnail to the thumbnail service.
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(thumbnail);
  context.service->SetPageThumbnail(context, image);
  DVLOG(1) << "Thumbnail taken for " << context.url << ": "
      << context.score.ToString();

  CleanUpFromThumbnailGeneration();
}

void ThumbnailTabHelper::RenderViewHostCreated(
    content::RenderViewHost* renderer) {
  // NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED is really a new
  // RenderView, not RenderViewHost, and there is no good way to get
  // notifications of RenderViewHosts. So just be tolerant of re-registrations.
  bool registered = registrar_.IsRegistered(
      this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::Source<RenderWidgetHost>(renderer->GetWidget()));
  if (!registered) {
    registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
                   content::Source<RenderWidgetHost>(renderer->GetWidget()));
  }
}

void ThumbnailTabHelper::WidgetHidden(RenderWidgetHost* widget) {
  UpdateThumbnailIfNecessary();
}
