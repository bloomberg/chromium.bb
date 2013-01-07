// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/thumbnail_tab_helper.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/thumbnails/render_widget_snapshot_taker.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/skbitmap_operations.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ThumbnailTabHelper);

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

namespace {

// The thumbnail size in DIP.
static const int kThumbnailWidth = 212;
static const int kThumbnailHeight = 132;

static const char kThumbnailHistogramName[] = "Thumbnail.ComputeMS";

// Returns the size used by RenderWidgetHost::CopyFromBackingStore.
//
// The size is calculated in such a way that the copied size in pixel becomes
// equal to (f * kThumbnailWidth, f * kThumbnailHeight), where f is the scale
// of ui::SCALE_FACTOR_200P. Since RenderWidgetHost::CopyFromBackingStore takes
// the size in DIP, we need to adjust the size based on |view|'s device scale
// factor in order to copy the pixels with the size above.
//
// The copied size was chosen for the following reasons.
//
// 1. When the scale factor of the primary monitor is ui::SCALE_FACTOR_200P, the
// generated thumbnail size is (f * kThumbnailWidth, f * kThumbnailHeight).
// In order to avoid degrading the image quality by magnification, the size
// of the copied pixels should be equal to or larger than this thumbnail size.
//
// 2. RenderWidgetHost::CopyFromBackingStore can be costly especially when
// it is necessary to read back the web contents image data from GPU. As the
// cost is roughly propotional to the number of the copied pixels, the size of
// the copied pixels should be as small as possible.
//
// When the scale factor of the primary monitor is ui::SCALE_FACTOR_100P,
// we still copy the pixels with the same size as ui::SCALE_FACTOR_200P because
// the resampling method used in RenderWidgetHost::CopyFromBackingStore is not
// good enough for the resampled image to be used directly for the thumbnail
// (http://crbug.com/141235). We assume this is not an issue in case of
// ui::SCALE_FACTOR_200P because the high resolution thumbnail on high density
// display alleviates the aliasing.
// TODO(mazda): Copy the pixels with the smaller size in the case of
// ui::SCALE_FACTOR_100P once the resampling method has been improved.
gfx::Size GetCopySizeForThumbnail(content::RenderWidgetHostView* view) {
  gfx::Size copy_size(kThumbnailWidth, kThumbnailHeight);
  ui::ScaleFactor scale_factor =
      ui::GetScaleFactorForNativeView(view->GetNativeView());
  switch (scale_factor) {
    case ui::SCALE_FACTOR_100P:
      copy_size = gfx::ToFlooredSize(gfx::ScaleSize(
          copy_size, ui::GetScaleFactorScale(ui::SCALE_FACTOR_200P)));
      break;
    case ui::SCALE_FACTOR_200P:
      // Use the size as-is.
      break;
    default:
      DLOG(WARNING) << "Unsupported scale factor. Use the same copy size as "
                    << "ui::SCALE_FACTOR_100P";
      copy_size = gfx::ToFlooredSize(gfx::ScaleSize(
          copy_size, ui::GetScaleFactorScale(ui::SCALE_FACTOR_200P)));
      break;
  }
  return copy_size;
}

// Returns the size of the thumbnail stored in the database in pixel.
gfx::Size GetThumbnailSizeInPixel() {
  gfx::Size thumbnail_size(kThumbnailWidth, kThumbnailHeight);
  // Determine the resolution of the thumbnail based on the maximum scale
  // factor.
  // TODO(mazda|oshima): Update thumbnail when the max scale factor changes.
  // crbug.com/159157.
  float max_scale_factor =
      ui::GetScaleFactorScale(ui::GetMaxScaleFactor());
  return gfx::ToFlooredSize(gfx::ScaleSize(thumbnail_size, max_scale_factor));
}

// Returns the clipping rectangle that is used for creating a thumbnail with
// the size of |desired_size| from the bitmap with the size of |source_size|.
// The type of clipping that needs to be done is assigned to |clip_result|.
gfx::Rect GetClippingRect(const gfx::Size& source_size,
                          const gfx::Size& desired_size,
                          ThumbnailTabHelper::ClipResult* clip_result) {
  DCHECK(clip_result);

  float desired_aspect =
      static_cast<float>(desired_size.width()) / desired_size.height();

  // Get the clipping rect so that we can preserve the aspect ratio while
  // filling the destination.
  gfx::Rect clipping_rect;
  if (source_size.width() < desired_size.width() ||
      source_size.height() < desired_size.height()) {
    // Source image is smaller: we clip the part of source image within the
    // dest rect, and then stretch it to fill the dest rect. We don't respect
    // the aspect ratio in this case.
    clipping_rect = gfx::Rect(desired_size);
    *clip_result = ThumbnailTabHelper::kSourceIsSmaller;
  } else {
    float src_aspect =
        static_cast<float>(source_size.width()) / source_size.height();
    if (src_aspect > desired_aspect) {
      // Wider than tall, clip horizontally: we center the smaller
      // thumbnail in the wider screen.
      int new_width = static_cast<int>(source_size.height() * desired_aspect);
      int x_offset = (source_size.width() - new_width) / 2;
      clipping_rect.SetRect(x_offset, 0, new_width, source_size.height());
      *clip_result = (src_aspect >= ThumbnailScore::kTooWideAspectRatio) ?
          ThumbnailTabHelper::kTooWiderThanTall :
          ThumbnailTabHelper::kWiderThanTall;
    } else if (src_aspect < desired_aspect) {
      clipping_rect =
          gfx::Rect(source_size.width(), source_size.width() / desired_aspect);
      *clip_result = ThumbnailTabHelper::kTallerThanWide;
    } else {
      clipping_rect = gfx::Rect(source_size);
      *clip_result = ThumbnailTabHelper::kNotClipped;
    }
  }
  return clipping_rect;
}

// Creates a downsampled thumbnail from the given bitmap.
// store. The returned bitmap will be isNull if there was an error creating it.
SkBitmap CreateThumbnail(
    const SkBitmap& bitmap,
    const gfx::Size& desired_size,
    ThumbnailTabHelper::ClipResult* clip_result) {
  base::TimeTicks begin_compute_thumbnail = base::TimeTicks::Now();

  SkBitmap clipped_bitmap;
  if (*clip_result == ThumbnailTabHelper::kUnprocessed) {
    // Clip the pixels that will commonly hold a scrollbar, which looks bad in
    // thumbnails.
    int scrollbar_size = gfx::scrollbar_size();
    SkIRect scrollbarless_rect =
        { 0, 0,
          std::max(1, bitmap.width() - scrollbar_size),
          std::max(1, bitmap.height() - scrollbar_size) };
    SkBitmap bmp;
    bitmap.extractSubset(&bmp, scrollbarless_rect);

    clipped_bitmap = ThumbnailTabHelper::GetClippedBitmap(
        bmp, desired_size.width(), desired_size.height(), clip_result);
  } else {
    clipped_bitmap = bitmap;
  }

  // Need to resize it to the size we want, so downsample until it's
  // close, and let the caller make it the exact size if desired.
  SkBitmap result = SkBitmapOperations::DownsampleByTwoUntilSize(
      clipped_bitmap, desired_size.width(), desired_size.height());
#if !defined(USE_AURA)
  // This is a bit subtle. SkBitmaps are refcounted, but the magic
  // ones in PlatformCanvas can't be assigned to SkBitmap with proper
  // refcounting.  If the bitmap doesn't change, then the downsampler
  // will return the input bitmap, which will be the reference to the
  // weird PlatformCanvas one insetad of a regular one. To get a
  // regular refcounted bitmap, we need to copy it.
  //
  // On Aura, the PlatformCanvas is platform-independent and does not have
  // any native platform resources that can't be refounted, so this issue does
  // not occur.
  //
  // Note that GetClippedBitmap() does extractSubset() but it won't copy
  // the pixels, hence we check result size == clipped_bitmap size here.
  if (clipped_bitmap.width() == result.width() &&
      clipped_bitmap.height() == result.height())
    clipped_bitmap.copyTo(&result, SkBitmap::kARGB_8888_Config);
#endif

  HISTOGRAM_TIMES(kThumbnailHistogramName,
                  base::TimeTicks::Now() - begin_compute_thumbnail);
  return result;
}

}  // namespace

