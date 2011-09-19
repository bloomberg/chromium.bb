// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/player_x11/x11_video_renderer.h"

#include <dlfcn.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>

#include "base/message_loop.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"

// Creates a 32-bit XImage.
static XImage* CreateImage(Display* display, int width, int height) {
  LOG(INFO) << "Allocating XImage " << width << "x" << height;
  return  XCreateImage(display,
                       DefaultVisual(display, DefaultScreen(display)),
                       DefaultDepth(display, DefaultScreen(display)),
                       ZPixmap,
                       0,
                       static_cast<char*>(malloc(width * height * 4)),
                       width,
                       height,
                       32,
                       width * 4);
}

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
    // Not all X servers support xRGB32 formats. However, the XRender spec
    // says that they must support an ARGB32 format, so we can always return
    // that.
    pictformat = XRenderFindStandardFormat(dpy, PictStandardARGB32);
    CHECK(pictformat) << "XRender ARGB32 not supported.";
  }

  return pictformat;
}

X11VideoRenderer::X11VideoRenderer(Display* display, Window window,
                                   MessageLoop* main_message_loop)
    : display_(display),
      window_(window),
      image_(NULL),
      picture_(0),
      use_render_(false),
      main_message_loop_(main_message_loop) {
}

X11VideoRenderer::~X11VideoRenderer() {}

void X11VideoRenderer::OnStop(media::FilterCallback* callback) {
  if (image_) {
    XDestroyImage(image_);
  }
  XRenderFreePicture(display_, picture_);
  if (callback) {
    callback->Run();
    delete callback;
  }
}

bool X11VideoRenderer::OnInitialize(media::VideoDecoder* decoder) {
  LOG(INFO) << "Initializing X11 Renderer...";

  // Resize the window to fit that of the video.
  int width = decoder->natural_size().width();
  int height = decoder->natural_size().height();
  XResizeWindow(display_, window_, width, height);

  // Allocate an XImage for caching RGB result.
  image_ = CreateImage(display_, width, height);

  // Testing XRender support. We'll use the very basic of XRender
  // so if it presents it is already good enough. We don't need
  // to check its version.
  int dummy;
  use_render_ = XRenderQueryExtension(display_, &dummy, &dummy);

  if (use_render_) {
    LOG(INFO) << "Using XRender extension.";

    // If we are using XRender, we'll create a picture representing the
    // window.
    XWindowAttributes attr;
    XGetWindowAttributes(display_, window_, &attr);

    XRenderPictFormat* pictformat = XRenderFindVisualFormat(
        display_,
        attr.visual);
    CHECK(pictformat) << "XRender does not support default visual";

    picture_ = XRenderCreatePicture(display_, window_, pictformat, 0, NULL);
    CHECK(picture_) << "Backing picture not created";
  }

  return true;
}

void X11VideoRenderer::OnFrameAvailable() {
  main_message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &X11VideoRenderer::PaintOnMainThread));
}

void X11VideoRenderer::PaintOnMainThread() {
  DCHECK_EQ(main_message_loop_, MessageLoop::current());

  scoped_refptr<media::VideoFrame> video_frame;
  GetCurrentFrame(&video_frame);
  if (!video_frame) {
    // TODO(jiesun): Use color fill rather than create black frame then scale.
    PutCurrentFrame(video_frame);
    return;
  }

  int width = video_frame->width();
  int height = video_frame->height();

  // Check if we need to re-allocate our XImage.
  if (image_->width != width || image_->height != height) {
    LOG(INFO) << "Detection resolution change: "
              << image_->width << "x" << image_->height << " -> "
              << width << "x" << height;
    XDestroyImage(image_);
    image_ = CreateImage(display_, width, height);
  }

  // Convert YUV frame to RGB.
  DCHECK(video_frame->format() == media::VideoFrame::YV12 ||
         video_frame->format() == media::VideoFrame::YV16);
  DCHECK(video_frame->stride(media::VideoFrame::kUPlane) ==
         video_frame->stride(media::VideoFrame::kVPlane));
  DCHECK(video_frame->planes() == media::VideoFrame::kNumYUVPlanes);

  DCHECK(image_->data);
  media::YUVType yuv_type =
      (video_frame->format() == media::VideoFrame::YV12) ?
      media::YV12 : media::YV16;
  media::ConvertYUVToRGB32(video_frame->data(media::VideoFrame::kYPlane),
                           video_frame->data(media::VideoFrame::kUPlane),
                           video_frame->data(media::VideoFrame::kVPlane),
                           (uint8*)image_->data,
                           video_frame->width(),
                           video_frame->height(),
                           video_frame->stride(media::VideoFrame::kYPlane),
                           video_frame->stride(media::VideoFrame::kUPlane),
                           image_->bytes_per_line,
                           yuv_type);
  PutCurrentFrame(video_frame);

  if (use_render_) {
    // If XRender is used, we'll upload the image to a pixmap. And then
    // creats a picture from the pixmap and composite the picture over
    // the picture represending the window.

    // Creates a XImage.
    XImage image;
    memset(&image, 0, sizeof(image));
    image.width = width;
    image.height = height;
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
                                         width,
                                         height,
                                         32);
    GC gc = XCreateGC(display_, pixmap, 0, NULL);
    XPutImage(display_, pixmap, gc, &image,
              0, 0, 0, 0,
              width, height);
    XFreeGC(display_, gc);

    // Creates the picture representing the pixmap.
    unsigned long picture = XRenderCreatePicture(
        display_, pixmap, GetRenderARGB32Format(display_), 0, NULL);

    // Composite the picture over the picture representing the window.
    XRenderComposite(display_, PictOpSrc, picture, 0,
                     picture_, 0, 0, 0, 0, 0, 0,
                     width, height);

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
            0, 0, 0, 0, width, height);
  XFlush(display_);
  XFreeGC(display_, gc);
}
