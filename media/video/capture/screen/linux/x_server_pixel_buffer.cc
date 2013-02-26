// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/linux/x_server_pixel_buffer.h"

#include <sys/shm.h>

#include "base/logging.h"

#if defined(TOOLKIT_GTK)
#include <gdk/gdk.h>
#else  // !defined(TOOLKIT_GTK)
#include <X11/Xlib.h>
#endif  // !defined(TOOLKIT_GTK)

namespace {

#if defined(TOOLKIT_GTK)
// GDK sets error handler for Xlib errors, so we need to use it to
// trap X errors when this code is compiled with GTK.
void EnableXServerErrorTrap() {
  gdk_error_trap_push();
}

int GetLastXServerError() {
  return gdk_error_trap_pop();
}

#else  // !defined(TOOLKIT_GTK)

static bool g_xserver_error_trap_enabled = false;
static int g_last_xserver_error_code = 0;

int XServerErrorHandler(Display* display, XErrorEvent* error_event) {
  DCHECK(g_xserver_error_trap_enabled);
  g_last_xserver_error_code = error_event->error_code;
  return 0;
}

void EnableXServerErrorTrap() {
  DCHECK(!g_xserver_error_trap_enabled);
  XSetErrorHandler(&XServerErrorHandler);
  g_xserver_error_trap_enabled = true;
  g_last_xserver_error_code = 0;
}

int GetLastXServerError() {
  DCHECK(g_xserver_error_trap_enabled);
  XSetErrorHandler(NULL);
  g_xserver_error_trap_enabled = false;
  return g_last_xserver_error_code;
}

#endif  // !defined(TOOLKIT_GTK)

}  // namespace

