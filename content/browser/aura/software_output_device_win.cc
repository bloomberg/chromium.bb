// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aura/software_output_device_win.h"

#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/skia_util.h"

namespace content {

SoftwareOutputDeviceWin::SoftwareOutputDeviceWin(ui::Compositor* compositor)
    : hwnd_(compositor->widget()),
      is_hwnd_composited_(false) {
  // TODO(skaslev) Remove this when crbug.com/180702 is fixed.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  LONG style = GetWindowLong(hwnd_, GWL_EXSTYLE);
  is_hwnd_composited_ = !!(style & WS_EX_COMPOSITED);
}

SoftwareOutputDeviceWin::~SoftwareOutputDeviceWin() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void SoftwareOutputDeviceWin::Resize(gfx::Size viewport_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (viewport_size_ == viewport_size)
    return;

  viewport_size_ = viewport_size;
  contents_.reset(new gfx::Canvas(viewport_size, 1.0f, true));
  memset(&bitmap_info_, 0, sizeof(bitmap_info_));
  gfx::CreateBitmapHeader(viewport_size_.width(), viewport_size_.height(),
                          &bitmap_info_.bmiHeader);
}

SkCanvas* SoftwareOutputDeviceWin::BeginPaint(gfx::Rect damage_rect) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(contents_);

  damage_rect_ = damage_rect;
  return contents_ ? contents_->sk_canvas() : NULL;
}

void SoftwareOutputDeviceWin::EndPaint(cc::SoftwareFrameData* frame_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(contents_);
  DCHECK(frame_data);

  if (!contents_)
    return;

  SoftwareOutputDevice::EndPaint(frame_data);

  gfx::Rect rect = damage_rect_;
  rect.Intersect(gfx::Rect(viewport_size_));
  if (rect.IsEmpty())
    return;

  SkCanvas* canvas = contents_->sk_canvas();
  DCHECK(canvas);
  if (is_hwnd_composited_) {
    RECT wr;
    GetWindowRect(hwnd_, &wr);
    SIZE size = {wr.right - wr.left, wr.bottom - wr.top};
    POINT position = {wr.left, wr.top};
    POINT zero = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0x00, 0xFF, AC_SRC_ALPHA};

    DWORD style = GetWindowLong(hwnd_, GWL_EXSTYLE);
    style &= ~WS_EX_COMPOSITED;
    style |= WS_EX_LAYERED;
    SetWindowLong(hwnd_, GWL_EXSTYLE, style);

    HDC dib_dc = skia::BeginPlatformPaint(canvas);
    ::UpdateLayeredWindow(hwnd_, NULL, &position, &size, dib_dc, &zero,
                          RGB(0xFF, 0xFF, 0xFF), &blend, ULW_ALPHA);
    skia::EndPlatformPaint(canvas);
  } else {
    HDC hdc = ::GetDC(hwnd_);
    RECT src_rect = rect.ToRECT();
    skia::DrawToNativeContext(canvas, hdc, rect.x(), rect.y(), &src_rect);
    ::ReleaseDC(hwnd_, hdc);
  }
}

void SoftwareOutputDeviceWin::CopyToBitmap(
    gfx::Rect rect, SkBitmap* output) {
  DCHECK(contents_);
  SkBaseDevice* device = contents_->sk_canvas()->getDevice();
  const SkBitmap& bitmap = device->accessBitmap(false);
  bitmap.extractSubset(output, gfx::RectToSkIRect(rect));
}

}  // namespace content
