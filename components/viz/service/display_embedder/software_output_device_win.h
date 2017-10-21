// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_H_

#include <stddef.h>
#include <windows.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/viz_service_export.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class SharedMemory;
}

namespace viz {
class SoftwareOutputDeviceWin;

class VIZ_SERVICE_EXPORT OutputDeviceBacking {
 public:
  OutputDeviceBacking();
  ~OutputDeviceBacking();

  void Resized();
  void RegisterOutputDevice(SoftwareOutputDeviceWin* device);
  void UnregisterOutputDevice(SoftwareOutputDeviceWin* device);
  base::SharedMemory* GetSharedMemory(const gfx::Size& size);

 private:
  size_t GetMaxByteSize();

  std::vector<SoftwareOutputDeviceWin*> devices_;
  std::unique_ptr<base::SharedMemory> backing_;
  size_t created_byte_size_;

  DISALLOW_COPY_AND_ASSIGN(OutputDeviceBacking);
};

class VIZ_SERVICE_EXPORT SoftwareOutputDeviceWin : public SoftwareOutputDevice {
 public:
  SoftwareOutputDeviceWin(OutputDeviceBacking* backing,
                          gfx::AcceleratedWidget widget);
  ~SoftwareOutputDeviceWin() override;

  void Resize(const gfx::Size& viewport_pixel_size,
              float scale_factor) override;
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override;
  void EndPaint() override;

  gfx::Size viewport_pixel_size() const { return viewport_pixel_size_; }
  void ReleaseContents();

 private:
  HWND hwnd_;
  std::unique_ptr<SkCanvas> contents_;
  bool is_hwnd_composited_;
  OutputDeviceBacking* backing_;
  bool in_paint_;
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceWin);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_H_
