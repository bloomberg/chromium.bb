// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/thumbnail_generator.h"

#include <algorithm>
#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/thumbnail_score.h"
#include "content/browser/renderer_host/backing_store.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/property_bag.h"
#include "googleurl/src/gurl.h"
#include "skia/ext/bitmap_platform_device.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skbitmap_operations.h"

#if defined(OS_WIN)
#include "content/common/section_util_win.h"
#endif

// Overview
// --------
// This class provides current thumbnails for tabs. The simplest operation is
// when a request for a thumbnail comes in, to grab the backing store and make
// a smaller version of that. Clients of the class can send such a request by
// GetThumbnailForRenderer() and AskForSnapshot().
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

namespace {

static const int kThumbnailWidth = 212;
static const int kThumbnailHeight = 132;

static const char kThumbnailHistogramName[] = "Thumbnail.ComputeMS";

// Returns a property accessor used for attaching a TabContents to a
// RenderWidgetHost. We maintain the RenderWidgetHost to TabContents
// mapping so that we can retrieve a TabContents from a RenderWidgetHost
PropertyAccessor<TabContents*>* GetTabContentsAccessor() {
  static PropertyAccessor<TabContents*> accessor;
  return &accessor;
}

// Creates a downsampled thumbnail for the given backing store. The returned
// bitmap will be isNull if there was an error creating it.
SkBitmap GetBitmapForBackingStore(
    BackingStore* backing_store,
    int desired_width,
    int desired_height,
    int options,
    ThumbnailGenerator::ClipResult* clip_result) {
  base::TimeTicks begin_compute_thumbnail = base::TimeTicks::Now();

  SkBitmap result;

  // Get the bitmap as a Skia object so we can resample it. This is a large
  // allocation and we can tolerate failure here, so give up if the allocation
  // fails.
  skia::PlatformCanvas temp_canvas;
  if (!backing_store->CopyFromBackingStore(gfx::Rect(backing_store->size()),
                                           &temp_canvas))
    return result;
  const SkBitmap& bmp = temp_canvas.getTopPlatformDevice().accessBitmap(false);

  // Check if a clipped thumbnail is requested.
  if (options & ThumbnailGenerator::kClippedThumbnail) {
    SkBitmap clipped_bitmap = ThumbnailGenerator::GetClippedBitmap(
        bmp, desired_width, desired_height, clip_result);

    // Need to resize it to the size we want, so downsample until it's
    // close, and let the caller make it the exact size if desired.
    result = SkBitmapOperations::DownsampleByTwoUntilSize(
        clipped_bitmap, desired_width, desired_height);
  } else {
    // Need to resize it to the size we want, so downsample until it's
    // close, and let the caller make it the exact size if desired.
    result = SkBitmapOperations::DownsampleByTwoUntilSize(
        bmp, desired_width, desired_height);

    // This is a bit subtle. SkBitmaps are refcounted, but the magic
    // ones in PlatformCanvas can't be assigned to SkBitmap with proper
    // refcounting.  If the bitmap doesn't change, then the downsampler
    // will return the input bitmap, which will be the reference to the
    // weird PlatformCanvas one insetad of a regular one. To get a
    // regular refcounted bitmap, we need to copy it.
    if (bmp.width() == result.width() &&
        bmp.height() == result.height())
      bmp.copyTo(&result, SkBitmap::kARGB_8888_Config);
  }

  HISTOGRAM_TIMES(kThumbnailHistogramName,
                  base::TimeTicks::Now() - begin_compute_thumbnail);
  return result;
}

}  // namespace

struct ThumbnailGenerator::AsyncRequestInfo {
  scoped_ptr<ThumbnailReadyCallback> callback;
  scoped_ptr<TransportDIB> thumbnail_dib;
  RenderWidgetHost* renderer;  // Not owned.
};

ThumbnailGenerator::ThumbnailGenerator() {
  // The BrowserProcessImpl creates this non-lazily. If you add nontrivial
  // stuff here, be sure to convert it to being lazily created.
  //
  // We don't register for notifications here since BrowserProcessImpl creates
  // us before the NotificationService is.
}

