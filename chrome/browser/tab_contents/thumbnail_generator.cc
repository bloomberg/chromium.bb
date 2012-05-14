// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/thumbnail_generator.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/thumbnail_score.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skbitmap_operations.h"

// Overview
// --------
// This class provides current thumbnails for tabs. The simplest operation is
// when a request for a thumbnail comes in, to grab the backing store and make
// a smaller version of that. Clients of the class can send such a request by
// AskForSnapshot().
//
// The class also provides a service for updating thumbnails to be used in
// "Most visited" section of the new tab page. The service can be started
// by StartThumbnailing(). The current algorithm of the service is as
// simple as follows:
//
//    When a renderer is about to be hidden (this usually occurs when the
//    current tab is closed or another tab is clicked), update the
//    thumbnail for the tab rendered by the renderer, if needed. The
//    heuristics to judge whether or not to update the thumbnail is
//    implemented in ShouldUpdateThumbnail().
//
// We'll likely revise the algorithm to improve quality of thumbnails this
// service generates.

using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;

namespace {

static const int kThumbnailWidth = 212;
static const int kThumbnailHeight = 132;

// This factor determines the number of pixels to be copied by
// RenderWidgetHost::CopyFromBackingStore for generating thumbnail.
// Smaller scale is good for performance, but too small scale causes aliasing
// because the resampling method is not good enough to retain the image quality.
// TODO(mazda): the Improve resampling method and use a smaller scale
// (http://crbug.com/118571).
static const double kThumbnailCopyScale = 2.0;

static const char kThumbnailHistogramName[] = "Thumbnail.ComputeMS";

// Calculates the size used by RenderWidgetHost::CopyFromBackingStore.
// The result is computed as the minimum size that satisfies the following
// conditions.
//   result.width : result.height == view_size.width : view_size.height
//   result.width >= kThumbnailCopyScale * desired_size.width
//   result.height >= kThumbnailCopyScale * desired_size.height
gfx::Size GetCopySizeForThumbnail(const gfx::Size& view_size,
                                  const gfx::Size& desired_size) {
  const double scale = kThumbnailCopyScale *
      std::max(static_cast<double>(desired_size.width()) / view_size.width(),
               static_cast<double>(desired_size.height()) / view_size.height());
  return gfx::Size(static_cast<int>(scale * view_size.width()),
                   static_cast<int>(scale * view_size.height()));
}

// Creates a downsampled thumbnail from the given bitmap.
// store. The returned bitmap will be isNull if there was an error creating it.
SkBitmap CreateThumbnail(
    const SkBitmap& bmp_with_scrollbars,
    int desired_width,
    int desired_height,
    int options,
    ThumbnailGenerator::ClipResult* clip_result) {
  base::TimeTicks begin_compute_thumbnail = base::TimeTicks::Now();

  // Clip the edgemost 15 pixels as that will commonly hold a scrollbar, which
  // looks bad in thumbnails.
  SkIRect scrollbarless_rect =
      { 0, 0,
        std::max(1, bmp_with_scrollbars.width() - 15),
        std::max(1, bmp_with_scrollbars.height() - 15) };
  SkBitmap bmp;
  bmp_with_scrollbars.extractSubset(&bmp, scrollbarless_rect);
  SkBitmap result;
  // Check if a clipped thumbnail is requested.
  if (options & ThumbnailGenerator::kClippedThumbnail) {
    SkBitmap clipped_bitmap = ThumbnailGenerator::GetClippedBitmap(
        bmp, desired_width, desired_height, clip_result);

    // Need to resize it to the size we want, so downsample until it's
    // close, and let the caller make it the exact size if desired.
    result = SkBitmapOperations::DownsampleByTwoUntilSize(
        clipped_bitmap, desired_width, desired_height);
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
  } else {
    // Need to resize it to the size we want, so downsample until it's
    // close, and let the caller make it the exact size if desired.
    result = SkBitmapOperations::DownsampleByTwoUntilSize(
        bmp, desired_width, desired_height);
#if !defined(USE_AURA)
    // See comments above about why we are making copy here.
    if (bmp.width() == result.width() &&
        bmp.height() == result.height())
      bmp.copyTo(&result, SkBitmap::kARGB_8888_Config);
#endif
  }

  HISTOGRAM_TIMES(kThumbnailHistogramName,
                  base::TimeTicks::Now() - begin_compute_thumbnail);
  return result;
}

}  // namespace

