// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/player/x11_video_renderer.h"

#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>

#include "media/base/buffers.h"
#include "media/base/yuv_convert.h"

X11VideoRenderer* X11VideoRenderer::instance_ = NULL;

// Returns the picture format for ARGB.
// This method is originally from chrome/common/x11_util.cc.
static XRenderPictFormat* GetRenderARGB32Format(Display* dpy) {
  static XRenderPictFormat* pictformat = NULL;
  if (pictformat)
    return pictformat;

  // First look for a 32-bit format which ignores the alpha value.
  XRenderPictFormat templ;
  templ.depth = 32;
  templ.type = PictTypeDirect;
  templ.direct.red = 16;
  templ.direct.green = 8;
  templ.direct.blue = 0;
  templ.direct.redMask = 0xff;
  templ.direct.greenMask = 0xff;
  templ.direct.blueMask = 0xff;
  templ.direct.alphaMask = 0;

  static const unsigned long kMask =
      PictFormatType | PictFormatDepth |
      PictFormatRed | PictFormatRedMask |
      PictFormatGreen | PictFormatGreenMask |
      PictFormatBlue | PictFormatBlueMask |
      PictFormatAlphaMask;

  pictformat = XRenderFindFormat(dpy, kMask, &templ, 0 /* first result */);

  if (!pictformat) {
    // Not all X servers support xRGB32 formats. However, the XRENDER spec
    // says that they must support an ARGB32 format, so we can always return
    // that.
    pictformat = XRenderFindStandardFormat(dpy, PictStandardARGB32);
    CHECK(pictformat) << "XRENDER ARGB32 not supported.";
  }

  return pictformat;
}

X11VideoRenderer::X11VideoRenderer(Display* display, Window window)
    : display_(display),
      window_(window),
      image_(NULL),
      new_frame_(false),
      picture_(0),
      use_render_(true) {
  // Save the instance of the video renderer.
  CHECK(!instance_);
  instance_ = this;
}

X11VideoRenderer::~X11VideoRenderer() {
  CHECK(instance_);
  instance_ = NULL;
}

// static
bool X11VideoRenderer::IsMediaFormatSupported(
    const media::MediaFormat& media_format) {
  int width = 0;
  int height = 0;
  return ParseMediaFormat(media_format, &width, &height);
}

void X11VideoRenderer::OnStop() {
  XDestroyImage(image_);
  if (use_render_) {
    XRenderFreePicture(display_, picture_);
  }
}

bool X11VideoRenderer::OnInitialize(media::VideoDecoder* decoder) {
  if (!ParseMediaFormat(decoder->media_format(), &width_, &height_))
    return false;

  // Resize the window to fit that of the video.
  XResizeWindow(display_, window_, width_, height_);

  // Testing XRender support. We'll use the very basic of XRender
  // so if it presents it is already good enough. We don't need
  // to check its version.
  int dummy;
  use_render_ = XRenderQueryExtension(display_, &dummy, &dummy);

  if (use_render_) {
    // If we are using XRender, we'll create a picture representing the
    // window.
    XWindowAttributes attr;
    XGetWindowAttributes(display_, window_, &attr);

    XRenderPictFormat* pictformat = XRenderFindVisualFormat(
        display_,
        attr.visual);
    CHECK(pictformat) << "XRENDER does not support default visual";

    picture_ = XRenderCreatePicture(display_, window_, pictformat, 0, NULL);
    CHECK(picture_) << "Backing picture not created";
  }

  // Initialize the XImage to store the output of YUV -> RGB conversion.
  image_ = XCreateImage(display_,
                        DefaultVisual(display_, DefaultScreen(display_)),
                        DefaultDepth(display_, DefaultScreen(display_)),
                        ZPixmap,
                        0,
                        static_cast<char*>(malloc(width_ * height_ * 4)),
                        width_,
                        height_,
                        32,
                        width_ * 4);
  DCHECK(image_);
  return true;
}

