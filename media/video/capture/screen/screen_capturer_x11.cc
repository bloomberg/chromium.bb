// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capturer.h"

#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <set>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "media/video/capture/screen/differ.h"
#include "media/video/capture/screen/mouse_cursor_shape.h"
#include "media/video/capture/screen/screen_capture_frame_queue.h"
#include "media/video/capture/screen/screen_capturer_helper.h"
#include "media/video/capture/screen/x11/x_server_pixel_buffer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace media {

namespace {

// A class to perform video frame capturing for Linux.
class ScreenCapturerLinux : public ScreenCapturer {
 public:
  ScreenCapturerLinux();
  virtual ~ScreenCapturerLinux();

  // TODO(ajwong): Do we really want this to be synchronous?
  bool Init(bool use_x_damage);

  // DesktopCapturer interface.
  virtual void Start(Callback* delegate) OVERRIDE;
  virtual void Capture(const webrtc::DesktopRegion& region) OVERRIDE;

  // ScreenCapturer interface.
  virtual void SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) OVERRIDE;

 private:
  void InitXDamage();

  // Read and handle all currently-pending XEvents.
  // In the DAMAGE case, process the XDamage events and store the resulting
  // damage rectangles in the ScreenCapturerHelper.
  // In all cases, call ScreenConfigurationChanged() in response to any
  // ConfigNotify events.
  void ProcessPendingXEvents();

  // Capture the cursor image and notify the delegate if it was captured.
  void CaptureCursor();

  // Capture screen pixels to the current buffer in the queue. In the DAMAGE
  // case, the ScreenCapturerHelper already holds the list of invalid rectangles
  // from ProcessPendingXEvents(). In the non-DAMAGE case, this captures the
  // whole screen, then calculates some invalid rectangles that include any
  // differences between this and the previous capture.
  webrtc::DesktopFrame* CaptureScreen();

  // Called when the screen configuration is changed. |root_window_size|
  // specifies the most recent size of the root window.
  void ScreenConfigurationChanged(const webrtc::DesktopSize& root_window_size);

  // Synchronize the current buffer with |last_buffer_|, by copying pixels from
  // the area of |last_invalid_rects|.
  // Note this only works on the assumption that kNumBuffers == 2, as
  // |last_invalid_rects| holds the differences from the previous buffer and
  // the one prior to that (which will then be the current buffer).
  void SynchronizeFrame();

  void DeinitXlib();

  // Capture a rectangle from |x_server_pixel_buffer_|, and copy the data into
  // |frame|.
  void CaptureRect(const webrtc::DesktopRect& rect,
                   webrtc::DesktopFrame* frame);

  // We expose two forms of blitting to handle variations in the pixel format.
  // In FastBlit, the operation is effectively a memcpy.
  void FastBlit(uint8* image,
                const webrtc::DesktopRect& rect,
                webrtc::DesktopFrame* frame);
  void SlowBlit(uint8* image,
                const webrtc::DesktopRect& rect,
                webrtc::DesktopFrame* frame);

  // Returns the number of bits |mask| has to be shifted left so its last
  // (most-significant) bit set becomes the most-significant bit of the word.
  // When |mask| is 0 the function returns 31.
  static uint32 GetRgbShift(uint32 mask);

  Callback* callback_;
  MouseShapeObserver* mouse_shape_observer_;

  // X11 graphics context.
  Display* display_;
  GC gc_;
  Window root_window_;

  // Last known dimensions of the root window.
  webrtc::DesktopSize root_window_size_;

  // XFixes.
  bool has_xfixes_;
  int xfixes_event_base_;
  int xfixes_error_base_;

  // XDamage information.
  bool use_damage_;
  Damage damage_handle_;
  int damage_event_base_;
  int damage_error_base_;
  XserverRegion damage_region_;

  // Access to the X Server's pixel buffer.
  XServerPixelBuffer x_server_pixel_buffer_;

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  ScreenCapturerHelper helper_;

  // Queue of the frames buffers.
  ScreenCaptureFrameQueue queue_;

  // Invalid region from the previous capture. This is used to synchronize the
  // current with the last buffer used.
  webrtc::DesktopRegion last_invalid_region_;

  // |Differ| for use when polling for changes.
  scoped_ptr<Differ> differ_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCapturerLinux);
};