struct ThumbnailGenerator::AsyncRequestInfo {
  ThumbnailReadyCallback callback;
  scoped_ptr<TransportDIB> thumbnail_dib;
  RenderWidgetHost* renderer;  // Not owned.
};

ThumbnailGenerator::ThumbnailGenerator()
    : enabled_(true),
      load_interrupted_(false),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  // The BrowserProcessImpl creates this non-lazily. If you add nontrivial
  // stuff here, be sure to convert it to being lazily created.
  //
  // We don't register for notifications here since BrowserProcessImpl creates
  // us before the NotificationService is.
}

ThumbnailGenerator::~ThumbnailGenerator() {
}

void ThumbnailGenerator::StartThumbnailing(WebContents* web_contents) {
  content::WebContentsObserver::Observe(web_contents);

  if (registrar_.IsEmpty()) {
    // Even though we deal in RenderWidgetHosts, we only care about its
    // subclass, RenderViewHost when it is in a tab. We don't make thumbnails
    // for RenderViewHosts that aren't in tabs, or RenderWidgetHosts that
    // aren't views like select popups.
    registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB,
                   content::Source<WebContents>(web_contents));
    registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                   content::Source<WebContents>(web_contents));
  }
}

void ThumbnailGenerator::MonitorRenderer(RenderWidgetHost* renderer,
                                         bool monitor) {
  content::Source<RenderWidgetHost> renderer_source =
      content::Source<RenderWidgetHost>(renderer);
  bool currently_monitored =
      registrar_.IsRegistered(
        this,
        content::NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,
        renderer_source);
  if (monitor != currently_monitored) {
    if (monitor) {
      registrar_.Add(
          this,
          content::NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,
          renderer_source);
      registrar_.Add(
          this,
          content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
          renderer_source);
    } else {
      registrar_.Remove(
          this,
          content::NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,
          renderer_source);
      registrar_.Remove(
          this,
          content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
          renderer_source);
    }
  }
}

void ThumbnailGenerator::AskForSnapshot(RenderWidgetHost* renderer,
                                        const ThumbnailReadyCallback& callback,
                                        gfx::Size page_size,
                                        gfx::Size desired_size) {
  // We are going to render the thumbnail asynchronously now, so keep
  // this callback for later lookup when the rendering is done.
  static int sequence_num = 0;
  sequence_num++;
  scoped_ptr<TransportDIB> thumbnail_dib(TransportDIB::Create(
      desired_size.width() * desired_size.height() * 4, sequence_num));

#if defined(USE_X11)
  // TODO: IPC a handle to the renderer like Windows.
  // http://code.google.com/p/chromium/issues/detail?id=89777
  NOTIMPLEMENTED();
  return;
#else

#if defined(OS_WIN)
  // Duplicate the handle to the DIB here because the renderer process does not
  // have permission. The duplicated handle is owned by the renderer process,
  // which is responsible for closing it.
  TransportDIB::Handle renderer_dib_handle;
  DuplicateHandle(GetCurrentProcess(), thumbnail_dib->handle(),
                  renderer->GetProcess()->GetHandle(), &renderer_dib_handle,
                  STANDARD_RIGHTS_REQUIRED | FILE_MAP_READ | FILE_MAP_WRITE,
                  FALSE, 0);
  if (!renderer_dib_handle) {
    LOG(WARNING) << "Could not duplicate dib handle for renderer";
    return;
  }
#else
  TransportDIB::Handle renderer_dib_handle = thumbnail_dib->handle();
#endif

  linked_ptr<AsyncRequestInfo> request_info(new AsyncRequestInfo);
  request_info->callback = callback;
  request_info->thumbnail_dib.reset(thumbnail_dib.release());
  request_info->renderer = renderer;
  ThumbnailCallbackMap::value_type new_value(sequence_num, request_info);
  std::pair<ThumbnailCallbackMap::iterator, bool> result =
      callback_map_.insert(new_value);
  if (!result.second) {
    NOTREACHED() << "Callback already registered?";
    return;
  }

  renderer->PaintAtSize(
      renderer_dib_handle, sequence_num, page_size, desired_size);

#endif  // defined(USE_X11)
}

