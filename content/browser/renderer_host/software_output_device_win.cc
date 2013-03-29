// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/software_output_device_win.h"

#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/gdi_util.h"

namespace content {

SoftwareOutputDeviceWin::SoftwareOutputDeviceWin(ui::Compositor* compositor)
    : compositor_(compositor) {
  // TODO(skaslev) Remove this when crbug.com/180702 is fixed.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  hwnd_ = compositor->widget();
}

SoftwareOutputDeviceWin::~SoftwareOutputDeviceWin() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void SoftwareOutputDeviceWin::Resize(gfx::Size viewport_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  cc::SoftwareOutputDevice::Resize(viewport_size);
  memset(&bitmap_info_, 0, sizeof(bitmap_info_));
  gfx::CreateBitmapHeader(viewport_size_.width(), viewport_size_.height(),
                          &bitmap_info_.bmiHeader);
}

void SoftwareOutputDeviceWin::EndPaint(cc::SoftwareFrameData* frame_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(device_);
  DCHECK(frame_data == NULL);

  if (!device_)
    return;

  gfx::Rect rect = damage_rect_;
  rect.Intersect(gfx::Rect(viewport_size_));
  if (rect.IsEmpty())
    return;

  const SkBitmap& bitmap = device_->accessBitmap(false);
  HDC hdc = ::GetDC(hwnd_);
  gfx::StretchDIBits(hdc,
                     rect.x(), rect.y(),
                     rect.width(), rect.height(),
                     rect.x(), rect.y(),
                     rect.width(), rect.height(),
                     bitmap.getPixels(),
                     &bitmap_info_);
  ::ReleaseDC(hwnd_, hdc);
}

}  // namespace content
