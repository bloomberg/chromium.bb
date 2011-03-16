// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/thumbnail_generator.h"

#include <algorithm>
#include <map>

#include "base/metrics/histogram.h"
#include "base/scoped_ptr.h"
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
// a smaller version of that.
//
// A complication happens because we don't always have nice backing stores for
// all tabs (there is a cache of several tabs we'll keep backing stores for).
// To get thumbnails for tabs with expired backing stores, we listen for
// backing stores that are being thrown out, and generate thumbnails before
// that happens. We attach them to the RenderWidgetHost via the property bag
// so we can retrieve them later. When a tab has a live backing store again,
// we throw away the thumbnail since it's now out-of-date.
//
// Another complication is performance. If the user brings up a tab switcher, we
// don't want to get all 5 cached backing stores since it is a very large amount
// of data. As a result, we generate thumbnails for tabs that are hidden even
// if the backing store is still valid. This means we'll have to do a maximum
// of generating thumbnails for the visible tabs at any point.
//
// The last performance consideration is when the user switches tabs quickly.
// This can happen by doing Control-PageUp/Down or juct clicking quickly on
// many different tabs (like when you're looking for one). We don't want to
// slow this down by making thumbnails for each tab as it's hidden. Therefore,
// we have a timer so that we don't invalidate thumbnails for tabs that are
// only shown briefly (which would cause the thumbnail to be regenerated when
// the tab is hidden).

namespace {

static const int kThumbnailWidth = 212;
static const int kThumbnailHeight = 132;

// Indicates the time that the RWH must be visible for us to update the
// thumbnail on it. If the user holds down control enter, there will be a lot
// of backing stores created and destroyed. WE don't want to interfere with
// that.
//
// Any operation that happens within this time of being shown is ignored.
// This means we won't throw the thumbnail away when the backing store is
// painted in this time.
static const int kVisibilitySlopMS = 3000;

static const char kThumbnailHistogramName[] = "Thumbnail.ComputeMS";

struct WidgetThumbnail {
  SkBitmap thumbnail;