void ThumbnailGenerator::WidgetDidReceivePaintAtSizeAck(
    RenderWidgetHost* widget,
    int sequence_num,
    const gfx::Size& size) {
  // Lookup the callback, run it, and erase it.
  ThumbnailCallbackMap::iterator item = callback_map_.find(sequence_num);
  if (item != callback_map_.end()) {
    TransportDIB* dib = item->second->thumbnail_dib.get();
    DCHECK(dib);
    if (!dib || !dib->Map()) {
      return;
    }

    // Create an SkBitmap from the DIB.
    SkBitmap non_owned_bitmap;
    SkBitmap result;

    // Fill out the non_owned_bitmap with the right config.  Note that
    // this code assumes that the transport dib is a 32-bit ARGB
    // image.
    non_owned_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                               size.width(), size.height());
    non_owned_bitmap.setPixels(dib->memory());

    // Now alloc/copy the memory so we own it and can pass it around,
    // and the memory won't go away when the DIB goes away.
    // TODO: Figure out a way to avoid this copy?
    non_owned_bitmap.copyTo(&result, SkBitmap::kARGB_8888_Config);

    item->second->callback.Run(result);

    // We're done with the callback, and with the DIB, so delete both.
    callback_map_.erase(item);
  }
}

void ThumbnailGenerator::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      // Install our observer for all new RVHs.
      RenderViewHost* renderer =
          content::Details<RenderViewHost>(details).ptr();
      MonitorRenderer(renderer, true);
      break;
    }

    case content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED:
      if (!*content::Details<bool>(details).ptr())
        WidgetHidden(content::Source<RenderWidgetHost>(source).ptr());
      break;

    case content::NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK: {
      std::pair<int, gfx::Size>* size_ack_details =
          content::Details<std::pair<int, gfx::Size> >(details).ptr();
      WidgetDidReceivePaintAtSizeAck(
          content::Source<RenderWidgetHost>(source).ptr(),
          size_ack_details->first,
          size_ack_details->second);
      break;
    }

    case content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED:
      WebContentsDisconnected(content::Source<WebContents>(source).ptr());
      break;

    default:
      NOTREACHED() << "Unexpected notification type: " << type;
  }
}

void ThumbnailGenerator::WidgetHidden(RenderWidgetHost* widget) {
  // web_contents() can be NULL, if StartThumbnailing() is not called, but
  // MonitorRenderer() is called. The use case is found in
  // chrome/test/base/ui_test_utils.cc.
  if (!enabled_ || !web_contents())
    return;
  UpdateThumbnailIfNecessary(web_contents());
}

void ThumbnailGenerator::WebContentsDisconnected(WebContents* contents) {
  // Go through the existing callbacks, and find any that have the
  // same renderer as this WebContents and remove them so they don't
  // hang around.
  ThumbnailCallbackMap::iterator iterator = callback_map_.begin();
  RenderWidgetHost* renderer = contents->GetRenderViewHost();
  while (iterator != callback_map_.end()) {
    if (iterator->second->renderer == renderer) {
      ThumbnailCallbackMap::iterator nuked = iterator;
      ++iterator;
      callback_map_.erase(nuked);
      continue;
    }
    ++iterator;
  }

  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  web_contents_weak_factory_.reset(NULL);
}