ScreenCapturerLinux::ScreenCapturerLinux()
    : callback_(NULL),
      mouse_shape_observer_(NULL),
      display_(NULL),
      gc_(NULL),
      root_window_(BadValue),
      has_xfixes_(false),
      xfixes_event_base_(-1),
      xfixes_error_base_(-1),
      use_damage_(false),
      damage_handle_(0),
      damage_event_base_(-1),
      damage_error_base_(-1),
      damage_region_(0) {
  helper_.SetLogGridSize(4);
}

ScreenCapturerLinux::~ScreenCapturerLinux() {
  DeinitXlib();
}

bool ScreenCapturerLinux::Init(bool use_x_damage) {
  // TODO(ajwong): We should specify the display string we are attaching to
  // in the constructor.
  display_ = XOpenDisplay(NULL);
  if (!display_) {
    LOG(ERROR) << "Unable to open display";
    return false;
  }

  root_window_ = RootWindow(display_, DefaultScreen(display_));
  if (root_window_ == BadValue) {
    LOG(ERROR) << "Unable to get the root window";
    DeinitXlib();
    return false;
  }

  gc_ = XCreateGC(display_, root_window_, 0, NULL);
  if (gc_ == NULL) {
    LOG(ERROR) << "Unable to get graphics context";
    DeinitXlib();
    return false;
  }

  // Check for XFixes extension. This is required for cursor shape
  // notifications, and for our use of XDamage.
  if (XFixesQueryExtension(display_, &xfixes_event_base_,
                           &xfixes_error_base_)) {
    has_xfixes_ = true;
  } else {
    LOG(INFO) << "X server does not support XFixes.";
  }

  // Register for changes to the dimensions of the root window.
  XSelectInput(display_, root_window_, StructureNotifyMask);

  root_window_size_ = XServerPixelBuffer::GetRootWindowSize(display_);
  x_server_pixel_buffer_.Init(display_, root_window_size_);

  if (has_xfixes_) {
    // Register for changes to the cursor shape.
    XFixesSelectCursorInput(display_, root_window_,
                            XFixesDisplayCursorNotifyMask);
  }

  if (use_x_damage) {
    InitXDamage();
  }

  return true;
}

void ScreenCapturerLinux::InitXDamage() {
  // Our use of XDamage requires XFixes.
  if (!has_xfixes_) {
    return;
  }

  // Check for XDamage extension.
  if (!XDamageQueryExtension(display_, &damage_event_base_,
                             &damage_error_base_)) {
    LOG(INFO) << "X server does not support XDamage.";
    return;
  }

  // TODO(lambroslambrou): Disable DAMAGE in situations where it is known
  // to fail, such as when Desktop Effects are enabled, with graphics
  // drivers (nVidia, ATI) that fail to report DAMAGE notifications
  // properly.

  // Request notifications every time the screen becomes damaged.
  damage_handle_ = XDamageCreate(display_, root_window_,
                                 XDamageReportNonEmpty);
  if (!damage_handle_) {
    LOG(ERROR) << "Unable to initialize XDamage.";
    return;
  }

  // Create an XFixes server-side region to collate damage into.
  damage_region_ = XFixesCreateRegion(display_, 0, 0);
  if (!damage_region_) {
    XDamageDestroy(display_, damage_handle_);
    LOG(ERROR) << "Unable to create XFixes region.";
    return;
  }

  use_damage_ = true;
  LOG(INFO) << "Using XDamage extension.";
}

void ScreenCapturerLinux::Start(Callback* callback) {
  DCHECK(!callback_);
  DCHECK(callback);

  callback_ = callback;
}

void ScreenCapturerLinux::Capture(const webrtc::DesktopRegion& region) {
  base::Time capture_start_time = base::Time::Now();

  queue_.MoveToNextFrame();

  // Process XEvents for XDamage and cursor shape tracking.
  ProcessPendingXEvents();

  // If the current frame is from an older generation then allocate a new one.
  // Note that we can't reallocate other buffers at this point, since the caller
  // may still be reading from them.
  if (!queue_.current_frame()) {
    scoped_ptr<webrtc::DesktopFrame> frame(
        new webrtc::BasicDesktopFrame(root_window_size_));
    queue_.ReplaceCurrentFrame(frame.Pass());
  }

  // Refresh the Differ helper used by CaptureFrame(), if needed.
  webrtc::DesktopFrame* frame = queue_.current_frame();
  if (!use_damage_ && (
      !differ_.get() ||
      (differ_->width() != frame->size().width()) ||
      (differ_->height() != frame->size().height()) ||
      (differ_->bytes_per_row() != frame->stride()))) {
    differ_.reset(new Differ(frame->size().width(), frame->size().height(),
                             webrtc::DesktopFrame::kBytesPerPixel,
                             frame->stride()));
  }

  webrtc::DesktopFrame* result = CaptureScreen();
  last_invalid_region_ = result->updated_region();
  result->set_capture_time_ms(
      (base::Time::Now() - capture_start_time).InMillisecondsRoundedUp());
  callback_->OnCaptureCompleted(result);
}