void X11VideoRenderer::OnFrameAvailable() {
  AutoLock auto_lock(lock_);
  new_frame_ = true;
}

void X11VideoRenderer::Paint() {
  // Use |new_frame_| to prevent overdraw since Paint() is called more
  // often than needed. It is OK to lock only this flag and we don't
  // want to lock the whole function because this method takes a long
  // time to complete.
  {
    AutoLock auto_lock(lock_);
    if (!new_frame_)
      return;
    new_frame_ = false;
  }

  scoped_refptr<media::VideoFrame> video_frame;
  GetCurrentFrame(&video_frame);

  if (!image_ || !video_frame)
    return;

  // Convert YUV frame to RGB.
  media::VideoSurface frame_in;
  if (video_frame->Lock(&frame_in)) {
    DCHECK(frame_in.format == media::VideoSurface::YV12 ||
           frame_in.format == media::VideoSurface::YV16);
    DCHECK(frame_in.strides[media::VideoSurface::kUPlane] ==
           frame_in.strides[media::VideoSurface::kVPlane]);
    DCHECK(frame_in.planes == media::VideoSurface::kNumYUVPlanes);
    DCHECK(image_->data);

    media::YUVType yuv_type = (frame_in.format == media::VideoSurface::YV12) ?
                              media::YV12 : media::YV16;
    media::ConvertYUVToRGB32(frame_in.data[media::VideoSurface::kYPlane],
                             frame_in.data[media::VideoSurface::kUPlane],
                             frame_in.data[media::VideoSurface::kVPlane],
                             (uint8*)image_->data,
                             frame_in.width,
                             frame_in.height,
                             frame_in.strides[media::VideoSurface::kYPlane],
                             frame_in.strides[media::VideoSurface::kUPlane],
                             image_->bytes_per_line,
                             yuv_type);
    video_frame->Unlock();
  } else {
    NOTREACHED();
  }

  if (use_render_) {
    // If XRender is used, we'll upload the image to a pixmap. And then
    // creats a picture from the pixmap and composite the picture over
    // the picture represending the window.

    // Creates a XImage.
    XImage image;
    memset(&image, 0, sizeof(image));
    image.width = width_;
    image.height = height_;
    image.depth = 32;
    image.bits_per_pixel = 32;
    image.format = ZPixmap;
    image.byte_order = LSBFirst;
    image.bitmap_unit = 8;
    image.bitmap_bit_order = LSBFirst;
    image.bytes_per_line = image_->bytes_per_line;
    image.red_mask = 0xff;
    image.green_mask = 0xff00;
    image.blue_mask = 0xff0000;
    image.data = image_->data;

    // Creates a pixmap and uploads from the XImage.
    unsigned long pixmap = XCreatePixmap(display_,
                                         window_,
                                         width_,
                                         height_,
                                         32);
    GC gc = XCreateGC(display_, pixmap, 0, NULL);
    XPutImage(display_, pixmap, gc, &image,
              0, 0, 0, 0,
              width_, height_);
    XFreeGC(display_, gc);

    // Creates the picture representing the pixmap.
    unsigned long picture = XRenderCreatePicture(
        display_, pixmap, GetRenderARGB32Format(display_), 0, NULL);

    // Composite the picture over the picture representing the window.
    XRenderComposite(display_, PictOpSrc, picture, 0,
                     picture_, 0, 0, 0, 0, 0, 0,
                     width_, height_);

    XRenderFreePicture(display_, picture);
    XFreePixmap(display_, pixmap);
    return;
  }

  // If XRender is not used, simply put the image to the server.
  // This will have a tearing effect but this is OK.
  // TODO(hclam): Upload the image to a pixmap and do XCopyArea()
  // to the window.
  GC gc = XCreateGC(display_, window_, 0, NULL);
  XPutImage(display_, window_, gc, image_,
            0, 0, 0, 0, width_, height_);
  XFlush(display_);
  XFreeGC(display_, gc);
}