namespace media {

XServerPixelBuffer::XServerPixelBuffer()
    : display_(NULL), root_window_(0),
      root_window_size_(SkISize::Make(0, 0)), x_image_(NULL),
      shm_segment_info_(NULL), shm_pixmap_(0), shm_gc_(NULL) {
}

XServerPixelBuffer::~XServerPixelBuffer() {
  Release();
}

void XServerPixelBuffer::Release() {
  if (x_image_) {
    XDestroyImage(x_image_);
    x_image_ = NULL;
  }
  if (shm_pixmap_) {
    XFreePixmap(display_, shm_pixmap_);
    shm_pixmap_ = 0;
  }
  if (shm_gc_) {
    XFreeGC(display_, shm_gc_);
    shm_gc_ = NULL;
  }
  if (shm_segment_info_) {
    if (shm_segment_info_->shmaddr != reinterpret_cast<char*>(-1))
      shmdt(shm_segment_info_->shmaddr);
    if (shm_segment_info_->shmid != -1)
      shmctl(shm_segment_info_->shmid, IPC_RMID, 0);
    delete shm_segment_info_;
    shm_segment_info_ = NULL;
  }
}

void XServerPixelBuffer::Init(Display* display,
                              const SkISize& screen_size) {
  Release();
  display_ = display;
  root_window_size_ = screen_size;
  int default_screen = DefaultScreen(display_);
  root_window_ = RootWindow(display_, default_screen);
  InitShm(default_screen);
}

// static
SkISize XServerPixelBuffer::GetRootWindowSize(Display* display) {
  XWindowAttributes root_attr;
  XGetWindowAttributes(display, DefaultRootWindow(display), &root_attr);
  return SkISize::Make(root_attr.width, root_attr.height);
}

void XServerPixelBuffer::InitShm(int screen) {
  Visual* default_visual = DefaultVisual(display_, screen);
  int default_depth = DefaultDepth(display_, screen);

  int major, minor;
  Bool havePixmaps;
  if (!XShmQueryVersion(display_, &major, &minor, &havePixmaps))
    // Shared memory not supported. CaptureRect will use the XImage API instead.
    return;

  bool using_shm = false;
  shm_segment_info_ = new XShmSegmentInfo;
  shm_segment_info_->shmid = -1;
  shm_segment_info_->shmaddr = reinterpret_cast<char*>(-1);
  shm_segment_info_->readOnly = False;
  x_image_ = XShmCreateImage(display_, default_visual, default_depth, ZPixmap,
                             0, shm_segment_info_, root_window_size_.width(),
                             root_window_size_.height());
  if (x_image_) {
    shm_segment_info_->shmid = shmget(
        IPC_PRIVATE, x_image_->bytes_per_line * x_image_->height,
        IPC_CREAT | 0600);
    if (shm_segment_info_->shmid != -1) {
      shm_segment_info_->shmaddr = x_image_->data =
          reinterpret_cast<char*>(shmat(shm_segment_info_->shmid, 0, 0));
      if (x_image_->data != reinterpret_cast<char*>(-1)) {
        EnableXServerErrorTrap();
        using_shm = XShmAttach(display_, shm_segment_info_);
        XSync(display_, False);
        if (GetLastXServerError() != 0)
          using_shm = false;
        if (using_shm) {
          VLOG(1) << "Using X shared memory segment "
                  << shm_segment_info_->shmid;
        }
      }
    } else {
      LOG(WARNING) << "Failed to get shared memory segment. "
                      "Performance may be degraded.";
    }
  }

  if (!using_shm) {
    LOG(WARNING) << "Not using shared memory. Performance may be degraded.";
    Release();
    return;
  }

  if (havePixmaps)
    havePixmaps = InitPixmaps(default_depth);

  shmctl(shm_segment_info_->shmid, IPC_RMID, 0);
  shm_segment_info_->shmid = -1;

  VLOG(1) << "Using X shared memory extension v" << major << "." << minor
          << " with" << (havePixmaps?"":"out") << " pixmaps.";
}

bool XServerPixelBuffer::InitPixmaps(int depth) {
  if (XShmPixmapFormat(display_) != ZPixmap)
    return false;

  EnableXServerErrorTrap();
  shm_pixmap_ = XShmCreatePixmap(display_, root_window_,
                                 shm_segment_info_->shmaddr,
                                 shm_segment_info_,
                                 root_window_size_.width(),
                                 root_window_size_.height(), depth);
  XSync(display_, False);
  if (GetLastXServerError() != 0) {
    // |shm_pixmap_| is not not valid because the request was not processed
    // by the X Server, so zero it.
    shm_pixmap_ = 0;
    return false;
  }

  EnableXServerErrorTrap();
  XGCValues shm_gc_values;
  shm_gc_values.subwindow_mode = IncludeInferiors;
  shm_gc_values.graphics_exposures = False;
  shm_gc_ = XCreateGC(display_, root_window_,
                      GCSubwindowMode | GCGraphicsExposures,
                      &shm_gc_values);
  XSync(display_, False);
  if (GetLastXServerError() != 0) {
    XFreePixmap(display_, shm_pixmap_);
    shm_pixmap_ = 0;
    shm_gc_ = 0;  // See shm_pixmap_ comment above.
    return false;
  }

  return true;
}

void XServerPixelBuffer::Synchronize() {
  if (shm_segment_info_ && !shm_pixmap_) {
    // XShmGetImage can fail if the display is being reconfigured.
    EnableXServerErrorTrap();
    XShmGetImage(display_, root_window_, x_image_, 0, 0, AllPlanes);
    GetLastXServerError();
  }
}

uint8* XServerPixelBuffer::CaptureRect(const SkIRect& rect) {
  DCHECK(SkIRect::MakeSize(root_window_size_).contains(rect));
  if (shm_segment_info_) {
    if (shm_pixmap_) {
      XCopyArea(display_, root_window_, shm_pixmap_, shm_gc_,
                rect.fLeft, rect.fTop, rect.width(), rect.height(),
                rect.fLeft, rect.fTop);
      XSync(display_, False);
    }
    return reinterpret_cast<uint8*>(x_image_->data) +
        rect.fTop * x_image_->bytes_per_line +
        rect.fLeft * x_image_->bits_per_pixel / 8;
  } else {
    if (x_image_)
      XDestroyImage(x_image_);
    x_image_ = XGetImage(display_, root_window_, rect.fLeft, rect.fTop,
                         rect.width(), rect.height(), AllPlanes, ZPixmap);
    return reinterpret_cast<uint8*>(x_image_->data);
  }
}

int XServerPixelBuffer::GetStride() const {
  return x_image_->bytes_per_line;
}

int XServerPixelBuffer::GetDepth() const {
  return x_image_->depth;
}

int XServerPixelBuffer::GetBitsPerPixel() const {
  return x_image_->bits_per_pixel;
}

int XServerPixelBuffer::GetRedMask() const {
  return x_image_->red_mask;
}

int XServerPixelBuffer::GetBlueMask() const {
  return x_image_->blue_mask;
}

int XServerPixelBuffer::GetGreenMask() const {
  return x_image_->green_mask;
}

}  // namespace media