void ScreenCapturerLinux::SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) {
  DCHECK(!mouse_shape_observer_);
  DCHECK(mouse_shape_observer);

  mouse_shape_observer_ = mouse_shape_observer;
}

void ScreenCapturerLinux::ProcessPendingXEvents() {
  // Find the number of events that are outstanding "now."  We don't just loop
  // on XPending because we want to guarantee this terminates.
  int events_to_process = XPending(display_);
  XEvent e;

  for (int i = 0; i < events_to_process; i++) {
    XNextEvent(display_, &e);
    if (use_damage_ && (e.type == damage_event_base_ + XDamageNotify)) {
      XDamageNotifyEvent* event = reinterpret_cast<XDamageNotifyEvent*>(&e);
      DCHECK(event->level == XDamageReportNonEmpty);
    } else if (e.type == ConfigureNotify) {
      const XConfigureEvent& event = e.xconfigure;
      ScreenConfigurationChanged(
          webrtc::DesktopSize(event.width, event.height));
    } else if (has_xfixes_ &&
               e.type == xfixes_event_base_ + XFixesCursorNotify) {
      XFixesCursorNotifyEvent* cne;
      cne = reinterpret_cast<XFixesCursorNotifyEvent*>(&e);
      if (cne->subtype == XFixesDisplayCursorNotify) {
        CaptureCursor();
      }
    } else {
      LOG(WARNING) << "Got unknown event type: " << e.type;
    }
  }
}

void ScreenCapturerLinux::CaptureCursor() {
  DCHECK(has_xfixes_);

  XFixesCursorImage* img = XFixesGetCursorImage(display_);
  if (!img) {
    return;
  }

  scoped_ptr<MouseCursorShape> cursor(new MouseCursorShape());
  cursor->size = webrtc::DesktopSize(img->width, img->height);
  cursor->hotspot = webrtc::DesktopVector(img->xhot, img->yhot);

  int total_bytes = cursor->size.width ()* cursor->size.height() *
      webrtc::DesktopFrame::kBytesPerPixel;
  cursor->data.resize(total_bytes);

  // Xlib stores 32-bit data in longs, even if longs are 64-bits long.
  unsigned long* src = img->pixels;
  uint32* dst = reinterpret_cast<uint32*>(string_as_array(&cursor->data));
  uint32* dst_end = dst + (img->width * img->height);
  while (dst < dst_end) {
    *dst++ = static_cast<uint32>(*src++);
  }
  XFree(img);

  if (mouse_shape_observer_)
    mouse_shape_observer_->OnCursorShapeChanged(cursor.Pass());
}

