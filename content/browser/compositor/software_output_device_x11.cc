// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/software_output_device_x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/x/x11_types.h"

namespace content {

SoftwareOutputDeviceX11::SoftwareOutputDeviceX11(ui::Compositor* compositor)
    : compositor_(compositor), display_(gfx::GetXDisplay()), gc_(NULL) {
  // TODO(skaslev) Remove this when crbug.com/180702 is fixed.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gc_ = XCreateGC(display_, compositor_->widget(), 0, NULL);
  if (!XGetWindowAttributes(display_, compositor_->widget(), &attributes_)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window "
               << compositor_->widget();
    return;
  }
}

SoftwareOutputDeviceX11::~SoftwareOutputDeviceX11() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  XFreeGC(display_, gc_);
}

void SoftwareOutputDeviceX11::EndPaint(cc::SoftwareFrameData* frame_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(canvas_);
  DCHECK(frame_data);

  if (!canvas_)
    return;

  SoftwareOutputDevice::EndPaint(frame_data);

  gfx::Rect rect = damage_rect_;
  rect.Intersect(gfx::Rect(viewport_size_));
  if (rect.IsEmpty())
    return;

  // TODO(jbauman): Switch to XShmPutImage since it's async.
  SkImageInfo info;
  size_t rowBytes;
  const void* addr = canvas_->peekPixels(&info, &rowBytes);
  gfx::PutARGBImage(display_,
                    attributes_.visual,
                    attributes_.depth,
                    compositor_->widget(),
                    gc_,
                    static_cast<const uint8*>(addr),
                    viewport_size_.width(),
                    viewport_size_.height(),
                    rect.x(),
                    rect.y(),
                    rect.x(),
                    rect.y(),
                    rect.width(),
                    rect.height());
}

}  // namespace content