ThumbnailGenerator::~ThumbnailGenerator() {
}

void ThumbnailGenerator::StartThumbnailing() {
  if (registrar_.IsEmpty()) {
    // Even though we deal in RenderWidgetHosts, we only care about its
    // subclass, RenderViewHost when it is in a tab. We don't make thumbnails
    // for RenderViewHosts that aren't in tabs, or RenderWidgetHosts that
    // aren't views like select popups.
    registrar_.Add(this, NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::RENDER_WIDGET_VISIBILITY_CHANGED,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::TAB_CONTENTS_DISCONNECTED,
                   NotificationService::AllSources());
  }
}

void ThumbnailGenerator::MonitorRenderer(RenderWidgetHost* renderer,
                                         bool monitor) {
  Source<RenderWidgetHost> renderer_source = Source<RenderWidgetHost>(renderer);
  bool currently_monitored =
      registrar_.IsRegistered(
        this,
        NotificationType::RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,
        renderer_source);
  if (monitor != currently_monitored) {
    if (monitor) {
      registrar_.Add(
          this,
          NotificationType::RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,
          renderer_source);
    } else {
      registrar_.Remove(
          this,
          NotificationType::RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,
          renderer_source);
    }
  }
}

void ThumbnailGenerator::AskForSnapshot(RenderWidgetHost* renderer,
                                        bool prefer_backing_store,
                                        ThumbnailReadyCallback* callback,
                                        gfx::Size page_size,
                                        gfx::Size desired_size) {
  if (prefer_backing_store) {
    BackingStore* backing_store = renderer->GetBackingStore(false);
    if (backing_store) {
      // We were able to find a non-null backing store for this renderer, so
      // we'll go with it.
      SkBitmap first_try = GetBitmapForBackingStore(backing_store,
                                                    desired_size.width(),
                                                    desired_size.height(),
                                                    kNoOptions,
                                                    NULL);
      callback->Run(first_try);

      delete callback;
      return;
    }
    // Now, if the backing store didn't exist, we will still try and
    // render asynchronously.
  }

  // We are going to render the thumbnail asynchronously now, so keep
  // this callback for later lookup when the rendering is done.
  static int sequence_num = 0;
  sequence_num++;
  scoped_ptr<TransportDIB> thumbnail_dib(TransportDIB::Create(
      desired_size.width() * desired_size.height() * 4, sequence_num));

#if defined(OS_WIN)
  // Duplicate the handle to the DIB here because the renderer process does not
  // have permission. The duplicated handle is owned by the renderer process,
  // which is responsible for closing it.
  TransportDIB::Handle renderer_dib_handle = chrome::GetSectionForProcess(
      thumbnail_dib->handle(),
      renderer->process()->GetHandle(),
      false);
  if (!renderer_dib_handle) {
    LOG(WARNING) << "Could not duplicate dib handle for renderer";
    delete callback;
    return;
  }
#else
  TransportDIB::Handle renderer_dib_handle = thumbnail_dib->handle();
#endif

  linked_ptr<AsyncRequestInfo> request_info(new AsyncRequestInfo);
  request_info->callback.reset(callback);
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
}

SkBitmap ThumbnailGenerator::GetThumbnailForRenderer(
    RenderWidgetHost* renderer) const {
  return GetThumbnailForRendererWithOptions(renderer, kNoOptions, NULL);
}

SkBitmap ThumbnailGenerator::GetThumbnailForRendererWithOptions(
    RenderWidgetHost* renderer,
    int options,
    ClipResult* clip_result) const {
  BackingStore* backing_store = renderer->GetBackingStore(false);
  if (!backing_store) {
    // When we have no backing store, there's no choice in what to use. We
    // have to return the empty thumbnail.
    return SkBitmap();
  }

  return GetBitmapForBackingStore(backing_store,
                                  kThumbnailWidth,
                                  kThumbnailHeight,
                                  options,
                                  clip_result);
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

    item->second->callback->Run(result);

    // We're done with the callback, and with the DIB, so delete both.
    callback_map_.erase(item);
  }
}