double ThumbnailGenerator::CalculateBoringScore(const SkBitmap& bitmap) {
  if (bitmap.isNull() || bitmap.empty())
    return 1.0;
  int histogram[256] = {0};
  color_utils::BuildLumaHistogram(bitmap, histogram);

  int color_count = *std::max_element(histogram, histogram + 256);
  int pixel_count = bitmap.width() * bitmap.height();
  return static_cast<double>(color_count) / pixel_count;
}

SkBitmap ThumbnailGenerator::GetClippedBitmap(const SkBitmap& bitmap,
                                              int desired_width,
                                              int desired_height,
                                              ClipResult* clip_result) {
  const SkRect dest_rect = { 0, 0,
                             SkIntToScalar(desired_width),
                             SkIntToScalar(desired_height) };
  const float dest_aspect = dest_rect.width() / dest_rect.height();

  // Get the src rect so that we can preserve the aspect ratio while filling
  // the destination.
  SkIRect src_rect;
  if (bitmap.width() < dest_rect.width() ||
      bitmap.height() < dest_rect.height()) {
    // Source image is smaller: we clip the part of source image within the
    // dest rect, and then stretch it to fill the dest rect. We don't respect
    // the aspect ratio in this case.
    src_rect.set(0, 0, static_cast<S16CPU>(dest_rect.width()),
                 static_cast<S16CPU>(dest_rect.height()));
    if (clip_result)
      *clip_result = ThumbnailGenerator::kSourceIsSmaller;
  } else {
    const float src_aspect =
        static_cast<float>(bitmap.width()) / bitmap.height();
    if (src_aspect > dest_aspect) {
      // Wider than tall, clip horizontally: we center the smaller
      // thumbnail in the wider screen.
      S16CPU new_width = static_cast<S16CPU>(bitmap.height() * dest_aspect);
      S16CPU x_offset = (bitmap.width() - new_width) / 2;
      src_rect.set(x_offset, 0, new_width + x_offset, bitmap.height());
      if (clip_result) {
        *clip_result = (src_aspect >= ThumbnailScore::kTooWideAspectRatio) ?
            ThumbnailGenerator::kTooWiderThanTall :
            ThumbnailGenerator::kWiderThanTall;
      }
    } else if (src_aspect < dest_aspect) {
      src_rect.set(0, 0, bitmap.width(),
                   static_cast<S16CPU>(bitmap.width() / dest_aspect));
      if (clip_result)
        *clip_result = ThumbnailGenerator::kTallerThanWide;
    } else {
      src_rect.set(0, 0, bitmap.width(), bitmap.height());
      if (clip_result)
        *clip_result = ThumbnailGenerator::kNotClipped;
    }
  }

  SkBitmap clipped_bitmap;
  bitmap.extractSubset(&clipped_bitmap, src_rect);
  return clipped_bitmap;
}

void ThumbnailGenerator::UpdateThumbnailIfNecessary(
    WebContents* web_contents) {
  const GURL& url = web_contents->GetURL();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  history::TopSites* top_sites = profile->GetTopSites();
  // Skip if we don't need to update the thumbnail.
  if (!ShouldUpdateThumbnail(profile, top_sites, url))
    return;

  AsyncUpdateThumbnail(web_contents);
}

