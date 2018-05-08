// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_win.h"

#include <algorithm>

#include "base/debug/alias.h"
#include "base/memory/shared_memory.h"
#include "base/win/windows_version.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/base/win/internal_constants.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/skia_util.h"

namespace viz {
namespace {

// If a window is larger than this in bytes, don't even try to create a
// backing bitmap for it.
constexpr size_t kMaxBitmapSizeBytes = 4 * (16384 * 8192);

bool NeedsToUseLayerWindow(HWND hwnd) {
  // Layered windows are a legacy way of supporting transparency for HWNDs. With
  // Desktop Window Manager (DWM) HWNDs support transparency natively. DWM is
  // always enabled on Windows 8 and later. However, for Windows 7 (and earlier)
  // DWM might be disabled and layered windows are necessary for HWNDs with
  // transparency.
  return base::win::GetVersion() <= base::win::VERSION_WIN7 &&
         GetProp(hwnd, ui::kWindowTranslucent);
}

}  // namespace

OutputDeviceBacking::OutputDeviceBacking() = default;

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

base::SharedMemory* OutputDeviceBacking::GetSharedMemory(
    const gfx::Size& size) {
  if (backing_)
    return backing_.get();
  size_t expected_byte_size = GetMaxByteSize();
  size_t required_size;
  if (!ResourceSizes::MaybeSizeInBytes(size, RGBA_8888, &required_size))
    return nullptr;
  if (required_size > expected_byte_size)
    return nullptr;

  created_byte_size_ = expected_byte_size;

  backing_ = std::make_unique<base::SharedMemory>();
  base::debug::Alias(&expected_byte_size);
  CHECK(backing_->CreateAnonymous(created_byte_size_));
  return backing_.get();
}

size_t OutputDeviceBacking::GetMaxByteSize() {
  // Minimum byte size is 1 because creating a 0-byte-long SharedMemory fails.
  size_t max_size = 1;
  for (const SoftwareOutputDeviceWin* device : devices_) {
    size_t current_size;
    if (!ResourceSizes::MaybeSizeInBytes(device->viewport_pixel_size(),
                                         RGBA_8888, &current_size))
      continue;
    if (current_size > kMaxBitmapSizeBytes)
      continue;
    max_size = std::max(max_size, current_size);
  }
  return max_size;
}

SoftwareOutputDeviceWin::SoftwareOutputDeviceWin(OutputDeviceBacking* backing,
                                                 HWND hwnd)
    : hwnd_(hwnd),
      use_layered_window_(NeedsToUseLayerWindow(hwnd)),
      backing_(use_layered_window_ ? nullptr : backing) {
  // Layered windows must be completely updated every time, so they can't
  // share contents with other windows.
  if (backing_)
    backing_->RegisterOutputDevice(this);
}

SoftwareOutputDeviceWin::~SoftwareOutputDeviceWin() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!in_paint_);
  if (backing_)
    backing_->UnregisterOutputDevice(this);
}

void SoftwareOutputDeviceWin::Resize(const gfx::Size& viewport_pixel_size,
                                     float scale_factor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!in_paint_);

  if (viewport_pixel_size_ == viewport_pixel_size)
    return;

  viewport_pixel_size_ = viewport_pixel_size;
  if (backing_)
    backing_->Resized();
  contents_.reset();
}

SkCanvas* SoftwareOutputDeviceWin::BeginPaint(const gfx::Rect& damage_rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!in_paint_);
  if (!contents_) {
    HANDLE shared_section = NULL;
    bool can_create_contents = true;
    if (backing_) {
      base::SharedMemory* memory =
          backing_->GetSharedMemory(viewport_pixel_size_);
      if (memory) {
        shared_section = memory->handle().GetHandle();
      } else {
        can_create_contents = false;
      }
    }
    if (can_create_contents) {
      contents_ = skia::CreatePlatformCanvasWithSharedSection(
          viewport_pixel_size_.width(), viewport_pixel_size_.height(), true,
          shared_section, skia::CRASH_ON_FAILURE);
    }
  }

  damage_rect_ = damage_rect;
  in_paint_ = true;
  return contents_.get();
}

void SoftwareOutputDeviceWin::EndPaint() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(in_paint_);

  in_paint_ = false;
  SoftwareOutputDevice::EndPaint();

  if (!contents_)
    return;

  gfx::Rect rect = damage_rect_;
  rect.Intersect(gfx::Rect(viewport_pixel_size_));
  if (rect.IsEmpty())
    return;

  HDC dib_dc = skia::GetNativeDrawingContext(contents_.get());

  if (use_layered_window_) {
    RECT wr;
    GetWindowRect(hwnd_, &wr);
    SIZE size = {wr.right - wr.left, wr.bottom - wr.top};
    POINT position = {wr.left, wr.top};
    POINT zero = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0x00, 0xFF, AC_SRC_ALPHA};

    DWORD style = GetWindowLong(hwnd_, GWL_EXSTYLE);
    DCHECK(!(style & WS_EX_COMPOSITED));
    if (!(style & WS_EX_LAYERED))
      SetWindowLong(hwnd_, GWL_EXSTYLE, style | WS_EX_LAYERED);

    ::UpdateLayeredWindow(hwnd_, NULL, &position, &size, dib_dc, &zero,
                          RGB(0xFF, 0xFF, 0xFF), &blend, ULW_ALPHA);
  } else {
    HDC hdc = ::GetDC(hwnd_);
    RECT src_rect = rect.ToRECT();
    skia::CopyHDC(dib_dc, hdc, rect.x(), rect.y(),
                  contents_.get()->imageInfo().isOpaque(), src_rect,
                  contents_.get()->getTotalMatrix());

    ::ReleaseDC(hwnd_, hdc);
  }
}

void SoftwareOutputDeviceWin::ReleaseContents() {
  DCHECK(!in_paint_);
  contents_.reset();
}

}  // namespace viz
