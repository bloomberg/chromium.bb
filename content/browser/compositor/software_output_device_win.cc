// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/software_output_device_win.h"

#include "base/memory/shared_memory.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/skia_util.h"

namespace content {

OutputDeviceBacking::OutputDeviceBacking() : created_byte_size_(0) {
}

OutputDeviceBacking::~OutputDeviceBacking() {
  DCHECK(devices_.empty());
}

void OutputDeviceBacking::Resized() {
  size_t new_size = GetMaxByteSize();
  if (new_size == created_byte_size_)
    return;
  for (SoftwareOutputDeviceWin* device : devices_) {
    device->ReleaseContents();
  }
  backing_.reset();
  created_byte_size_ = 0;
}

void OutputDeviceBacking::RegisterOutputDevice(
    SoftwareOutputDeviceWin* device) {
  devices_.push_back(device);
}

void OutputDeviceBacking::UnregisterOutputDevice(
    SoftwareOutputDeviceWin* device) {
  auto it = std::find(devices_.begin(), devices_.end(), device);
  DCHECK(it != devices_.end());
  devices_.erase(it);
  Resized();
}

base::SharedMemory* OutputDeviceBacking::GetSharedMemory() {
  if (backing_)
    return backing_.get();
  created_byte_size_ = GetMaxByteSize();

  backing_.reset(new base::SharedMemory);
  CHECK(backing_->CreateAnonymous(created_byte_size_));
  return backing_.get();
}

size_t OutputDeviceBacking::GetMaxByteSize() {
  // Minimum byte size is 1 because creating a 0-byte-long SharedMemory fails.
  size_t max_size = 1;
  for (const SoftwareOutputDeviceWin* device : devices_) {
    max_size = std::max(
        max_size,
        static_cast<size_t>(device->viewport_pixel_size().GetArea() * 4));
  }
  return max_size;
}

SoftwareOutputDeviceWin::SoftwareOutputDeviceWin(OutputDeviceBacking* backing,
                                                 ui::Compositor* compositor)
    : hwnd_(compositor->widget()),
      is_hwnd_composited_(false),
      backing_(backing),
      in_paint_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  LONG style = GetWindowLong(hwnd_, GWL_EXSTYLE);
  is_hwnd_composited_ = !!(style & WS_EX_COMPOSITED);
  // Layered windows must be completely updated every time, so they can't
  // share contents with other windows.
  if (is_hwnd_composited_)
    backing_ = nullptr;
  if (backing_)
    backing_->RegisterOutputDevice(this);
}

SoftwareOutputDeviceWin::~SoftwareOutputDeviceWin() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!in_paint_);
  if (backing_)
    backing_->UnregisterOutputDevice(this);
}

void SoftwareOutputDeviceWin::Resize(const gfx::Size& viewport_pixel_size,
                                     float scale_factor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!in_paint_);

  scale_factor_ = scale_factor;

  if (viewport_pixel_size_ == viewport_pixel_size)
    return;

  viewport_pixel_size_ = viewport_pixel_size;
  if (backing_)
    backing_->Resized();
  contents_.clear();
}

SkCanvas* SoftwareOutputDeviceWin::BeginPaint(const gfx::Rect& damage_rect) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!in_paint_);
  if (!contents_) {
    HANDLE shared_section = NULL;
    if (backing_)
      shared_section = backing_->GetSharedMemory()->handle().GetHandle();
    contents_ = skia::AdoptRef(skia::CreatePlatformCanvas(
        viewport_pixel_size_.width(), viewport_pixel_size_.height(), true,
        shared_section, skia::CRASH_ON_FAILURE));
  }

  damage_rect_ = damage_rect;
  in_paint_ = true;
  return contents_.get();
}

void SoftwareOutputDeviceWin::EndPaint() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(contents_);
  DCHECK(in_paint_);

  in_paint_ = false;
  SoftwareOutputDevice::EndPaint();

  gfx::Rect rect = damage_rect_;
  rect.Intersect(gfx::Rect(viewport_pixel_size_));
  if (rect.IsEmpty())
    return;

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

    HDC dib_dc = skia::BeginPlatformPaint(contents_.get());
    ::UpdateLayeredWindow(hwnd_, NULL, &position, &size, dib_dc, &zero,
                          RGB(0xFF, 0xFF, 0xFF), &blend, ULW_ALPHA);
    skia::EndPlatformPaint(contents_.get());
  } else {
    HDC hdc = ::GetDC(hwnd_);
    RECT src_rect = rect.ToRECT();
    skia::DrawToNativeContext(contents_.get(), hdc, rect.x(), rect.y(),
                              &src_rect);
    ::ReleaseDC(hwnd_, hdc);
  }
}

void SoftwareOutputDeviceWin::ReleaseContents() {
  DCHECK(!contents_ || contents_->unique());
  DCHECK(!in_paint_);
  contents_.clear();
}

}  // namespace content
