// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_x11.h"

#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {

SkiaOutputDeviceX11::SkiaOutputDeviceX11(GrContext* gr_context,
                                         gfx::AcceleratedWidget widget)
    : SkiaOutputDeviceOffscreen(gr_context),
      display_(gfx::GetXDisplay()),
      widget_(widget),
      gc_(XCreateGC(display_, widget_, 0, nullptr)) {
  int result = XGetWindowAttributes(display_, widget_, &attributes_);
  LOG_IF(FATAL, !result) << "XGetWindowAttributes failed for window "
                         << widget_;
  bpp_ = gfx::BitsPerPixelForPixmapDepth(display_, attributes_.depth);
  support_rendr_ = ui::QueryRenderSupport(display_);
}

SkiaOutputDeviceX11::~SkiaOutputDeviceX11() {
  XFreeGC(display_, gc_);
}

void SkiaOutputDeviceX11::Reshape(const gfx::Size& size) {
  SkiaOutputDeviceOffscreen::Reshape(size);
  sk_bitmap_.allocN32Pixels(size.width(), size.height());
}

gfx::SwapResult SkiaOutputDeviceX11::SwapBuffers() {
  gfx::Rect rect(0, 0, draw_surface_->width(), draw_surface_->height());

  bool result = draw_surface_->readPixels(sk_bitmap_, 0, 0);
  LOG_IF(FATAL, !result) << "Failed to read pixels from offscreen SkSurface.";

  if (bpp_ == 32 || bpp_ == 16) {
    // gfx::PutARGBImage() only supports 16 and 32 bpp.
    // TODO(penghuang): Switch to XShmPutImage.
    gfx::PutARGBImage(display_, attributes_.visual, attributes_.depth, widget_,
                      gc_, static_cast<const uint8_t*>(sk_bitmap_.getPixels()),
                      rect.width(), rect.height(), rect.x(), rect.y(), rect.x(),
                      rect.y(), rect.width(), rect.height());
  } else if (support_rendr_) {
    Pixmap pixmap =
        XCreatePixmap(display_, widget_, rect.width(), rect.height(), 32);
    GC gc = XCreateGC(display_, pixmap, 0, nullptr);

    XImage image = {};
    image.width = rect.width();
    image.height = rect.height();
    image.depth = 32;
    image.bits_per_pixel = 32;
    image.format = ZPixmap;
    image.byte_order = LSBFirst;
    image.bitmap_unit = 8;
    image.bitmap_bit_order = LSBFirst;
    image.bytes_per_line = sk_bitmap_.rowBytes();
    image.red_mask = 0xff;
    image.green_mask = 0xff00;
    image.blue_mask = 0xff0000;
    image.data =
        const_cast<char*>(static_cast<const char*>(sk_bitmap_.getPixels()));
    XPutImage(display_, pixmap, gc, &image, rect.x(),
              rect.y() /* source x, y */, 0, 0 /* dest x, y */, rect.width(),
              rect.height());
    XFreeGC(display_, gc);

    Picture picture = XRenderCreatePicture(
        display_, pixmap, ui::GetRenderARGB32Format(display_), 0, nullptr);
    XRenderPictFormat* pictformat =
        XRenderFindVisualFormat(display_, attributes_.visual);
    Picture dest_picture =
        XRenderCreatePicture(display_, widget_, pictformat, 0, nullptr);
    XRenderComposite(display_,
                     PictOpSrc,       // op
                     picture,         // src
                     0,               // mask
                     dest_picture,    // dest
                     0,               // src_x
                     0,               // src_y
                     0,               // mask_x
                     0,               // mask_y
                     rect.x(),        // dest_x
                     rect.y(),        // dest_y
                     rect.width(),    // width
                     rect.height());  // height
    XRenderFreePicture(display_, picture);
    XRenderFreePicture(display_, dest_picture);
    XFreePixmap(display_, pixmap);
  } else {
    NOTIMPLEMENTED();
  }
  XFlush(display_);
  return gfx::SwapResult::SWAP_ACK;
}

}  // namespace viz
