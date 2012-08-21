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
#include "ui/base/layout.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/skbitmap_operations.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

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
      copy_size =
          copy_size.Scale(ui::GetScaleFactorScale(ui::SCALE_FACTOR_200P));
      break;
    case ui::SCALE_FACTOR_200P:
      // Use the size as-is.
      break;
    default:
      DLOG(WARNING) << "Unsupported scale factor. Use the same copy size as "
                    << "ui::SCALE_FACTOR_100P";
      copy_size =
          copy_size.Scale(ui::GetScaleFactorScale(ui::SCALE_FACTOR_200P));
      break;
  }
  return copy_size;
}

// Returns the size of the thumbnail stored in the database in pixel.
gfx::Size GetThumbnailSizeInPixel() {
  gfx::Size thumbnail_size(kThumbnailWidth, kThumbnailHeight);
  // Determine the resolution of the thumbnail based on the primary monitor.
  // TODO(oshima): Use device's default scale factor.
  gfx::Display primary_display = gfx::Screen::GetPrimaryDisplay();
  return thumbnail_size.Scale(primary_display.device_scale_factor());
}

// Returns the clipping rectangle that is used for creating a thumbnail with
// the size of |desired_size| from the bitmap with the size of |source_size|.
// The type of clipping that needs to be done is assigned to |clip_result|.
gfx::Rect GetClippingRect(const gfx::Size& source_size,
                          const gfx::Size& desired_size,
                          ThumbnailGenerator::ClipResult* clip_result) {
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
    *clip_result = ThumbnailGenerator::kSourceIsSmaller;
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
          ThumbnailGenerator::kTooWiderThanTall :
          ThumbnailGenerator::kWiderThanTall;
    } else if (src_aspect < desired_aspect) {
      clipping_rect =
          gfx::Rect(source_size.width(), source_size.width() / desired_aspect);
      *clip_result = ThumbnailGenerator::kTallerThanWide;
    } else {
      clipping_rect = gfx::Rect(source_size);
      *clip_result = ThumbnailGenerator::kNotClipped;
    }
  }
  return clipping_rect;
}

// Creates a downsampled thumbnail from the given bitmap.
// store. The returned bitmap will be isNull if there was an error creating it.
SkBitmap CreateThumbnail(
    const SkBitmap& bitmap,
    const gfx::Size& desired_size,
    ThumbnailGenerator::ClipResult* clip_result) {
  base::TimeTicks begin_compute_thumbnail = base::TimeTicks::Now();

  SkBitmap clipped_bitmap;
  if (*clip_result == ThumbnailGenerator::kUnprocessed) {
    // Clip the pixels that will commonly hold a scrollbar, which looks bad in
    // thumbnails.
    int scrollbar_size = gfx::scrollbar_size();
    SkIRect scrollbarless_rect =
        { 0, 0,
          std::max(1, bitmap.width() - scrollbar_size),
          std::max(1, bitmap.height() - scrollbar_size) };
    SkBitmap bmp;
    bitmap.extractSubset(&bmp, scrollbarless_rect);

    clipped_bitmap = ThumbnailGenerator::GetClippedBitmap(
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
    registrar_.Add(this,
                   content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
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
  float scale_factor = ui::GetScaleFactorScale(ui::GetScaleFactorForNativeView(
      renderer->GetView()->GetNativeView()));
  gfx::Size desired_size_in_pixel = desired_size.Scale(scale_factor);
  scoped_ptr<TransportDIB> thumbnail_dib(TransportDIB::Create(
      desired_size_in_pixel.GetArea() * 4, sequence_num));

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
    case content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED: {
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

void ThumbnailGenerator::UpdateThumbnailIfNecessary(
    WebContents* web_contents) {
  // Skip if a pending entry exists. WidgetHidden can be called while navigaing
  // pages and this is not a timing when thumbnails should be generated.
  if (web_contents->GetController().GetPendingEntry())
    return;
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
    const ClipResult& clip_result) {

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

  gfx::Image image(thumbnail);
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
  if (!view->IsSurfaceAvailableForCopy()) {
#if defined(OS_WIN)
    // On Windows XP, neither the backing store nor the compositing surface is
    // available in the browser when accelerated compositing is active, so ask
    // the renderer to send a snapshot for creating the thumbnail.
    if (base::win::GetVersion() < base::win::VERSION_VISTA) {
      gfx::Size view_size =
          render_widget_host->GetView()->GetViewBounds().size();
      AskForSnapshot(render_widget_host,
                     base::Bind(&ThumbnailGenerator::UpdateThumbnailWithBitmap,
                                weak_factory_.GetWeakPtr(),
                                web_contents,
                                ThumbnailGenerator::kUnprocessed),
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
  ClipResult clip_result = ThumbnailGenerator::kUnprocessed;
  copy_rect = GetClippingRect(copy_rect.size(),
                              gfx::Size(kThumbnailWidth, kThumbnailHeight),
                              &clip_result);
  gfx::Size copy_size = GetCopySizeForThumbnail(view);
  skia::PlatformCanvas* temp_canvas = new skia::PlatformCanvas;
  render_widget_host->CopyFromBackingStore(
      copy_rect,
      copy_size,
      base::Bind(&ThumbnailGenerator::UpdateThumbnailWithCanvas,
                 weak_factory_.GetWeakPtr(),
                 web_contents,
                 clip_result,
                 base::Owned(temp_canvas)),
      temp_canvas);
}

void ThumbnailGenerator::UpdateThumbnailWithBitmap(
    WebContents* web_contents,
    ClipResult clip_result,
    const SkBitmap& bitmap) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (bitmap.isNull() || bitmap.empty())
    return;

  SkBitmap thumbnail = CreateThumbnail(bitmap,
                                       GetThumbnailSizeInPixel(),
                                       &clip_result);
  UpdateThumbnail(web_contents, thumbnail, clip_result);
}

void ThumbnailGenerator::UpdateThumbnailWithCanvas(
    WebContents* web_contents,
    ClipResult clip_result,
    skia::PlatformCanvas* temp_canvas,
    bool succeeded) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!succeeded)
    return;

  SkBitmap bitmap = skia::GetTopDevice(*temp_canvas)->accessBitmap(false);
  UpdateThumbnailWithBitmap(web_contents, clip_result, bitmap);
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

void ThumbnailGenerator::DidStartLoading(
    content::RenderViewHost* render_view_host) {
  load_interrupted_ = false;
}

void ThumbnailGenerator::StopNavigation() {
  // This function gets called when the page loading is interrupted by the
  // stop button.
  load_interrupted_ = true;
}