ThumbnailTabHelper::ThumbnailingContext::ThumbnailingContext(
    content::WebContents* web_contents,
    bool load_interrupted)
    : browser_context(web_contents->GetBrowserContext()),
      url(web_contents->GetURL()),
      clip_result(kUnprocessed) {
  score.at_top =
      (web_contents->GetRenderViewHost()->GetLastScrollOffset().y() == 0);
  score.load_completed = !web_contents->IsLoading() && !load_interrupted;
}

ThumbnailTabHelper::ThumbnailingContext::~ThumbnailingContext() {
}

ThumbnailTabHelper::ThumbnailTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      enabled_(true),
      load_interrupted_(false) {
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

    case content::NOTIFICATION_RENDER_VIEW_HOST_DELETED:
      RenderViewHostDeleted(content::Source<RenderViewHost>(source).ptr());
      break;

    default:
      NOTREACHED() << "Unexpected notification type: " << type;
  }
}

void ThumbnailTabHelper::RenderViewHostCreated(
    content::RenderViewHost* renderer) {
  // NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED is really a new
  // RenderView, not RenderViewHost, and there is no good way to get
  // notifications of RenderViewHosts. So just be tolerant of re-registrations.
  bool registered = registrar_.IsRegistered(
      this,
      content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::Source<RenderWidgetHost>(renderer));
  if (!registered) {
    registrar_.Add(
        this,
        content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
        content::Source<RenderWidgetHost>(renderer));
    registrar_.Add(
        this,
        content::NOTIFICATION_RENDER_VIEW_HOST_DELETED,
        content::Source<RenderViewHost>(renderer));
  }
}

