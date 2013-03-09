// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/backing_store_win.h"

#include "base/command_line.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/common/content_switches.h"
#include "skia/ext/platform_canvas.h"
#include "ui/base/win/dpi.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/surface/transport_dib.h"

namespace content {
namespace {

// Creates a dib conforming to the height/width/section parameters passed in.
HANDLE CreateDIB(HDC dc, int width, int height, int color_depth) {
  BITMAPV5HEADER hdr = {0};
  ZeroMemory(&hdr, sizeof(BITMAPV5HEADER));

  // These values are shared with gfx::PlatformDevice
  hdr.bV5Size = sizeof(BITMAPINFOHEADER);
  hdr.bV5Width = width;
  hdr.bV5Height = -height;  // minus means top-down bitmap
  hdr.bV5Planes = 1;
  hdr.bV5BitCount = color_depth;
  hdr.bV5Compression = BI_RGB;  // no compression
  hdr.bV5SizeImage = 0;
  hdr.bV5XPelsPerMeter = 1;
  hdr.bV5YPelsPerMeter = 1;
  hdr.bV5ClrUsed = 0;
  hdr.bV5ClrImportant = 0;

  if (BackingStoreWin::ColorManagementEnabled()) {
    hdr.bV5CSType = LCS_sRGB;
    hdr.bV5Intent = LCS_GM_IMAGES;
  }

  void* data = NULL;
  HANDLE dib = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&hdr),
                                0, &data, NULL, 0);
  DCHECK(data);
  return dib;
}

}  // namespace

BackingStoreWin::BackingStoreWin(RenderWidgetHost* widget,
                                 const gfx::Size& size)
    : BackingStore(widget, size),
      backing_store_dib_(NULL),
      original_bitmap_(NULL) {
  HDC screen_dc = ::GetDC(NULL);
  color_depth_ = ::GetDeviceCaps(screen_dc, BITSPIXEL);
  // Color depths less than 16 bpp require a palette to be specified. Instead,
  // we specify the desired color depth as 16 which lets the OS to come up
  // with an approximation.
  if (color_depth_ < 16)
    color_depth_ = 16;
  hdc_ = CreateCompatibleDC(screen_dc);
  ReleaseDC(NULL, screen_dc);
}

BackingStoreWin::~BackingStoreWin() {
  DCHECK(hdc_);
  if (original_bitmap_) {
    SelectObject(hdc_, original_bitmap_);
  }
  if (backing_store_dib_) {
    DeleteObject(backing_store_dib_);
    backing_store_dib_ = NULL;
  }
  DeleteDC(hdc_);
}

// static
bool BackingStoreWin::ColorManagementEnabled() {
  static bool enabled = false;
  static bool checked = false;
  if (!checked) {
    checked = true;
    const CommandLine& command = *CommandLine::ForCurrentProcess();
    enabled = command.HasSwitch(switches::kEnableMonitorProfile);
  }
  return enabled;
}

size_t BackingStoreWin::MemorySize() {
  gfx::Size size_in_pixels = gfx::ToCeiledSize(gfx::ScaleSize(size(),
      ui::win::GetDeviceScaleFactor()));
  return size_in_pixels.GetArea() * (color_depth_ / 8);
}

void BackingStoreWin::PaintToBackingStore(
    RenderProcessHost* process,
    TransportDIB::Id bitmap,
    const gfx::Rect& bitmap_rect,
    const std::vector<gfx::Rect>& copy_rects,
    float scale_factor,
    const base::Closure& completion_callback,
    bool* scheduled_completion_callback) {
  *scheduled_completion_callback = false;
  gfx::Size size_in_pixels = gfx::ToCeiledSize(gfx::ScaleSize(
    size(), scale_factor));
  if (!backing_store_dib_) {
    backing_store_dib_ = CreateDIB(hdc_,
                                   size_in_pixels.width(),
                                   size_in_pixels.height(),
                                   color_depth_);
    if (!backing_store_dib_) {
      NOTREACHED();
      return;
    }
    original_bitmap_ = SelectObject(hdc_, backing_store_dib_);
  }

  TransportDIB* dib = process->GetTransportDIB(bitmap);
  if (!dib)
    return;

  gfx::Rect pixel_bitmap_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(bitmap_rect, scale_factor));

  BITMAPINFO bitmap_info;
  memset(&bitmap_info, 0, sizeof(bitmap_info));
  gfx::CreateBitmapHeader(pixel_bitmap_rect.width(),
                          pixel_bitmap_rect.height(),
                          &bitmap_info.bmiHeader);
  // Account for a bitmap_rect that exceeds the bounds of our view.
  gfx::Rect view_rect(size());

  for (size_t i = 0; i < copy_rects.size(); i++) {
    gfx::Rect paint_rect = gfx::IntersectRects(view_rect, copy_rects[i]);
    gfx::Rect pixel_copy_rect = gfx::ToEnclosingRect(
        gfx::ScaleRect(paint_rect, scale_factor));
    gfx::Rect target_rect = pixel_copy_rect;
    gfx::StretchDIBits(hdc_,
                       target_rect.x(),
                       target_rect.y(),
                       target_rect.width(),
                       target_rect.height(),
                       pixel_copy_rect.x() - pixel_bitmap_rect.x(),
                       pixel_copy_rect.y() - pixel_bitmap_rect.y(),
                       pixel_copy_rect.width(),
                       pixel_copy_rect.height(),
                       dib->memory(),
                       &bitmap_info);
  }
}

bool BackingStoreWin::CopyFromBackingStore(const gfx::Rect& rect,
                                           skia::PlatformBitmap* output) {
  // TODO(kevers): Make sure this works with HiDPI backing stores.
  if (!output->Allocate(rect.width(), rect.height(), true))
    return false;

  HDC temp_dc = output->GetSurface();
  BitBlt(temp_dc, 0, 0, rect.width(), rect.height(),
         hdc(), rect.x(), rect.y(), SRCCOPY);
  return true;
}

void BackingStoreWin::ScrollBackingStore(const gfx::Vector2d& delta,
                                         const gfx::Rect& clip_rect,
                                         const gfx::Size& view_size) {
  // TODO(darin): this doesn't work if delta x() and y() are both non-zero!
  DCHECK(delta.x() == 0 || delta.y() == 0);

  float scale = ui::win::GetDeviceScaleFactor();
  gfx::Rect screen_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(clip_rect, scale));
  int dx = static_cast<int>(delta.x() * scale);
  int dy = static_cast<int>(delta.y() * scale);
  RECT damaged_rect, r = screen_rect.ToRECT();
  ScrollDC(hdc_, dx, dy, NULL, &r, NULL, &damaged_rect);
}

}  // namespace content
