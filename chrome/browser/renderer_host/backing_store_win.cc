// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store_win.h"

#include "app/surface/transport_dib.h"
#include "base/command_line.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/common/chrome_switches.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/gdi_util.h"

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

void CallStretchDIBits(HDC hdc, int dest_x, int dest_y, int dest_w, int dest_h,
                       int src_x, int src_y, int src_w, int src_h, void* pixels,
                       const BITMAPINFO* bitmap_info) {
  // When blitting a rectangle that touches the bottom, left corner of the
  // bitmap, StretchDIBits looks at it top-down!  For more details, see
  // http://wiki.allegro.cc/index.php?title=StretchDIBits.
  int rv;
  int bitmap_h = -bitmap_info->bmiHeader.biHeight;
  int bottom_up_src_y = bitmap_h - src_y - src_h;
  if (bottom_up_src_y == 0 && src_x == 0 && src_h != bitmap_h) {
    rv = StretchDIBits(hdc,
                       dest_x, dest_h + dest_y - 1, dest_w, -dest_h,
                       src_x, bitmap_h - src_y + 1, src_w, -src_h,
                       pixels, bitmap_info, DIB_RGB_COLORS, SRCCOPY);
  } else {
    rv = StretchDIBits(hdc,
                       dest_x, dest_y, dest_w, dest_h,
                       src_x, bottom_up_src_y, src_w, src_h,
                       pixels, bitmap_info, DIB_RGB_COLORS, SRCCOPY);
  }
  DCHECK(rv != GDI_ERROR);
}

}  // namespace

BackingStoreWin::BackingStoreWin(RenderWidgetHost* widget, const gfx::Size& size)
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
  return size().GetArea() * (color_depth_ / 8);
}

void BackingStoreWin::PaintToBackingStore(
    RenderProcessHost* process,
    TransportDIB::Id bitmap,
    const gfx::Rect& bitmap_rect,
    const std::vector<gfx::Rect>& copy_rects) {
  if (!backing_store_dib_) {
    backing_store_dib_ = CreateDIB(hdc_, size().width(),
                                   size().height(), color_depth_);
    if (!backing_store_dib_) {
      NOTREACHED();
      return;
    }
    original_bitmap_ = SelectObject(hdc_, backing_store_dib_);
  }

  TransportDIB* dib = process->GetTransportDIB(bitmap);
  if (!dib)
    return;

  BITMAPINFOHEADER hdr;
  gfx::CreateBitmapHeader(bitmap_rect.width(), bitmap_rect.height(), &hdr);
  // Account for a bitmap_rect that exceeds the bounds of our view
  gfx::Rect view_rect(size());

  for (size_t i = 0; i < copy_rects.size(); i++) {
    gfx::Rect paint_rect = view_rect.Intersect(copy_rects[i]);
    CallStretchDIBits(hdc_,
                      paint_rect.x(),
                      paint_rect.y(),
                      paint_rect.width(),
                      paint_rect.height(),
                      paint_rect.x() - bitmap_rect.x(),
                      paint_rect.y() - bitmap_rect.y(),
                      paint_rect.width(),
                      paint_rect.height(),
                      dib->memory(),
                      reinterpret_cast<BITMAPINFO*>(&hdr));
  }
}

bool BackingStoreWin::CopyFromBackingStore(const gfx::Rect& rect,
                                           skia::PlatformCanvas* output) {
  if (!output->initialize(rect.width(), rect.height(), true))
    return false;

  HDC temp_dc = output->beginPlatformPaint();
  BitBlt(temp_dc, 0, 0, rect.width(), rect.height(),
         hdc(), rect.x(), rect.y(), SRCCOPY);
  output->endPlatformPaint();
  return true;
}

void BackingStoreWin::ScrollBackingStore(int dx, int dy,
                                         const gfx::Rect& clip_rect,
                                         const gfx::Size& view_size) {
  RECT damaged_rect, r = clip_rect.ToRECT();
  ScrollDC(hdc_, dx, dy, NULL, &r, NULL, &damaged_rect);

  // TODO(darin): this doesn't work if dx and dy are both non-zero!
  DCHECK(dx == 0 || dy == 0);
}
