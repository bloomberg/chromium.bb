// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/render_widget_snapshot_taker.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/layout.h"
#include "ui/gfx/size.h"
#include "ui/gfx/size_conversions.h"
#include "ui/surface/transport_dib.h"

using content::RenderWidgetHost;

struct RenderWidgetSnapshotTaker::AsyncRequestInfo {
  SnapshotReadyCallback callback;
  scoped_ptr<TransportDIB> thumbnail_dib;
  RenderWidgetHost* renderer;  // Not owned.
};

RenderWidgetSnapshotTaker::RenderWidgetSnapshotTaker() {
  // The BrowserProcessImpl creates this non-lazily. If you add nontrivial
  // stuff here, be sure to convert it to being lazily created.
  //
  // We don't register for notifications here since BrowserProcessImpl creates
  // us before the NotificationService is.
}

RenderWidgetSnapshotTaker::~RenderWidgetSnapshotTaker() {
}

void RenderWidgetSnapshotTaker::AskForSnapshot(
    RenderWidgetHost* renderer,
    const SnapshotReadyCallback& callback,
    gfx::Size page_size,
    gfx::Size desired_size) {
  // We are going to render the thumbnail asynchronously now, so keep
  // this callback for later lookup when the rendering is done.
  static int sequence_num = 0;
  sequence_num++;
  float scale_factor = ui::GetScaleFactorScale(ui::GetScaleFactorForNativeView(
      renderer->GetView()->GetNativeView()));
  gfx::Size desired_size_in_pixel = gfx::ToFlooredSize(
      gfx::ScaleSize(desired_size, scale_factor));
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
  SnapshotCallbackMap::value_type new_value(sequence_num, request_info);
  std::pair<SnapshotCallbackMap::iterator, bool> result =
      callback_map_.insert(new_value);
  if (!result.second) {
    NOTREACHED() << "Callback already registered?";
    return;
  }

  MonitorRenderer(renderer, true);
  renderer->PaintAtSize(
      renderer_dib_handle, sequence_num, page_size, desired_size);

#endif  // defined(USE_X11)
}

void RenderWidgetSnapshotTaker::CancelSnapshot(
    content::RenderWidgetHost* renderer) {
  SnapshotCallbackMap::iterator iterator = callback_map_.begin();
  while (iterator != callback_map_.end()) {
    if (iterator->second->renderer == renderer) {
      SnapshotCallbackMap::iterator nuked = iterator;
      ++iterator;
      callback_map_.erase(nuked);
      MonitorRenderer(renderer, false);
      continue;
    }
    ++iterator;
  }
}

void RenderWidgetSnapshotTaker::WidgetDidReceivePaintAtSizeAck(
    RenderWidgetHost* widget,
    int sequence_num,
    const gfx::Size& size) {
  // Lookup the callback, run it, and erase it.
  SnapshotCallbackMap::iterator item = callback_map_.find(sequence_num);
  if (item != callback_map_.end()) {
    DCHECK_EQ(widget, item->second->renderer);
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
    if (dib->size() < non_owned_bitmap.getSafeSize())
      return;
    non_owned_bitmap.setPixels(dib->memory());

    // Now alloc/copy the memory so we own it and can pass it around,
    // and the memory won't go away when the DIB goes away.
    // TODO: Figure out a way to avoid this copy?
    non_owned_bitmap.copyTo(&result, SkBitmap::kARGB_8888_Config);

    item->second->callback.Run(result);

    // We're done with the callback, and with the DIB, so delete both.
    callback_map_.erase(item);
    MonitorRenderer(widget, false);
  }
}

void RenderWidgetSnapshotTaker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK: {
      std::pair<int, gfx::Size>* size_ack_details =
          content::Details<std::pair<int, gfx::Size> >(details).ptr();
      WidgetDidReceivePaintAtSizeAck(
          content::Source<RenderWidgetHost>(source).ptr(),
          size_ack_details->first,
          size_ack_details->second);
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification type: " << type;
  }
}

void RenderWidgetSnapshotTaker::MonitorRenderer(RenderWidgetHost* renderer,
                                                bool monitor) {
  content::Source<RenderWidgetHost> renderer_source =
      content::Source<RenderWidgetHost>(renderer);
  if (monitor) {
    int new_count = ++host_monitor_counts_[renderer];
    if (new_count == 1) {
      registrar_.Add(
          this,
          content::NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,
          renderer_source);
    }
  } else {
    int new_count = --host_monitor_counts_[renderer];
    if (new_count == 0) {
      host_monitor_counts_.erase(renderer);
      registrar_.Remove(
          this,
          content::NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,
          renderer_source);
    }
  }
}
