// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/panels/taskbar_window_thumbnailer_win.h"

#include <dwmapi.h>

#include "base/logging.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/gdi_util.h"

namespace {

HBITMAP GetNativeBitmapFromSkBitmap(const SkBitmap& bitmap) {
  int width = bitmap.width();
  int height = bitmap.height();

  BITMAPV4HEADER native_bitmap_header;
  gfx::CreateBitmapV4Header(width, height, &native_bitmap_header);

  HDC dc = ::GetDC(NULL);
  void* bits;
  HBITMAP native_bitmap = ::CreateDIBSection(dc,
      reinterpret_cast<BITMAPINFO*>(&native_bitmap_header),
      DIB_RGB_COLORS,
      &bits,
      NULL,
      0);
  DCHECK(native_bitmap);
  ::ReleaseDC(NULL, dc);
  bitmap.copyPixelsTo(bits, width * height * 4, width * 4);
  return native_bitmap;
}

void EnableCustomThumbnail(HWND hwnd, bool enable) {
  BOOL enable_value = enable;
  ::DwmSetWindowAttribute(hwnd,
                          DWMWA_FORCE_ICONIC_REPRESENTATION,
                          &enable_value,
                          sizeof(enable_value));
  ::DwmSetWindowAttribute(hwnd,
                          DWMWA_HAS_ICONIC_BITMAP,
                          &enable_value,
                          sizeof(enable_value));
}

}  // namespace


TaskbarWindowThumbnailerWin::TaskbarWindowThumbnailerWin(HWND hwnd)
    : hwnd_(hwnd) {
}

TaskbarWindowThumbnailerWin::~TaskbarWindowThumbnailerWin() {
}

void TaskbarWindowThumbnailerWin::Start() {
  capture_bitmap_.reset(CaptureWindowImage());
  EnableCustomThumbnail(hwnd_, true);
}

void TaskbarWindowThumbnailerWin::Stop() {
  capture_bitmap_.reset();
  EnableCustomThumbnail(hwnd_, false);
}

bool TaskbarWindowThumbnailerWin::FilterMessage(HWND hwnd,
                                                UINT message,
                                                WPARAM w_param,
                                                LPARAM l_param,
                                                LRESULT* l_result) {
  DCHECK_EQ(hwnd_, hwnd);
  switch (message) {
    case WM_DWMSENDICONICTHUMBNAIL:
      return OnDwmSendIconicThumbnail(HIWORD(l_param),
                                      LOWORD(l_param),
                                      l_result);
    case WM_DWMSENDICONICLIVEPREVIEWBITMAP:
      return OnDwmSendIconicLivePreviewBitmap(l_result);
  }
  return false;
}

bool TaskbarWindowThumbnailerWin::OnDwmSendIconicThumbnail(
    int width, int height, LRESULT* l_result) {
  DCHECK(capture_bitmap_.get());

  SkBitmap* thumbnail_bitmap = capture_bitmap_.get();

  // Scale the image if needed.
  SkBitmap scaled_bitmap;
  if (capture_bitmap_->width() != width ||
      capture_bitmap_->height() != height) {
    double x_scale = static_cast<double>(width) / capture_bitmap_->width();
    double y_scale = static_cast<double>(height) / capture_bitmap_->height();
    double scale = std::min(x_scale, y_scale);
    width = capture_bitmap_->width() * scale;
    height = capture_bitmap_->height() * scale;
    scaled_bitmap = skia::ImageOperations::Resize(
        *capture_bitmap_, skia::ImageOperations::RESIZE_GOOD, width, height);
    thumbnail_bitmap = &scaled_bitmap;
  }

  HBITMAP native_bitmap = GetNativeBitmapFromSkBitmap(*thumbnail_bitmap);
  ::DwmSetIconicThumbnail(hwnd_, native_bitmap, 0);
  ::DeleteObject(native_bitmap);

  *l_result = 0;
  return true;
}

bool TaskbarWindowThumbnailerWin::OnDwmSendIconicLivePreviewBitmap(
    LRESULT* l_result) {
  scoped_ptr<SkBitmap> live_bitmap(CaptureWindowImage());
  HBITMAP native_bitmap = GetNativeBitmapFromSkBitmap(*live_bitmap);
  ::DwmSetIconicLivePreviewBitmap(hwnd_, native_bitmap, NULL, 0);
  ::DeleteObject(native_bitmap);

  *l_result = 0;
  return true;
}

SkBitmap* TaskbarWindowThumbnailerWin::CaptureWindowImage() const {
  RECT bounds;
  ::GetWindowRect(hwnd_, &bounds);
  int width = bounds.right - bounds.left;
  int height = bounds.bottom - bounds.top;

  gfx::Canvas canvas(gfx::Size(width, height), ui::SCALE_FACTOR_100P, false);
  HDC target_dc = canvas.BeginPlatformPaint();
  HDC source_dc = ::GetDC(hwnd_);
  ::BitBlt(target_dc, 0, 0, width, height, source_dc, 0, 0, SRCCOPY);
  ::ReleaseDC(hwnd_, source_dc);
  canvas.EndPlatformPaint();
  return new SkBitmap(canvas.ExtractImageRep().sk_bitmap());
}
