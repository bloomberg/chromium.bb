// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/panels/taskbar_window_thumbnailer_win.h"

#include <dwmapi.h>

#include "base/logging.h"
#include "base/win/scoped_hdc.h"
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

void TaskbarWindowThumbnailerWin::Start(
    const std::vector<HWND>& snapshot_hwnds) {
  snapshot_hwnds_ = snapshot_hwnds;
  if (snapshot_hwnds_.empty())
    snapshot_hwnds_.push_back(hwnd_);
  capture_bitmap_.reset(CaptureWindowImage());
  if (capture_bitmap_)
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
  int enclosing_x = 0;
  int enclosing_y = 0;
  int enclosing_right = 0;
  int enclosing_bottom = 0;
  for (std::vector<HWND>::const_iterator iter = snapshot_hwnds_.begin();
       iter != snapshot_hwnds_.end(); ++iter) {
    RECT bounds;
    if (!::GetWindowRect(*iter, &bounds))
      continue;
    if (iter == snapshot_hwnds_.begin()) {
      enclosing_x = bounds.left;
      enclosing_y = bounds.top;
      enclosing_right = bounds.right;
      enclosing_bottom = bounds.bottom;
    } else {
      if (bounds.left < enclosing_x)
        enclosing_x = bounds.left;
      if (bounds.top < enclosing_y)
        enclosing_y = bounds.top;
      if (bounds.right > enclosing_right)
        enclosing_right = bounds.right;
      if (bounds.bottom > enclosing_bottom)
        enclosing_bottom = bounds.bottom;
    }
  }

  int width = enclosing_right - enclosing_x;
  int height = enclosing_bottom - enclosing_y;
  if (!width || !height)
    return NULL;

  gfx::Canvas canvas(gfx::Size(width, height), ui::SCALE_FACTOR_100P, false);
  {
    skia::ScopedPlatformPaint scoped_platform_paint(canvas.sk_canvas());
    HDC target_dc = scoped_platform_paint.GetPlatformSurface();
    for (std::vector<HWND>::const_iterator iter = snapshot_hwnds_.begin();
         iter != snapshot_hwnds_.end(); ++iter) {
      HWND current_hwnd = *iter;
      RECT current_bounds;
      if (!::GetWindowRect(current_hwnd, &current_bounds))
        continue;
      base::win::ScopedGetDC source_dc(current_hwnd);
      ::BitBlt(target_dc,
               current_bounds.left - enclosing_x,
               current_bounds.top - enclosing_y,
               current_bounds.right - current_bounds.left,
               current_bounds.bottom - current_bounds.top,
               source_dc,
               0,
               0,
               SRCCOPY);
      ::ReleaseDC(current_hwnd, source_dc);
    }
  }
  return new SkBitmap(canvas.ExtractImageRep().sk_bitmap());
}
