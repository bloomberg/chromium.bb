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

class GpuBackingStore;
class GpuThread;

namespace gfx {
class Rect;
class Size;
}

typedef CWinTraits<WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0>
    GpuRenderWidgetHostViewWinTraits;

class GpuViewWin
    : public IPC::Channel::Listener,
      public CWindowImpl<GpuViewWin,
                         CWindow,
                         GpuRenderWidgetHostViewWinTraits> {
 public:
  GpuViewWin(GpuThread* gpu_thread,
                             gfx::NativeViewId parent_window,
                             int32 routing_id);
  ~GpuViewWin();

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  void DidScrollBackingStoreRect(int dx, int dy, const gfx::Rect& rect);

  BEGIN_MSG_MAP(GpuViewWin)
    MSG_WM_PAINT(OnPaint)
    MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseEvent)
    MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseEvent)
    MESSAGE_HANDLER(WM_LBUTTONDOWN, OnMouseEvent)
    MESSAGE_HANDLER(WM_MBUTTONDOWN, OnMouseEvent)
    MESSAGE_HANDLER(WM_RBUTTONDOWN, OnMouseEvent)
    MESSAGE_HANDLER(WM_LBUTTONUP, OnMouseEvent)
    MESSAGE_HANDLER(WM_MBUTTONUP, OnMouseEvent)
    MESSAGE_HANDLER(WM_RBUTTONUP, OnMouseEvent)
    MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnMouseEvent)
    MESSAGE_HANDLER(WM_MBUTTONDBLCLK, OnMouseEvent)
    MESSAGE_HANDLER(WM_RBUTTONDBLCLK, OnMouseEvent)
    MESSAGE_HANDLER(WM_SYSKEYDOWN, OnKeyEvent)
    MESSAGE_HANDLER(WM_SYSKEYUP, OnKeyEvent)
    MESSAGE_HANDLER(WM_KEYDOWN, OnKeyEvent)
    MESSAGE_HANDLER(WM_KEYUP, OnKeyEvent)
    MESSAGE_HANDLER(WM_MOUSEWHEEL, OnWheelEvent)
    MESSAGE_HANDLER(WM_MOUSEHWHEEL, OnWheelEvent)
    MESSAGE_HANDLER(WM_HSCROLL, OnWheelEvent)
    MESSAGE_HANDLER(WM_VSCROLL, OnWheelEvent)
    MESSAGE_HANDLER(WM_CHAR, OnKeyEvent)
    MESSAGE_HANDLER(WM_SYSCHAR, OnKeyEvent)
    MESSAGE_HANDLER(WM_IME_CHAR, OnKeyEvent)
  END_MSG_MAP()

 private:
  // IPC message handlers.
  void OnNewBackingStore(int32 routing_id, const gfx::Size& size);

  // Windows message handlers.
  void OnPaint(HDC unused_dc);
  LRESULT OnMouseEvent(UINT message,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);
  LRESULT OnKeyEvent(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     BOOL& handled);
  LRESULT OnWheelEvent(UINT message,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL& handled);

  GpuThread* gpu_thread_;
  int32 routing_id_;

  HWND parent_window_;

  scoped_ptr<GpuBackingStore> backing_store_;

  DISALLOW_COPY_AND_ASSIGN(GpuViewWin);
};

#endif  // CHROME_GPU_GPU_VIEW_WIN_H_
