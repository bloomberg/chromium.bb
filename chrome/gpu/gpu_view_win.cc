// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_view_win.h"

#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_backing_store.h"
#include "chrome/gpu/gpu_thread.h"

namespace {

void DrawBackground(const RECT& dirty_rect, CPaintDC* dc) {
  HBRUSH white_brush = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
  dc->FillRect(&dirty_rect, white_brush);
}

void DrawResizeCorner(const RECT& dirty_rect, HDC dc) {
  // TODO(brettw): implement this.
}

}  // namespace

GpuViewWin::GpuViewWin(GpuThread* gpu_thread,
                       gfx::NativeViewId parent_window,
                       int32 routing_id)
    : gpu_thread_(gpu_thread),
      routing_id_(routing_id),
      parent_window_(gfx::NativeViewFromId(parent_window)) {
  gpu_thread_->AddRoute(routing_id_, this);
  Create(gfx::NativeViewFromId(parent_window));
  SetWindowText(L"GPU window");
  ShowWindow(SW_SHOW);
}

GpuViewWin::~GpuViewWin() {
  gpu_thread_->RemoveRoute(routing_id_);
  // TODO(brettw) may want to delete any dangling backing stores, or perhaps
  // assert if one still exists.
}

void GpuViewWin::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(GpuViewWin, msg)
    IPC_MESSAGE_HANDLER(GpuMsg_NewBackingStore, OnNewBackingStore)
  IPC_END_MESSAGE_MAP_EX()
}

void GpuViewWin::OnChannelConnected(int32 peer_pid) {
}

void GpuViewWin::OnChannelError() {
  // TODO(brettw) do we need to delete ourselves now?
}

void GpuViewWin::DidScrollBackingStoreRect(int dx, int dy,
                                           const gfx::Rect& rect) {
  // We need to pass in SW_INVALIDATE to ScrollWindowEx.  The documentation on
  // MSDN states that it only applies to the HRGN argument, which is wrong.
  // Not passing in this flag does not invalidate the region which was scrolled
  // from, thus causing painting issues.
  RECT clip_rect = rect.ToRECT();
  ScrollWindowEx(dx, dy, NULL, &clip_rect, NULL, NULL, SW_INVALIDATE);
}

void GpuViewWin::OnNewBackingStore(int32 routing_id, const gfx::Size& size) {
  backing_store_.reset(
      new GpuBackingStore(this, gpu_thread_, routing_id, size));
  MoveWindow(0, 0, size.width(), size.height(), TRUE);
}

void GpuViewWin::OnPaint(HDC unused_dc) {
  // Grab the region to paint before creation of paint_dc since it clears the
  // damage region.
  ScopedGDIObject<HRGN> damage_region(CreateRectRgn(0, 0, 0, 0));
  GetUpdateRgn(damage_region, FALSE);

  CPaintDC paint_dc(m_hWnd);

  gfx::Rect damaged_rect(paint_dc.m_ps.rcPaint);
  if (damaged_rect.IsEmpty())
    return;

  if (backing_store_.get()) {
    gfx::Rect bitmap_rect(gfx::Point(), backing_store_->size());

    // Blit only the damaged regions from the backing store.
    DWORD data_size = GetRegionData(damage_region, 0, NULL);
    // TODO(brettw) why is the "+1" necessary here? When I remove it, the
    // page paints black, but according to the documentation, its not needed.
    scoped_array<char> region_data_buf(new char[data_size + 1]);
    RGNDATA* region_data = reinterpret_cast<RGNDATA*>(region_data_buf.get());
    GetRegionData(damage_region, data_size, region_data);

    RECT* region_rects = reinterpret_cast<RECT*>(region_data->Buffer);
    for (DWORD i = 0; i < region_data->rdh.nCount; ++i) {
      gfx::Rect paint_rect = bitmap_rect.Intersect(gfx::Rect(region_rects[i]));
      if (!paint_rect.IsEmpty()) {
        DrawResizeCorner(paint_rect.ToRECT(), backing_store_->hdc());
        BitBlt(paint_dc.m_hDC,
               paint_rect.x(),
               paint_rect.y(),
               paint_rect.width(),
               paint_rect.height(),
               backing_store_->hdc(),
               paint_rect.x(),
               paint_rect.y(),
               SRCCOPY);
      }
    }

    // Fill the remaining portion of the damaged_rect with the background
    if (damaged_rect.right() > bitmap_rect.right()) {
      RECT r;
      r.left = std::max(bitmap_rect.right(), damaged_rect.x());
      r.right = damaged_rect.right();
      r.top = damaged_rect.y();
      r.bottom = std::min(bitmap_rect.bottom(), damaged_rect.bottom());
      DrawBackground(r, &paint_dc);
    }
    if (damaged_rect.bottom() > bitmap_rect.bottom()) {
      RECT r;
      r.left = damaged_rect.x();
      r.right = damaged_rect.right();
      r.top = std::max(bitmap_rect.bottom(), damaged_rect.y());
      r.bottom = damaged_rect.bottom();
      DrawBackground(r, &paint_dc);
    }
  } else {
    DrawBackground(paint_dc.m_ps.rcPaint, &paint_dc);
  }
}

LRESULT GpuViewWin::OnMouseEvent(UINT message,
                                 WPARAM wparam,
                                 LPARAM lparam,
                                 BOOL& handled) {
  handled = true;
  ::PostMessage(GetParent(), message, wparam, lparam);
  return 0;
}

LRESULT GpuViewWin::OnKeyEvent(UINT message,
                               WPARAM wparam,
                               LPARAM lparam,
                               BOOL& handled) {
  handled = true;
  ::PostMessage(GetParent(), message, wparam, lparam);
  return 0;
}

LRESULT GpuViewWin::OnWheelEvent(UINT message,
                                 WPARAM wparam,
                                 LPARAM lparam,
                                 BOOL& handled) {
  handled = true;
  ::PostMessage(GetParent(), message, wparam, lparam);
  return 0;
}
