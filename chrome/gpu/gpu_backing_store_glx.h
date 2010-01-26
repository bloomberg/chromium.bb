// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_BACKING_STORE_GLX_H_
#define CHROME_GPU_GPU_BACKING_STORE_GLX_H_

#include "base/basictypes.h"
#include "base/process.h"
#include "chrome/common/transport_dib.h"
#include "chrome/common/x11_util.h"
#include "ipc/ipc_channel.h"

class GpuViewX;
class GpuThread;
class SkBitmap;

class GpuBackingStoreGLX : public IPC::Channel::Listener {
 public:
  GpuBackingStoreGLX(GpuViewX* view,
                     GpuThread* gpu_thread,
                     int32 routing_id,
                     const gfx::Size& size);
  ~GpuBackingStoreGLX();

  const gfx::Size& size() const { return size_; }
  const gfx::Size& texture_size() const { return texture_size_; }
  unsigned int texture_id() const { return texture_id_; }

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

 private:
  // Message handlers.
  void OnPaintToBackingStore(base::ProcessId source_process_id,
                             TransportDIB::Id id,
                             const gfx::Rect& bitmap_rect,
                             const std::vector<gfx::Rect>& copy_rects);
  void OnScrollBackingStore(int dx, int dy,
                            const gfx::Rect& clip_rect,
                            const gfx::Size& view_size);

  void PaintOneRectToBackingStore(const SkBitmap& transport_bitmap,
                                  const gfx::Rect& bitmap_rect,
                                  const gfx::Rect& copy_rect);

  GpuViewX* view_;
  GpuThread* gpu_thread_;
  int32 routing_id_;
  gfx::Size size_;

  unsigned int texture_id_;  // 0 when uninitialized.

  // The size of the texture loaded into GL. This is 0x0 when there is no
  // texture loaded. This may be different than the size of the backing store
  // because we could have been resized without yet getting the updated
  // bitmap.
  gfx::Size texture_size_;

  DISALLOW_COPY_AND_ASSIGN(GpuBackingStoreGLX);
};

#endif  // CHROME_GPU_GPU_BACKING_STORE_GLX_H_