webrtc::DesktopFrame* ScreenCapturerLinux::CaptureScreen() {
  webrtc::DesktopFrame* frame = queue_.current_frame()->Share();

  // Pass the screen size to the helper, so it can clip the invalid region if it
  // expands that region to a grid.
  helper_.set_size_most_recent(frame->size());

  // In the DAMAGE case, ensure the frame is up-to-date with the previous frame
  // if any.  If there isn't a previous frame, that means a screen-resolution
  // change occurred, and |invalid_rects| will be updated to include the whole
  // screen.
  if (use_damage_ && queue_.previous_frame())
    SynchronizeFrame();

  webrtc::DesktopRegion* updated_region = frame->mutable_updated_region();

  x_server_pixel_buffer_.Synchronize();
  if (use_damage_ && queue_.previous_frame()) {
    // Atomically fetch and clear the damage region.
    XDamageSubtract(display_, damage_handle_, None, damage_region_);
    int rects_num = 0;
    XRectangle bounds;
    XRectangle* rects = XFixesFetchRegionAndBounds(display_, damage_region_,
                                                   &rects_num, &bounds);
    for (int i = 0; i < rects_num; ++i) {
      updated_region->AddRect(webrtc::DesktopRect::MakeXYWH(
          rects[i].x, rects[i].y, rects[i].width, rects[i].height));
    }
    XFree(rects);
    helper_.InvalidateRegion(*updated_region);

    // Capture the damaged portions of the desktop.
    helper_.TakeInvalidRegion(updated_region);

    // Clip the damaged portions to the current screen size, just in case some
    // spurious XDamage notifications were received for a previous (larger)
    // screen size.
    updated_region->IntersectWith(
        webrtc::DesktopRect::MakeSize(root_window_size_));
    for (webrtc::DesktopRegion::Iterator it(*updated_region);
         !it.IsAtEnd(); it.Advance()) {
      CaptureRect(it.rect(), frame);
    }
  } else {
    // Doing full-screen polling, or this is the first capture after a
    // screen-resolution change.  In either case, need a full-screen capture.
    webrtc::DesktopRect screen_rect =
        webrtc::DesktopRect::MakeSize(frame->size());
    CaptureRect(screen_rect, frame);

    if (queue_.previous_frame()) {
      // Full-screen polling, so calculate the invalid rects here, based on the
      // changed pixels between current and previous buffers.
      DCHECK(differ_ != NULL);
      DCHECK(queue_.previous_frame()->data());
      differ_->CalcDirtyRegion(queue_.previous_frame()->data(),
                               frame->data(), updated_region);
    } else {
      // No previous buffer, so always invalidate the whole screen, whether
      // or not DAMAGE is being used.  DAMAGE doesn't necessarily send a
      // full-screen notification after a screen-resolution change, so
      // this is done here.
      updated_region->SetRect(screen_rect);
    }
  }

  return frame;
}

void ScreenCapturerLinux::ScreenConfigurationChanged(
    const webrtc::DesktopSize& root_window_size) {
  root_window_size_ = root_window_size;

  // Make sure the frame buffers will be reallocated.
  queue_.Reset();

  helper_.ClearInvalidRegion();
  x_server_pixel_buffer_.Init(display_, root_window_size_);
}

void ScreenCapturerLinux::SynchronizeFrame() {
  // Synchronize the current buffer with the previous one since we do not
  // capture the entire desktop. Note that encoder may be reading from the
  // previous buffer at this time so thread access complaints are false
  // positives.

  // TODO(hclam): We can reduce the amount of copying here by subtracting
  // |capturer_helper_|s region from |last_invalid_region_|.
  // http://crbug.com/92354
  DCHECK(queue_.previous_frame());

  webrtc::DesktopFrame* current = queue_.current_frame();
  webrtc::DesktopFrame* last = queue_.previous_frame();
  DCHECK_NE(current, last);
  for (webrtc::DesktopRegion::Iterator it(last_invalid_region_);
       !it.IsAtEnd(); it.Advance()) {
    const webrtc::DesktopRect& r = it.rect();
    int offset = r.top() * current->stride() +
        r.left() * webrtc::DesktopFrame::kBytesPerPixel;
    for (int i = 0; i < r.height(); ++i) {
      memcpy(current->data() + offset, last->data() + offset,
             r.width() * webrtc::DesktopFrame::kBytesPerPixel);
      offset += current->size().width() * webrtc::DesktopFrame::kBytesPerPixel;
    }
  }
}

void ScreenCapturerLinux::DeinitXlib() {
  if (gc_) {
    XFreeGC(display_, gc_);
    gc_ = NULL;
  }

  x_server_pixel_buffer_.Release();

  if (display_) {
    if (damage_handle_)
      XDamageDestroy(display_, damage_handle_);
    if (damage_region_)
      XFixesDestroyRegion(display_, damage_region_);
    XCloseDisplay(display_);
    display_ = NULL;
    damage_handle_ = 0;
    damage_region_ = 0;
  }
}

void ScreenCapturerLinux::CaptureRect(const webrtc::DesktopRect& rect,
                                      webrtc::DesktopFrame* frame) {
  uint8* image = x_server_pixel_buffer_.CaptureRect(rect);
  int depth = x_server_pixel_buffer_.GetDepth();
  if ((depth == 24 || depth == 32) &&
      x_server_pixel_buffer_.GetBitsPerPixel() == 32 &&
      x_server_pixel_buffer_.GetRedMask() == 0xff0000 &&
      x_server_pixel_buffer_.GetGreenMask() == 0xff00 &&
      x_server_pixel_buffer_.GetBlueMask() == 0xff) {
    DVLOG(3) << "Fast blitting";
    FastBlit(image, rect, frame);
  } else {
    DVLOG(3) << "Slow blitting";
    SlowBlit(image, rect, frame);
  }
}

