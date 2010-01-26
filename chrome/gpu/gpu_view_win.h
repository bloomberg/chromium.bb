// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_VIEW_WIN_H_
#define CHROME_GPU_GPU_VIEW_WIN_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>

#include "app/gfx/native_widget_types.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "ipc/ipc_channel.h"

class GpuBackingStoreWin;
class GpuThread;

namespace gfx {
class Rect;
class Size;
}

// WS_DISABLED means that input events will be delivered to the parent, which is
// what we want for our overlay window.
typedef CWinTraits<
    WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_DISABLED, 0>
    GpuRenderWidgetHostViewWinTraits;

class GpuViewWin
    : public IPC::Channel::Listener,
      public CWindowImpl<GpuViewWin,
                         CWindow,
                         GpuRenderWidgetHostViewWinTraits> {
 public:
  GpuViewWin(GpuThread* gpu_thread,
             HWND parent,
             int32 routing_id);
  ~GpuViewWin();

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  void DidScrollBackingStoreRect(int dx, int dy, const gfx::Rect& rect);

  BEGIN_MSG_MAP(GpuViewWin)
    MSG_WM_PAINT(OnPaint)
  END_MSG_MAP()

 private:
  // IPC message handlers.
  void OnNewBackingStore(int32 routing_id, const gfx::Size& size);

  // Windows message handlers.
  void OnPaint(HDC unused_dc);

  GpuThread* gpu_thread_;
  int32 routing_id_;

  HWND parent_;

  scoped_ptr<GpuBackingStoreWin> backing_store_;

  DISALLOW_COPY_AND_ASSIGN(GpuViewWin);
};

#endif  // CHROME_GPU_GPU_VIEW_WIN_H_
