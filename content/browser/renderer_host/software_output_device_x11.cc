// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/software_output_device_x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/compositor/compositor.h"

namespace content {

SoftwareOutputDeviceX11::SoftwareOutputDeviceX11(ui::Compositor* compositor)
    : compositor_(compositor),
      display_(ui::GetXDisplay()),
      gc_(NULL),
      image_(NULL) {
  // TODO(skaslev) Remove this when crbug.com/180702 is fixed.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gc_ = XCreateGC(display_, compositor_->widget(), 0, NULL);
}

SoftwareOutputDeviceX11::~SoftwareOutputDeviceX11() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  XFreeGC(display_, gc_);
  ClearImage();
}

void SoftwareOutputDeviceX11::ClearImage() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (image_) {
    // XDestroyImage deletes the data referenced by the image which
    // is actually owned by the device_. So we have to reset data here.
    image_->data = NULL;
    XDestroyImage(image_);
    image_ = NULL;
  }
}

void SoftwareOutputDeviceX11::Resize(const gfx::Size& viewport_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  cc::SoftwareOutputDevice::Resize(viewport_size);

  ClearImage();
  if (!device_)
    return;

  const SkBitmap& bitmap = device_->accessBitmap(false);
  image_ = XCreateImage(display_, CopyFromParent,
                        DefaultDepth(display_, DefaultScreen(display_)),
                        ZPixmap, 0,
                        static_cast<char*>(bitmap.getPixels()),
                        viewport_size_.width(), viewport_size_.height(),
                        32, 4 * viewport_size_.width());
}

void SoftwareOutputDeviceX11::EndPaint(cc::SoftwareFrameData* frame_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(device_);
  DCHECK(frame_data == NULL);

  if (!device_)
    return;

  gfx::Rect rect = damage_rect_;
  rect.Intersect(gfx::Rect(viewport_size_));
  if (rect.IsEmpty())
    return;

  // TODO(skaslev): Maybe switch XShmPutImage since it's async.
  XPutImage(display_, compositor_->widget(), gc_, image_,
            rect.x(), rect.y(),
            rect.x(), rect.y(),
            rect.width(), rect.height());
}

}  // namespace content