void ThumbnailTabHelper::WidgetHidden(RenderWidgetHost* widget) {
  if (!enabled_)
    return;
  UpdateThumbnailIfNecessary(web_contents());
}

void ThumbnailTabHelper::RenderViewHostDeleted(
    content::RenderViewHost* renderer) {
  g_browser_process->GetRenderWidgetSnapshotTaker()->CancelSnapshot(renderer);

  registrar_.Remove(
      this,
      content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::Source<RenderWidgetHost>(renderer));
  registrar_.Remove(
      this,
      content::NOTIFICATION_RENDER_VIEW_HOST_DELETED,
      content::Source<RenderViewHost>(renderer));
}

double ThumbnailTabHelper::CalculateBoringScore(const SkBitmap& bitmap) {
  if (bitmap.isNull() || bitmap.empty())
    return 1.0;
  int histogram[256] = {0};
  color_utils::BuildLumaHistogram(bitmap, histogram);

  int color_count = *std::max_element(histogram, histogram + 256);
  int pixel_count = bitmap.width() * bitmap.height();
  return static_cast<double>(color_count) / pixel_count;
}

SkBitmap ThumbnailTabHelper::GetClippedBitmap(const SkBitmap& bitmap,
                                              int desired_width,
                                              int desired_height,
                                              ClipResult* clip_result) {
  gfx::Rect clipping_rect =
      GetClippingRect(gfx::Size(bitmap.width(), bitmap.height()),
                      gfx::Size(desired_width, desired_height),
                      clip_result);
  SkIRect src_rect = { clipping_rect.x(), clipping_rect.y(),
                       clipping_rect.right(), clipping_rect.bottom() };
  SkBitmap clipped_bitmap;
  bitmap.extractSubset(&clipped_bitmap, src_rect);
  return clipped_bitmap;
}

void ThumbnailTabHelper::UpdateThumbnailIfNecessary(
    WebContents* web_contents) {
  // Destroying a WebContents may trigger it to be hidden, prompting a snapshot
  // which would be unwise to attempt <http://crbug.com/130097>. If the
  // WebContents is in the middle of destruction, do not risk it.
  if (web_contents->IsBeingDestroyed())
    return;
  // Skip if a pending entry exists. WidgetHidden can be called while navigating
  // pages and this is not a time when thumbnails should be generated.
  if (web_contents->GetController().GetPendingEntry())
    return;
  const GURL& url = web_contents->GetURL();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service =
      ThumbnailServiceFactory::GetForProfile(profile);

  // Skip if we don't need to update the thumbnail.
  if (thumbnail_service == NULL ||
      !thumbnail_service->ShouldAcquirePageThumbnail(url)) {
    return;
  }

  AsyncUpdateThumbnail(web_contents);
}

