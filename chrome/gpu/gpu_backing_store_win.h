// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_BACKING_STORE_WIN_H_
#define CHROME_GPU_GPU_BACKING_STORE_WIN_H_

#include <windows.h>

#include <vector>

#include "app/gfx/native_widget_types.h"
#include "base/basictypes.h"
#include "base/gfx/size.h"
#include "chrome/common/transport_dib.h"
#include "ipc/ipc_channel.h"

class GpuThread;
class GpuViewWin;

namespace gfx {
class Rect;
class Size;
}

class GpuBackingStoreWin : public IPC::Channel::Listener {
 public:
  GpuBackingStoreWin(GpuViewWin* view,
                     GpuThread* gpu_thread,
                     int32 routing_id,
                     const gfx::Size& size);
  ~GpuBackingStoreWin();

  gfx::Size size() const { return size_; }
  HDC hdc() const { return hdc_; }

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

  GpuViewWin* view_;

  GpuThread* gpu_thread_;
  int32 routing_id_;

  gfx::Size size_;

  // The backing store dc.
  HDC hdc_;

  // Handle to the backing store dib.
  HANDLE backing_store_dib_;

  // Handle to the original bitmap in the dc.
  HANDLE original_bitmap_;

  // Number of bits per pixel of the screen.
  int color_depth_;

  DISALLOW_COPY_AND_ASSIGN(GpuBackingStoreWin);
};

#endif  // CHROME_GPU_GPU_BACKING_STORE_WIN_H_