void ScreenCapturerLinux::FastBlit(uint8* image,
                                   const webrtc::DesktopRect& rect,
                                   webrtc::DesktopFrame* frame) {
  uint8* src_pos = image;
  int src_stride = x_server_pixel_buffer_.GetStride();
  int dst_x = rect.left(), dst_y = rect.top();

  uint8* dst_pos = frame->data() + frame->stride() * dst_y;
  dst_pos += dst_x * webrtc::DesktopFrame::kBytesPerPixel;

  int height = rect.height();
  int row_bytes = rect.width() * webrtc::DesktopFrame::kBytesPerPixel;
  for (int y = 0; y < height; ++y) {
    memcpy(dst_pos, src_pos, row_bytes);
    src_pos += src_stride;
    dst_pos += frame->stride();
  }
}

void ScreenCapturerLinux::SlowBlit(uint8* image,
                                   const webrtc::DesktopRect& rect,
                                   webrtc::DesktopFrame* frame) {
  int src_stride = x_server_pixel_buffer_.GetStride();
  int dst_x = rect.left(), dst_y = rect.top();
  int width = rect.width(), height = rect.height();

  uint32 red_mask = x_server_pixel_buffer_.GetRedMask();
  uint32 green_mask = x_server_pixel_buffer_.GetGreenMask();
  uint32 blue_mask = x_server_pixel_buffer_.GetBlueMask();

  uint32 red_shift = GetRgbShift(red_mask);
  uint32 green_shift = GetRgbShift(green_mask);
  uint32 blue_shift = GetRgbShift(blue_mask);

  unsigned int bits_per_pixel = x_server_pixel_buffer_.GetBitsPerPixel();

  uint8* dst_pos = frame->data() + frame->stride() * dst_y;
  uint8* src_pos = image;
  dst_pos += dst_x * webrtc::DesktopFrame::kBytesPerPixel;
  // TODO(hclam): Optimize, perhaps using MMX code or by converting to
  // YUV directly
  for (int y = 0; y < height; y++) {
    uint32* dst_pos_32 = reinterpret_cast<uint32*>(dst_pos);
    uint32* src_pos_32 = reinterpret_cast<uint32*>(src_pos);
    uint16* src_pos_16 = reinterpret_cast<uint16*>(src_pos);
    for (int x = 0; x < width; x++) {
      // Dereference through an appropriately-aligned pointer.
      uint32 pixel;
      if (bits_per_pixel == 32)
        pixel = src_pos_32[x];
      else if (bits_per_pixel == 16)
        pixel = src_pos_16[x];
      else
        pixel = src_pos[x];
      uint32 r = (pixel & red_mask) << red_shift;
      uint32 g = (pixel & green_mask) << green_shift;
      uint32 b = (pixel & blue_mask) << blue_shift;

      // Write as 32-bit RGB.
      dst_pos_32[x] = ((r >> 8) & 0xff0000) | ((g >> 16) & 0xff00) |
          ((b >> 24) & 0xff);
    }
    dst_pos += frame->stride();
    src_pos += src_stride;
  }
}

// static
uint32 ScreenCapturerLinux::GetRgbShift(uint32 mask) {
  int shift = 0;
  if ((mask & 0xffff0000u) == 0) {
    mask <<= 16;
    shift += 16;
  }
  if ((mask & 0xff000000u) == 0) {
    mask <<= 8;
    shift += 8;
  }
  if ((mask & 0xf0000000u) == 0) {
    mask <<= 4;
    shift += 4;
  }
  if ((mask & 0xc0000000u) == 0) {
    mask <<= 2;
    shift += 2;
  }
  if ((mask & 0x80000000u) == 0)
    shift += 1;

  return shift;
}

}  // namespace

// static
scoped_ptr<ScreenCapturer> ScreenCapturer::Create() {
  scoped_ptr<ScreenCapturerLinux> capturer(new ScreenCapturerLinux());
  if (!capturer->Init(false))
    capturer.reset();
  return capturer.PassAs<ScreenCapturer>();
}

// static
scoped_ptr<ScreenCapturer> ScreenCapturer::CreateWithXDamage(
    bool use_x_damage) {
  scoped_ptr<ScreenCapturerLinux> capturer(new ScreenCapturerLinux());
  if (!capturer->Init(use_x_damage))
    capturer.reset();
  return capturer.PassAs<ScreenCapturer>();
}

}  // namespace media
