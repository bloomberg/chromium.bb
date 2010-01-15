// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_PROXY_H_
#define CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_PROXY_H_

#include "base/basictypes.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "ipc/ipc_channel.h"

class GpuProcessHost;

class BackingStoreProxy : public BackingStore,
                          public IPC::Channel::Listener {
 public:
  BackingStoreProxy(RenderWidgetHost* widget, const gfx::Size& size,
                    GpuProcessHost* process, int32 routing_id);
  virtual ~BackingStoreProxy();

  // BackingStore implementation.
  virtual void PaintToBackingStore(RenderProcessHost* process,
                                   TransportDIB::Id bitmap,
                                   const gfx::Rect& bitmap_rect,
                                   const std::vector<gfx::Rect>& copy_rects,
                                   bool* painted_synchronously);
  virtual bool CopyFromBackingStore(const gfx::Rect& rect,
                                    skia::PlatformCanvas* output);
  virtual void ScrollBackingStore(int dx, int dy,
                                  const gfx::Rect& clip_rect,
                                  const gfx::Size& view_size);

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

 private:
  // Message handlers.
  void OnPaintToBackingStoreACK();

  GpuProcessHost* process_;
  int32 routing_id_;

  // Set to true when we're waiting for the GPU process to do a paint and send
  // back a "done" message. In this case, the renderer will be waiting for our
  // message that we're done using the backing store.
  bool waiting_for_paint_ack_;

  DISALLOW_COPY_AND_ASSIGN(BackingStoreProxy);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_BACKING_STORE_PROXY_H_