void ThumbnailGenerator::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      // Install our observer for all new RVHs.
      RenderViewHost* renderer = Details<RenderViewHost>(details).ptr();
      TabContents* contents = Source<TabContents>(source).ptr();
      MonitorRenderer(renderer, true);
      // Attach the tab contents to the renderer.
      // TODO(satorux): Rework this code. This relies on some internals of
      // how TabContents and RVH work. We should make this class
      // per-tab. See also crbug.com/78990.
      GetTabContentsAccessor()->SetProperty(
          renderer->property_bag(), contents);
      VLOG(1) << "renderer " << renderer << "is created for tab " << contents;
      break;
    }

    case NotificationType::RENDER_WIDGET_VISIBILITY_CHANGED:
      if (!*Details<bool>(details).ptr())
        WidgetHidden(Source<RenderWidgetHost>(source).ptr());
      break;

    case NotificationType::RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK: {
      RenderWidgetHost::PaintAtSizeAckDetails* size_ack_details =
          Details<RenderWidgetHost::PaintAtSizeAckDetails>(details).ptr();
      WidgetDidReceivePaintAtSizeAck(
          Source<RenderWidgetHost>(source).ptr(),
          size_ack_details->tag,
          size_ack_details->size);
      break;
    }

    case NotificationType::TAB_CONTENTS_DISCONNECTED:
      TabContentsDisconnected(Source<TabContents>(source).ptr());
      break;

    default:
      NOTREACHED() << "Unexpected notification type: " << type.value;
  }
}

void ThumbnailGenerator::WidgetHidden(RenderWidgetHost* widget) {
  // Retrieve the tab contents rendered by the widget.
  TabContents** property = GetTabContentsAccessor()->GetProperty(
      widget->property_bag());
  if (!property) {
    LOG(ERROR) << "This widget is not associated with tab contents: "
               << widget;
    return;
  }
  TabContents* contents = *property;
  UpdateThumbnailIfNecessary(contents);
}

void ThumbnailGenerator::TabContentsDisconnected(TabContents* contents) {
  // Go through the existing callbacks, and find any that have the
  // same renderer as this TabContents and remove them so they don't
  // hang around.
  ThumbnailCallbackMap::iterator iterator = callback_map_.begin();
  RenderWidgetHost* renderer = contents->render_view_host();
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

double ThumbnailGenerator::CalculateBoringScore(SkBitmap* bitmap) {
  if (bitmap->isNull() || bitmap->empty())
    return 1.0;
  int histogram[256] = {0};
  color_utils::BuildLumaHistogram(bitmap, histogram);

  int color_count = *std::max_element(histogram, histogram + 256);
  int pixel_count = bitmap->width() * bitmap->height();
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
      if (clip_result)
        *clip_result = ThumbnailGenerator::kWiderThanTall;
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
    TabContents* tab_contents) {
  const GURL& url = tab_contents->GetURL();
  history::TopSites* top_sites = tab_contents->profile()->GetTopSites();
  // Skip if we don't need to update the thumbnail.
  if (!ShouldUpdateThumbnail(tab_contents->profile(), top_sites, url))
    return;

  const int options = ThumbnailGenerator::kClippedThumbnail;
  ThumbnailGenerator::ClipResult clip_result = ThumbnailGenerator::kNotClipped;
  SkBitmap thumbnail = GetThumbnailForRendererWithOptions(
      tab_contents->render_view_host(), options, &clip_result);
  // Failed to generate a thumbnail. Maybe the tab is in the background?
  if (thumbnail.isNull())
    return;

  // Compute the thumbnail score.
  ThumbnailScore score;
  score.at_top =
      (tab_contents->render_view_host()->last_scroll_offset().y() == 0);
  score.boring_score = ThumbnailGenerator::CalculateBoringScore(&thumbnail);
  score.good_clipping =
      (clip_result == ThumbnailGenerator::kTallerThanWide ||
       clip_result == ThumbnailGenerator::kNotClipped);

  top_sites->SetPageThumbnail(url, thumbnail, score);
  VLOG(1) << "Thumbnail taken for " << url << ": " << score.ToString();
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
