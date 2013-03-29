// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SOFTWARE_OUTPUT_DEVICE_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_SOFTWARE_OUTPUT_DEVICE_WIN_H_

#include "cc/output/software_output_device.h"

#include <windows.h>

namespace ui {
class Compositor;
}

namespace content {

class SoftwareOutputDeviceWin : public cc::SoftwareOutputDevice {
 public:
  explicit SoftwareOutputDeviceWin(ui::Compositor* compositor);

  virtual ~SoftwareOutputDeviceWin();

  virtual void Resize(gfx::Size viewport_size) OVERRIDE;

  virtual void EndPaint(cc::SoftwareFrameData* frame_data) OVERRIDE;

 private:
  ui::Compositor* compositor_;
  HWND hwnd_;
  BITMAPINFO bitmap_info_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SOFTWARE_OUTPUT_DEVICE_WIN_H_