void ThumbnailTabHelper::UpdateThumbnail(
    ThumbnailingContext* context,
    const SkBitmap& thumbnail) {
  Profile* profile =
      Profile::FromBrowserContext(context->browser_context);
  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service =
      ThumbnailServiceFactory::GetForProfile(profile);

  if (!thumbnail_service)
    return;

  context->score.boring_score = CalculateBoringScore(thumbnail);
  context->score.good_clipping =
      (context->clip_result == ThumbnailTabHelper::kWiderThanTall ||
       context->clip_result == ThumbnailTabHelper::kTallerThanWide ||
       context->clip_result == ThumbnailTabHelper::kNotClipped);

  gfx::Image image(thumbnail);
  thumbnail_service->SetPageThumbnail(context->url, image, context->score);
  VLOG(1) << "Thumbnail taken for " << context->url << ": "
          << context->score.ToString();
}

void ThumbnailTabHelper::AsyncUpdateThumbnail(
    WebContents* web_contents) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  RenderWidgetHost* render_widget_host = web_contents->GetRenderViewHost();
  content::RenderWidgetHostView* view = render_widget_host->GetView();
  if (!view)
    return;
  if (!view->IsSurfaceAvailableForCopy()) {
#if defined(OS_WIN)
    // On Windows XP, neither the backing store nor the compositing surface is
    // available in the browser when accelerated compositing is active, so ask
    // the renderer to send a snapshot for creating the thumbnail.
    if (base::win::GetVersion() < base::win::VERSION_VISTA) {
      scoped_refptr<ThumbnailingContext> context(
          new ThumbnailingContext(web_contents, load_interrupted_));
      gfx::Size view_size =
          render_widget_host->GetView()->GetViewBounds().size();
      g_browser_process->GetRenderWidgetSnapshotTaker()->AskForSnapshot(
          render_widget_host,
          base::Bind(&ThumbnailTabHelper::UpdateThumbnailWithBitmap,
                     context),
          view_size,
          view_size);
    }
#endif
    return;
  }

  gfx::Rect copy_rect = gfx::Rect(view->GetViewBounds().size());
  // Clip the pixels that will commonly hold a scrollbar, which looks bad in
  // thumbnails.
  int scrollbar_size = gfx::scrollbar_size();
  copy_rect.Inset(0, 0, scrollbar_size, scrollbar_size);

  scoped_refptr<ThumbnailingContext> context(
      new ThumbnailingContext(web_contents, load_interrupted_));
  copy_rect = GetClippingRect(copy_rect.size(),
                              gfx::Size(kThumbnailWidth, kThumbnailHeight),
                              &context->clip_result);

  gfx::Size copy_size = GetCopySizeForThumbnail(view);
  skia::PlatformBitmap* temp_bitmap = new skia::PlatformBitmap;
  render_widget_host->CopyFromBackingStore(
      copy_rect,
      copy_size,
      base::Bind(&ThumbnailTabHelper::UpdateThumbnailWithCanvas,
                 context,
                 base::Owned(temp_bitmap)),
      temp_bitmap);
}

void ThumbnailTabHelper::UpdateThumbnailWithBitmap(
    ThumbnailingContext* context,
    const SkBitmap& bitmap) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (bitmap.isNull() || bitmap.empty())
    return;

  SkBitmap thumbnail = CreateThumbnail(bitmap,
                                       GetThumbnailSizeInPixel(),
                                       &context->clip_result);

  UpdateThumbnail(context, thumbnail);
}

void ThumbnailTabHelper::UpdateThumbnailWithCanvas(
    ThumbnailingContext* context,
    skia::PlatformBitmap* temp_bitmap,
    bool succeeded) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!succeeded)
    return;

  SkBitmap bitmap = temp_bitmap->GetBitmap();
  UpdateThumbnailWithBitmap(context, bitmap);
}

void ThumbnailTabHelper::DidStartLoading(
    content::RenderViewHost* render_view_host) {
  load_interrupted_ = false;
}

void ThumbnailTabHelper::StopNavigation() {
  // This function gets called when the page loading is interrupted by the
  // stop button.
  load_interrupted_ = true;
}