void ThumbnailGenerator::UpdateThumbnail(
    WebContents* web_contents, const SkBitmap& thumbnail,
    const ThumbnailGenerator::ClipResult& clip_result) {

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  history::TopSites* top_sites = profile->GetTopSites();
  if (!top_sites)
    return;

  // Compute the thumbnail score.
  ThumbnailScore score;
  score.at_top =
      (web_contents->GetRenderViewHost()->GetLastScrollOffset().y() == 0);
  score.boring_score = ThumbnailGenerator::CalculateBoringScore(thumbnail);
  score.good_clipping =
      (clip_result == ThumbnailGenerator::kWiderThanTall ||
       clip_result == ThumbnailGenerator::kTallerThanWide ||
       clip_result == ThumbnailGenerator::kNotClipped);
  score.load_completed = (!load_interrupted_ && !web_contents->IsLoading());

  gfx::Image image(new SkBitmap(thumbnail));
  const GURL& url = web_contents->GetURL();
  top_sites->SetPageThumbnail(url, &image, score);
  VLOG(1) << "Thumbnail taken for " << url << ": " << score.ToString();
}

void ThumbnailGenerator::AsyncUpdateThumbnail(
    WebContents* web_contents) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  RenderWidgetHost* render_widget_host = web_contents->GetRenderViewHost();
  content::RenderWidgetHostView* view = render_widget_host->GetView();
  if (!view)
    return;

  const gfx::Size copy_size =
      GetCopySizeForThumbnail(view->GetViewBounds().size(),
                              gfx::Size(kThumbnailWidth, kThumbnailHeight));
  skia::PlatformCanvas* temp_canvas = new skia::PlatformCanvas;
  web_contents_weak_factory_.reset(
      new base::WeakPtrFactory<WebContents>(web_contents));
  render_widget_host->CopyFromBackingStore(
      gfx::Rect(), copy_size, temp_canvas,
      base::Bind(&ThumbnailGenerator::AsyncUpdateThumbnailFinish,
                 weak_factory_.GetWeakPtr(),
                 web_contents_weak_factory_->GetWeakPtr(),
                 base::Owned(temp_canvas)));
}

void ThumbnailGenerator::AsyncUpdateThumbnailFinish(
    base::WeakPtr<WebContents> web_contents,
    skia::PlatformCanvas* temp_canvas,
    bool succeeded) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // The weak pointer can be invalidated by the subsequent AsyncUpdateThumbnail.
  if (!web_contents.get())
    return;

  if (!succeeded)
    return;

  SkBitmap bmp_with_scrollbars =
      skia::GetTopDevice(*temp_canvas)->accessBitmap(false);
  ClipResult clip_result;
  SkBitmap thumbnail = CreateThumbnail(bmp_with_scrollbars,
                                       kThumbnailWidth,
                                       kThumbnailHeight,
                                       ThumbnailGenerator::kClippedThumbnail,
                                       &clip_result);
  UpdateThumbnail(web_contents.get(), thumbnail, clip_result);
}

bool ThumbnailGenerator::ShouldUpdateThumbnail(Profile* profile,
                                               history::TopSites* top_sites,
                                               const GURL& url) {
  if (!profile || !top_sites)
    return false;
  // Skip if it's in the incognito mode.
  if (profile->IsOffTheRecord())
    return false;
  // Skip if the given URL is not appropriate for history.
  if (!HistoryService::CanAddURL(url))
    return false;
  // Skip if the top sites list is full, and the URL is not known.
  if (top_sites->IsFull() && !top_sites->IsKnownURL(url))
    return false;
  // Skip if we don't have to udpate the existing thumbnail.
  ThumbnailScore current_score;
  if (top_sites->GetPageThumbnailScore(url, &current_score) &&
      !current_score.ShouldConsiderUpdating())
    return false;
  // Skip if we don't have to udpate the temporary thumbnail (i.e. the one
  // not yet saved).
  ThumbnailScore temporary_score;
  if (top_sites->GetTemporaryPageThumbnailScore(url, &temporary_score) &&
      !temporary_score.ShouldConsiderUpdating())
    return false;

  return true;
}

void ThumbnailGenerator::DidStartLoading() {
  load_interrupted_ = false;
}

void ThumbnailGenerator::StopNavigation() {
  // This function gets called when the page loading is interrupted by the
  // stop button.
  load_interrupted_ = true;
}
