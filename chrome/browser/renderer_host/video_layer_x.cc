// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/video_layer_x.h"

#include "app/x11_util_internal.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "media/base/yuv_convert.h"


// Assume that somewhere along the line, someone will do width * height * 4
// with signed numbers. If the maximum value is 2**31, then 2**31 / 4 =
// 2**29 and floor(sqrt(2**29)) = 23170.

// Max height and width for layers
static const int kMaxVideoLayerSize = 23170;

VideoLayerX::VideoLayerX(RenderWidgetHost* widget,
                         const gfx::Size& size,
                         void* visual,
                         int depth)
    : VideoLayer(widget, size),
      visual_(visual),
      depth_(depth),
      display_(x11_util::GetXDisplay()),
      rgb_frame_size_(0) {
  DCHECK(!size.IsEmpty());

  // Create our pixmap + GC representing an RGB version of a video frame.
  pixmap_ = XCreatePixmap(display_, x11_util::GetX11RootWindow(),
                          size.width(), size.height(), depth_);
  pixmap_gc_ = XCreateGC(display_, pixmap_, 0, NULL);
  pixmap_bpp_ = x11_util::BitsPerPixelForPixmapDepth(display_, depth_);
}

VideoLayerX::~VideoLayerX() {
  // In unit tests, |display_| may be NULL.
  if (!display_)
    return;

  XFreePixmap(display_, pixmap_);
  XFreeGC(display_, static_cast<GC>(pixmap_gc_));
}

void VideoLayerX::CopyTransportDIB(RenderProcessHost* process,
                                   TransportDIB::Id bitmap,
                                   const gfx::Rect& bitmap_rect) {
  if (!display_)
    return;

  if (bitmap_rect.IsEmpty())
    return;

  if (bitmap_rect.size() != size()) {
    LOG(ERROR) << "Scaled video layer not supported.";
    return;
  }

  // Save location and size of destination bitmap.
  rgb_rect_ = bitmap_rect;

  const int width = bitmap_rect.width();
  const int height = bitmap_rect.height();
  const size_t new_rgb_frame_size = static_cast<size_t>(width * height * 4);

  if (width <= 0 || width > kMaxVideoLayerSize ||
      height <= 0 || height > kMaxVideoLayerSize)
    return;

  // Lazy allocate |rgb_frame_|.
  if (!rgb_frame_.get() || rgb_frame_size_ < new_rgb_frame_size) {
    // TODO(scherkus): handle changing dimensions and re-allocating.
    CHECK(size() == rgb_rect_.size());
    rgb_frame_.reset(new uint8[new_rgb_frame_size]);
    rgb_frame_size_ = new_rgb_frame_size;
  }

  TransportDIB* dib = process->GetTransportDIB(bitmap);
  if (!dib)
    return;

  // Perform colour space conversion.
  const uint8* y_plane = reinterpret_cast<uint8*>(dib->memory());
  const uint8* u_plane = y_plane + width * height;
  const uint8* v_plane = u_plane + ((width * height) >> 2);
  media::ConvertYUVToRGB32(y_plane,
                           u_plane,
                           v_plane,
                           rgb_frame_.get(),
                           width,
                           height,
                           width,
                           width / 2,
                           width * 4,
                           media::YV12);

  // Draw ARGB frame onto our pixmap.
  x11_util::PutARGBImage(display_, visual_, depth_, pixmap_, pixmap_gc_,
                         rgb_frame_.get(), width, height);
}

void VideoLayerX::XShow(XID target) {
  if (rgb_rect_.IsEmpty())
    return;

  XCopyArea(display_, pixmap_, target, static_cast<GC>(pixmap_gc_),
            0, 0, rgb_rect_.width(), rgb_rect_.height(),
            rgb_rect_.x(), rgb_rect_.y());
}