  // Indicates the last time the RenderWidgetHost was shown and hidden.
  base::TimeTicks last_shown;
  base::TimeTicks last_hidden;
};

PropertyAccessor<WidgetThumbnail>* GetThumbnailAccessor() {
  static PropertyAccessor<WidgetThumbnail> accessor;
  return &accessor;
}

// Returns the existing WidgetThumbnail for a RVH, or creates a new one and
// returns that if none exists.
WidgetThumbnail* GetDataForHost(RenderWidgetHost* host) {
  WidgetThumbnail* wt = GetThumbnailAccessor()->GetProperty(
      host->property_bag());
  if (wt)
    return wt;

  GetThumbnailAccessor()->SetProperty(host->property_bag(),
                                      WidgetThumbnail());
  return GetThumbnailAccessor()->GetProperty(host->property_bag());
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

ThumbnailGenerator::ThumbnailGenerator()
    : no_timeout_(false) {
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
    registrar_.Add(this, NotificationType::RENDER_WIDGET_HOST_DESTROYED,
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
        NotificationType::RENDER_WIDGET_HOST_WILL_DESTROY_BACKING_STORE,
        renderer_source);
  if (monitor != currently_monitored) {
    if (monitor) {
      registrar_.Add(
          this,
          NotificationType::RENDER_WIDGET_HOST_WILL_DESTROY_BACKING_STORE,
          renderer_source);
      registrar_.Add(
          this,
          NotificationType::RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
          renderer_source);
      registrar_.Add(
          this,
          NotificationType::RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,
          renderer_source);
    } else {
      registrar_.Remove(
          this,
          NotificationType::RENDER_WIDGET_HOST_WILL_DESTROY_BACKING_STORE,
          renderer_source);
      registrar_.Remove(
          this,
          NotificationType::RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
          renderer_source);
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
  WidgetThumbnail* wt = GetDataForHost(renderer);

  BackingStore* backing_store = renderer->GetBackingStore(false);
  if (!backing_store) {
    // When we have no backing store, there's no choice in what to use. We
    // have to return either the existing thumbnail or the empty one if there
    // isn't a saved one.
    return wt->thumbnail;
  }

  // Now that we have a backing store, we have a choice to use it to make
  // a new thumbnail, or use a previously stashed one if we have it.
  //
  // Return the previously-computed one if we have it and it hasn't expired.
  if (!wt->thumbnail.isNull() &&
      (no_timeout_ ||
       base::TimeTicks::Now() -
       base::TimeDelta::FromMilliseconds(kVisibilitySlopMS) < wt->last_shown))
    return wt->thumbnail;

  // Save this thumbnail in case we need to use it again soon. It will be
  // invalidated on the next paint.
  wt->thumbnail = GetBitmapForBackingStore(backing_store,
                                           kThumbnailWidth,
                                           kThumbnailHeight,
                                           options,
                                           clip_result);
  return wt->thumbnail;
}

void ThumbnailGenerator::WidgetWillDestroyBackingStore(
    RenderWidgetHost* widget,
    BackingStore* backing_store) {
  // Since the backing store is going away, we need to save it as a thumbnail.
  WidgetThumbnail* wt = GetDataForHost(widget);

  // If there is already a thumbnail on the RWH that's visible, it means that
  // not enough time has elapsed since being shown, and we can ignore generating
  // a new one.
  if (!wt->thumbnail.isNull())
    return;

  // Save a scaled-down image of the page in case we're asked for the thumbnail
  // when there is no RenderViewHost. If this fails, we don't want to overwrite
  // an existing thumbnail.
  SkBitmap new_thumbnail = GetBitmapForBackingStore(backing_store,
                                                    kThumbnailWidth,
                                                    kThumbnailHeight,
                                                    kNoOptions,
                                                    NULL);
  if (!new_thumbnail.isNull())
    wt->thumbnail = new_thumbnail;
}

void ThumbnailGenerator::WidgetDidUpdateBackingStore(RenderWidgetHost* widget) {
  // Notify interested parties that they might want to update their
  // snapshots.
  NotificationService::current()->Notify(
      NotificationType::THUMBNAIL_GENERATOR_SNAPSHOT_CHANGED,
      Source<ThumbnailGenerator>(this),
      Details<RenderWidgetHost>(widget));

  // Clear the current thumbnail since it's no longer valid.
  WidgetThumbnail* wt = GetThumbnailAccessor()->GetProperty(
      widget->property_bag());
  if (!wt)
    return;  // Nothing to do.

  // If this operation is within the time slop after being shown, keep the
  // existing thumbnail.
  if (no_timeout_ ||
      base::TimeTicks::Now() -
      base::TimeDelta::FromMilliseconds(kVisibilitySlopMS) < wt->last_shown)
    return;  // TODO(brettw) schedule thumbnail generation for this renderer in
             // case we don't get a paint for it after the time slop, but it's
             // still visible.

  // Clear the thumbnail, since it's now out of date.
  wt->thumbnail = SkBitmap();
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
      MonitorRenderer(renderer, true);
      break;
    }

    case NotificationType::RENDER_WIDGET_VISIBILITY_CHANGED:
      if (*Details<bool>(details).ptr())
        WidgetShown(Source<RenderWidgetHost>(source).ptr());
      else
        WidgetHidden(Source<RenderWidgetHost>(source).ptr());
      break;

    case NotificationType::RENDER_WIDGET_HOST_DESTROYED:
      WidgetDestroyed(Source<RenderWidgetHost>(source).ptr());
      break;

    case NotificationType::RENDER_WIDGET_HOST_WILL_DESTROY_BACKING_STORE:
      WidgetWillDestroyBackingStore(
          Source<RenderWidgetHost>(source).ptr(),
          Details<BackingStore>(details).ptr());
      break;

    case NotificationType::RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE:
      WidgetDidUpdateBackingStore(Source<RenderWidgetHost>(source).ptr());
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
      NOTREACHED();
  }
}

void ThumbnailGenerator::WidgetShown(RenderWidgetHost* widget) {
  WidgetThumbnail* wt = GetDataForHost(widget);
  wt->last_shown = base::TimeTicks::Now();

  // If there is no thumbnail (like we're displaying a background tab for the
  // first time), then we don't have do to invalidate the existing one.
  if (wt->thumbnail.isNull())
    return;

  std::vector<RenderWidgetHost*>::iterator found =
      std::find(shown_hosts_.begin(), shown_hosts_.end(), widget);
  if (found != shown_hosts_.end()) {
    NOTREACHED() << "Showing a RWH we already think is shown";
    shown_hosts_.erase(found);
  }
  shown_hosts_.push_back(widget);

  // Keep the old thumbnail for a small amount of time after the tab has been
  // shown. This is so in case it's hidden quickly again, we don't waste any
  // work regenerating it.
  if (timer_.IsRunning())
    return;
  timer_.Start(base::TimeDelta::FromMilliseconds(
                   no_timeout_ ? 0 : kVisibilitySlopMS),
               this, &ThumbnailGenerator::ShownDelayHandler);
}

void ThumbnailGenerator::WidgetHidden(RenderWidgetHost* widget) {
  WidgetThumbnail* wt = GetDataForHost(widget);
  wt->last_hidden = base::TimeTicks::Now();

  // If the tab is on the list of ones to invalidate the thumbnail, we need to
  // remove it.
  EraseHostFromShownList(widget);

  // There may still be a valid cached thumbnail on the RWH, so we don't need to
  // make a new one.
  if (!wt->thumbnail.isNull())
    return;
  wt->thumbnail = GetThumbnailForRenderer(widget);
}

void ThumbnailGenerator::WidgetDestroyed(RenderWidgetHost* widget) {
  EraseHostFromShownList(widget);
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

void ThumbnailGenerator::ShownDelayHandler() {
  base::TimeTicks threshold = base::TimeTicks::Now() -
      base::TimeDelta::FromMilliseconds(kVisibilitySlopMS);

  // Check the list of all pending RWHs (normally only one) to see if any of
  // their times have expired.
  for (size_t i = 0; i < shown_hosts_.size(); i++) {
    WidgetThumbnail* wt = GetDataForHost(shown_hosts_[i]);
    if (no_timeout_ || wt->last_shown <= threshold) {
      // This thumbnail has expired, delete it.
      wt->thumbnail = SkBitmap();
      shown_hosts_.erase(shown_hosts_.begin() + i);
      i--;
    }
  }

  // We need to schedule another run if there are still items in the list to
  // process. We use half the timeout for these re-runs to catch the items
  // that were added since the timer was run the first time.
  if (!shown_hosts_.empty()) {
    DCHECK(!no_timeout_);
    timer_.Start(base::TimeDelta::FromMilliseconds(kVisibilitySlopMS) / 2, this,
                 &ThumbnailGenerator::ShownDelayHandler);
  }
}

void ThumbnailGenerator::EraseHostFromShownList(RenderWidgetHost* widget) {
  std::vector<RenderWidgetHost*>::iterator found =
      std::find(shown_hosts_.begin(), shown_hosts_.end(), widget);
  if (found != shown_hosts_.end())
    shown_hosts_.erase(found);
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
    TabContents* tab_contents, const GURL& url) {
  history::TopSites* top_sites = tab_contents->profile()->GetTopSites();
  // Skip if we don't need to update the thumbnail.
  if (!ShouldUpdateThumbnail(tab_contents->profile(), top_sites, url))
    return;

  ThumbnailGenerator* generator = g_browser_process->GetThumbnailGenerator();
  const int options = ThumbnailGenerator::kClippedThumbnail;
  ThumbnailGenerator::ClipResult clip_result = ThumbnailGenerator::kNotClipped;
  SkBitmap thumbnail = generator->GetThumbnailForRendererWithOptions(
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
  const bool is_known = top_sites->IsKnownURL(url);
  if (top_sites->IsFull() && !is_known)
    return false;
  // Skip if we don't have to udpate the existing thumbnail.
  ThumbnailScore current_score;
  if (is_known &&
      top_sites->GetPageThumbnailScore(url, &current_score) &&
      !current_score.ShouldConsiderUpdating())
    return false;

  return true;
}
