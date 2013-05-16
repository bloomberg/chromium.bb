// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capturer.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include <dlfcn.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <OpenGL/CGLMacro.h>
#include <OpenGL/OpenGL.h>
#include <stddef.h>
#include <set>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_native_library.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "media/video/capture/screen/mac/desktop_configuration.h"
#include "media/video/capture/screen/mac/scoped_pixel_buffer_object.h"
#include "media/video/capture/screen/mouse_cursor_shape.h"
#include "media/video/capture/screen/screen_capture_frame_queue.h"
#include "media/video/capture/screen/screen_capturer_helper.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace media {

namespace {

// Definitions used to dynamic-link to deprecated OS 10.6 functions.
const char* kApplicationServicesLibraryName =
    "/System/Library/Frameworks/ApplicationServices.framework/"
    "ApplicationServices";
typedef void* (*CGDisplayBaseAddressFunc)(CGDirectDisplayID);
typedef size_t (*CGDisplayBytesPerRowFunc)(CGDirectDisplayID);
typedef size_t (*CGDisplayBitsPerPixelFunc)(CGDirectDisplayID);
const char* kOpenGlLibraryName =
    "/System/Library/Frameworks/OpenGL.framework/OpenGL";
typedef CGLError (*CGLSetFullScreenFunc)(CGLContextObj);

// Standard Mac displays have 72dpi, but we report 96dpi for
// consistency with Windows and Linux.
const int kStandardDPI = 96;

// Scales all coordinates of a rect by a specified factor.
webrtc::DesktopRect ScaleAndRoundCGRect(const CGRect& rect, float scale) {
  return webrtc::DesktopRect::MakeLTRB(
    static_cast<int>(floor(rect.origin.x * scale)),
    static_cast<int>(floor(rect.origin.y * scale)),
    static_cast<int>(ceil((rect.origin.x + rect.size.width) * scale)),
    static_cast<int>(ceil((rect.origin.y + rect.size.height) * scale)));
}

// Copy pixels in the |rect| from |src_place| to |dest_plane|.
void CopyRect(const uint8* src_plane,
              int src_plane_stride,
              uint8* dest_plane,
              int dest_plane_stride,
              int bytes_per_pixel,
              const webrtc::DesktopRect& rect) {
  // Get the address of the starting point.
  const int src_y_offset = src_plane_stride * rect.top();
  const int dest_y_offset = dest_plane_stride * rect.top();
  const int x_offset = bytes_per_pixel * rect.left();
  src_plane += src_y_offset + x_offset;
  dest_plane += dest_y_offset + x_offset;

  // Copy pixels in the rectangle line by line.
  const int bytes_per_line = bytes_per_pixel * rect.width();
  const int height = rect.height();
  for (int i = 0 ; i < height; ++i) {
    memcpy(dest_plane, src_plane, bytes_per_line);
    src_plane += src_plane_stride;
    dest_plane += dest_plane_stride;
  }
}

// The amount of time allowed for displays to reconfigure.
const int64 kDisplayConfigurationEventTimeoutInSeconds = 10;

// A class to perform video frame capturing for mac.
class ScreenCapturerMac : public ScreenCapturer {
 public:
  ScreenCapturerMac();
  virtual ~ScreenCapturerMac();

  bool Init();

  // Overridden from ScreenCapturer:
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const webrtc::DesktopRegion& region) OVERRIDE;
  virtual void SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) OVERRIDE;

 private:
  void CaptureCursor();

  void GlBlitFast(const webrtc::DesktopFrame& frame,
                  const webrtc::DesktopRegion& region);
  void GlBlitSlow(const webrtc::DesktopFrame& frame);
  void CgBlitPreLion(const webrtc::DesktopFrame& frame,
                     const webrtc::DesktopRegion& region);
  void CgBlitPostLion(const webrtc::DesktopFrame& frame,
                      const webrtc::DesktopRegion& region);

  // Called when the screen configuration is changed.
  void ScreenConfigurationChanged();

  bool RegisterRefreshAndMoveHandlers();
  void UnregisterRefreshAndMoveHandlers();

  void ScreenRefresh(CGRectCount count, const CGRect *rect_array);
  void ScreenUpdateMove(CGScreenUpdateMoveDelta delta,
                        size_t count,
                        const CGRect *rect_array);
  void DisplaysReconfigured(CGDirectDisplayID display,
                            CGDisplayChangeSummaryFlags flags);
  static void ScreenRefreshCallback(CGRectCount count,
                                    const CGRect *rect_array,
                                    void *user_parameter);
  static void ScreenUpdateMoveCallback(CGScreenUpdateMoveDelta delta,
                                       size_t count,
                                       const CGRect *rect_array,
                                       void *user_parameter);
  static void DisplaysReconfiguredCallback(CGDirectDisplayID display,
                                           CGDisplayChangeSummaryFlags flags,
                                           void *user_parameter);

  void ReleaseBuffers();

  Callback* callback_;
  MouseShapeObserver* mouse_shape_observer_;

  CGLContextObj cgl_context_;
  ScopedPixelBufferObject pixel_buffer_object_;

  // Queue of the frames buffers.
  ScreenCaptureFrameQueue queue_;

  // Current display configuration.
  MacDesktopConfiguration desktop_config_;

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  ScreenCapturerHelper helper_;

  // Image of the last cursor that we sent to the client.
  base::mac::ScopedCFTypeRef<CGImageRef> current_cursor_;

  // Contains an invalid region from the previous capture.
  webrtc::DesktopRegion last_invalid_region_;

  // Used to ensure that frame captures do not take place while displays
  // are being reconfigured.
  base::WaitableEvent display_configuration_capture_event_;

  // Records the Ids of attached displays which are being reconfigured.
  // Accessed on the thread on which we are notified of display events.
  std::set<CGDirectDisplayID> reconfiguring_displays_;

  // Power management assertion to prevent the screen from sleeping.
  IOPMAssertionID power_assertion_id_display_;

  // Power management assertion to indicate that the user is active.
  IOPMAssertionID power_assertion_id_user_;

  // Dynamically link to deprecated APIs for Mac OS X 10.6 support.
  base::ScopedNativeLibrary app_services_library_;
  CGDisplayBaseAddressFunc cg_display_base_address_;
  CGDisplayBytesPerRowFunc cg_display_bytes_per_row_;
  CGDisplayBitsPerPixelFunc cg_display_bits_per_pixel_;
  base::ScopedNativeLibrary opengl_library_;
  CGLSetFullScreenFunc cgl_set_full_screen_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCapturerMac);
};

scoped_ptr<webrtc::DesktopFrame> CreateFrame(
    const MacDesktopConfiguration& desktop_config) {

  webrtc::DesktopSize size(desktop_config.pixel_bounds.width(),
                           desktop_config.pixel_bounds.height());
  scoped_ptr<webrtc::DesktopFrame> frame(new webrtc::BasicDesktopFrame(size));

  frame->set_dpi(webrtc::DesktopVector(
      kStandardDPI * desktop_config.dip_to_pixel_scale,
      kStandardDPI * desktop_config.dip_to_pixel_scale));
  return frame.Pass();
}

ScreenCapturerMac::ScreenCapturerMac()
    : callback_(NULL),
      mouse_shape_observer_(NULL),
      cgl_context_(NULL),
      display_configuration_capture_event_(false, true),
      power_assertion_id_display_(kIOPMNullAssertionID),
      power_assertion_id_user_(kIOPMNullAssertionID),
      cg_display_base_address_(NULL),
      cg_display_bytes_per_row_(NULL),
      cg_display_bits_per_pixel_(NULL),
      cgl_set_full_screen_(NULL) {
}

ScreenCapturerMac::~ScreenCapturerMac() {
  if (power_assertion_id_display_ != kIOPMNullAssertionID) {
    IOPMAssertionRelease(power_assertion_id_display_);
    power_assertion_id_display_ = kIOPMNullAssertionID;
  }
  if (power_assertion_id_user_ != kIOPMNullAssertionID) {
    IOPMAssertionRelease(power_assertion_id_user_);
    power_assertion_id_user_ = kIOPMNullAssertionID;
  }

  ReleaseBuffers();
  UnregisterRefreshAndMoveHandlers();
  CGError err = CGDisplayRemoveReconfigurationCallback(
      ScreenCapturerMac::DisplaysReconfiguredCallback, this);
  if (err != kCGErrorSuccess) {
    LOG(ERROR) << "CGDisplayRemoveReconfigurationCallback " << err;
  }
}

bool ScreenCapturerMac::Init() {
  if (!RegisterRefreshAndMoveHandlers()) {
    return false;
  }

  CGError err = CGDisplayRegisterReconfigurationCallback(
      ScreenCapturerMac::DisplaysReconfiguredCallback, this);
  if (err != kCGErrorSuccess) {
    LOG(ERROR) << "CGDisplayRegisterReconfigurationCallback " << err;
    return false;
  }

  ScreenConfigurationChanged();
  return true;
}

void ScreenCapturerMac::ReleaseBuffers() {
  if (cgl_context_) {
    pixel_buffer_object_.Release();
    CGLDestroyContext(cgl_context_);
    cgl_context_ = NULL;
  }
  // The buffers might be in use by the encoder, so don't delete them here.
  // Instead, mark them as "needs update"; next time the buffers are used by
  // the capturer, they will be recreated if necessary.
  queue_.Reset();
}

void ScreenCapturerMac::Start(Callback* callback) {
  DCHECK(!callback_);
  DCHECK(callback);

  callback_ = callback;

  // Create power management assertions to wake the display and prevent it from
  // going to sleep on user idle.
  // TODO(jamiewalch): Use IOPMAssertionDeclareUserActivity on 10.7.3 and above
  //                   instead of the following two assertions.
  IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep,
                              kIOPMAssertionLevelOn,
                              CFSTR("Chrome Remote Desktop connection active"),
                              &power_assertion_id_display_);
  // This assertion ensures that the display is woken up if it  already asleep
  // (as used by Apple Remote Desktop).
  IOPMAssertionCreateWithName(CFSTR("UserIsActive"),
                              kIOPMAssertionLevelOn,
                              CFSTR("Chrome Remote Desktop connection active"),
                              &power_assertion_id_user_);
}

void ScreenCapturerMac::Capture(
    const webrtc::DesktopRegion& region_to_capture) {
  base::Time capture_start_time = base::Time::Now();

  queue_.MoveToNextFrame();

  // Wait until the display configuration is stable. If one or more displays
  // are reconfiguring then |display_configuration_capture_event_| will not be
  // set until the reconfiguration completes.
  // TODO(wez): Replace this with an early-exit (See crbug.com/104542).
  CHECK(display_configuration_capture_event_.TimedWait(
      base::TimeDelta::FromSeconds(
          kDisplayConfigurationEventTimeoutInSeconds)));

  webrtc::DesktopRegion region;
  helper_.TakeInvalidRegion(&region);

  // If the current buffer is from an older generation then allocate a new one.
  // Note that we can't reallocate other buffers at this point, since the caller
  // may still be reading from them.
  if (!queue_.current_frame())
    queue_.ReplaceCurrentFrame(CreateFrame(desktop_config_));

  webrtc::DesktopFrame* current_frame = queue_.current_frame();

  bool flip = false;  // GL capturers need flipping.
  if (base::mac::IsOSLionOrLater()) {
    // Lion requires us to use their new APIs for doing screen capture. These
    // APIS currently crash on 10.6.8 if there is no monitor attached.
    CgBlitPostLion(*current_frame, region);
  } else if (cgl_context_) {
    flip = true;
    if (pixel_buffer_object_.get() != 0) {
      GlBlitFast(*current_frame, region);
    } else {
      // See comment in ScopedPixelBufferObject::Init about why the slow
      // path is always used on 10.5.
      GlBlitSlow(*current_frame);
    }
  } else {
    CgBlitPreLion(*current_frame, region);
  }

  uint8* buffer = current_frame->data();
  int stride = current_frame->stride();
  if (flip) {
    stride = -stride;
    buffer += (current_frame->size().height() - 1) * current_frame->stride();
  }

  webrtc::DesktopFrame* new_frame = queue_.current_frame()->Share();
  *new_frame->mutable_updated_region() = region;

  helper_.set_size_most_recent(new_frame->size());

  // Signal that we are done capturing data from the display framebuffer,
  // and accessing display structures.
  display_configuration_capture_event_.Signal();

  // Capture the current cursor shape and notify |callback_| if it has changed.
  CaptureCursor();

  new_frame->set_capture_time_ms(
      (base::Time::Now() - capture_start_time).InMillisecondsRoundedUp());
  callback_->OnCaptureCompleted(new_frame);
}

void ScreenCapturerMac::SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) {
  DCHECK(!mouse_shape_observer_);
  DCHECK(mouse_shape_observer);
  mouse_shape_observer_ = mouse_shape_observer;
}

void ScreenCapturerMac::CaptureCursor() {
  NSCursor* cursor = [NSCursor currentSystemCursor];
  if (cursor == nil) {
    return;
  }

  NSImage* nsimage = [cursor image];
  NSPoint hotspot = [cursor hotSpot];
  NSSize size = [nsimage size];
  CGImageRef image = [nsimage CGImageForProposedRect:NULL
                                             context:nil
                                               hints:nil];
  if (image == nil) {
    return;
  }

  if (CGImageGetBitsPerPixel(image) != 32 ||
      CGImageGetBytesPerRow(image) != (size.width * 4) ||
      CGImageGetBitsPerComponent(image) != 8) {
    return;
  }

  // Compare the current cursor with the last one we sent to the client
  // and exit if the cursor is the same.
  if (current_cursor_.get() != NULL) {
    CGImageRef current = current_cursor_.get();
    if (CGImageGetWidth(image) == CGImageGetWidth(current) &&
        CGImageGetHeight(image) == CGImageGetHeight(current) &&
        CGImageGetBitsPerPixel(image) == CGImageGetBitsPerPixel(current) &&
        CGImageGetBytesPerRow(image) == CGImageGetBytesPerRow(current) &&
        CGImageGetBitsPerComponent(image) ==
            CGImageGetBitsPerComponent(current)) {
      CGDataProviderRef provider_new = CGImageGetDataProvider(image);
      base::mac::ScopedCFTypeRef<CFDataRef> data_ref_new(
          CGDataProviderCopyData(provider_new));
      CGDataProviderRef provider_current = CGImageGetDataProvider(current);
      base::mac::ScopedCFTypeRef<CFDataRef> data_ref_current(
          CGDataProviderCopyData(provider_current));

      if (data_ref_new.get() != NULL && data_ref_current.get() != NULL) {
        int data_size = CFDataGetLength(data_ref_new);
        CHECK(data_size == CFDataGetLength(data_ref_current));
        const uint8* data_new = CFDataGetBytePtr(data_ref_new);
        const uint8* data_current = CFDataGetBytePtr(data_ref_current);
        if (memcmp(data_new, data_current, data_size) == 0) {
          return;
        }
      }
    }
  }

  // Record the last cursor image.
  current_cursor_.reset(CGImageCreateCopy(image));

  VLOG(3) << "Sending cursor: " << size.width << "x" << size.height;

  CGDataProviderRef provider = CGImageGetDataProvider(image);
  base::mac::ScopedCFTypeRef<CFDataRef> image_data_ref(
      CGDataProviderCopyData(provider));
  if (image_data_ref.get() == NULL) {
    return;
  }
  const char* cursor_src_data =
      reinterpret_cast<const char*>(CFDataGetBytePtr(image_data_ref));
  int data_size = CFDataGetLength(image_data_ref);

  // Create a MouseCursorShape that describes the cursor and pass it to
  // the client.
  scoped_ptr<MouseCursorShape> cursor_shape(new MouseCursorShape());
  cursor_shape->size.set(size.width, size.height);
  cursor_shape->hotspot.set(hotspot.x, hotspot.y);
  cursor_shape->data.assign(cursor_src_data, cursor_src_data + data_size);

  if (mouse_shape_observer_)
    mouse_shape_observer_->OnCursorShapeChanged(cursor_shape.Pass());
}

void ScreenCapturerMac::GlBlitFast(const webrtc::DesktopFrame& frame,
                                   const webrtc::DesktopRegion& region) {
  // Clip to the size of our current screen.
  webrtc::DesktopRect clip_rect = webrtc::DesktopRect::MakeSize(frame.size());
  if (queue_.previous_frame()) {
    // We are doing double buffer for the capture data so we just need to copy
    // the invalid region from the previous capture in the current buffer.
    // TODO(hclam): We can reduce the amount of copying here by subtracting
    // |capturer_helper_|s region from |last_invalid_region_|.
    // http://crbug.com/92354

    // Since the image obtained from OpenGL is upside-down, need to do some
    // magic here to copy the correct rectangle.
    const int y_offset = (frame.size().width() - 1) * frame.stride();
    for (webrtc::DesktopRegion::Iterator i(last_invalid_region_);
         !i.IsAtEnd(); i.Advance()) {
      webrtc::DesktopRect copy_rect = i.rect();
      copy_rect.IntersectWith(clip_rect);
      if (!copy_rect.is_empty()) {
        CopyRect(queue_.previous_frame()->data() + y_offset,
                 -frame.stride(),
                 frame.data() + y_offset,
                 -frame.stride(),
                 webrtc::DesktopFrame::kBytesPerPixel,
                 copy_rect);
      }
    }
  }
  last_invalid_region_ = region;

  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pixel_buffer_object_.get());
  glReadPixels(0, 0, frame.size().height(), frame.size().width(), GL_BGRA,
               GL_UNSIGNED_BYTE, 0);
  GLubyte* ptr = static_cast<GLubyte*>(
      glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB));
  if (ptr == NULL) {
    // If the buffer can't be mapped, assume that it's no longer valid and
    // release it.
    pixel_buffer_object_.Release();
  } else {
    // Copy only from the dirty rects. Since the image obtained from OpenGL is
    // upside-down we need to do some magic here to copy the correct rectangle.
    const int y_offset = (frame.size().height() - 1) * frame.stride();
    for (webrtc::DesktopRegion::Iterator i(region);
         !i.IsAtEnd(); i.Advance()) {
      webrtc::DesktopRect copy_rect = i.rect();
      copy_rect.IntersectWith(clip_rect);
      if (!copy_rect.is_empty()) {
        CopyRect(ptr + y_offset,
                 -frame.stride(),
                 frame.data() + y_offset,
                 -frame.stride(),
                 webrtc::DesktopFrame::kBytesPerPixel,
                 copy_rect);
      }
    }
  }
  if (!glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB)) {
    // If glUnmapBuffer returns false, then the contents of the data store are
    // undefined. This might be because the screen mode has changed, in which
    // case it will be recreated in ScreenConfigurationChanged, but releasing
    // the object here is the best option. Capturing will fall back on
    // GlBlitSlow until such time as the pixel buffer object is recreated.
    pixel_buffer_object_.Release();
  }
  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
}

void ScreenCapturerMac::GlBlitSlow(const webrtc::DesktopFrame& frame) {
  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glReadBuffer(GL_FRONT);
  glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
  glPixelStorei(GL_PACK_ALIGNMENT, 4);  // Force 4-byte alignment.
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glPixelStorei(GL_PACK_SKIP_ROWS, 0);
  glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
  // Read a block of pixels from the frame buffer.
  glReadPixels(0, 0, frame.size().width(), frame.size().height(),
               GL_BGRA, GL_UNSIGNED_BYTE, frame.data());
  glPopClientAttrib();
}

void ScreenCapturerMac::CgBlitPreLion(const webrtc::DesktopFrame& frame,
                                      const webrtc::DesktopRegion& region) {
  // Copy the entire contents of the previous capture buffer, to capture over.
  // TODO(wez): Get rid of this as per crbug.com/145064, or implement
  // crbug.com/92354.
  if (queue_.previous_frame()) {
    memcpy(frame.data(),
           queue_.previous_frame()->data(),
           frame.stride() * frame.size().height());
  }

  for (size_t i = 0; i < desktop_config_.displays.size(); ++i) {
    const MacDisplayConfiguration& display_config = desktop_config_.displays[i];

    // Use deprecated APIs to determine the display buffer layout.
    DCHECK(cg_display_base_address_ && cg_display_bytes_per_row_ &&
        cg_display_bits_per_pixel_);
    uint8* display_base_address =
      reinterpret_cast<uint8*>((*cg_display_base_address_)(display_config.id));
    CHECK(display_base_address);
    int src_bytes_per_row = (*cg_display_bytes_per_row_)(display_config.id);
    int src_bytes_per_pixel =
        (*cg_display_bits_per_pixel_)(display_config.id) / 8;

    // Determine the display's position relative to the desktop, in pixels.
    webrtc::DesktopRect display_bounds = display_config.pixel_bounds;
    display_bounds.Translate(-desktop_config_.pixel_bounds.left(),
                             -desktop_config_.pixel_bounds.top());

    // Determine which parts of the blit region, if any, lay within the monitor.
    webrtc::DesktopRegion copy_region = region;
    copy_region.IntersectWith(display_bounds);
    if (copy_region.is_empty())
      continue;

    // Translate the region to be copied into display-relative coordinates.
    copy_region.Translate(-display_bounds.left(), -display_bounds.top());

    // Calculate where in the output buffer the display's origin is.
    uint8* out_ptr = frame.data() +
         (display_bounds.left() * src_bytes_per_pixel) +
         (display_bounds.top() * frame.stride());

    // Copy the dirty region from the display buffer into our desktop buffer.
    for (webrtc::DesktopRegion::Iterator i(copy_region);
         !i.IsAtEnd(); i.Advance()) {
      CopyRect(display_base_address,
               src_bytes_per_row,
               out_ptr,
               frame.stride(),
               src_bytes_per_pixel,
               i.rect());
    }
  }
}

void ScreenCapturerMac::CgBlitPostLion(const webrtc::DesktopFrame& frame,
                                       const webrtc::DesktopRegion& region) {
  // Copy the entire contents of the previous capture buffer, to capture over.
  // TODO(wez): Get rid of this as per crbug.com/145064, or implement
  // crbug.com/92354.
  if (queue_.previous_frame()) {
    memcpy(frame.data(),
           queue_.previous_frame()->data(),
           frame.stride() * frame.size().height());
  }

  for (size_t i = 0; i < desktop_config_.displays.size(); ++i) {
    const MacDisplayConfiguration& display_config = desktop_config_.displays[i];

    // Determine the display's position relative to the desktop, in pixels.
    webrtc::DesktopRect display_bounds = display_config.pixel_bounds;
    display_bounds.Translate(-desktop_config_.pixel_bounds.left(),
                             -desktop_config_.pixel_bounds.top());

    // Determine which parts of the blit region, if any, lay within the monitor.
    webrtc::DesktopRegion copy_region = region;
    copy_region.IntersectWith(display_bounds);
    if (copy_region.is_empty())
      continue;

    // Translate the region to be copied into display-relative coordinates.
    copy_region.Translate(-display_bounds.left(), -display_bounds.top());

    // Create an image containing a snapshot of the display.
    base::mac::ScopedCFTypeRef<CGImageRef> image(
        CGDisplayCreateImage(display_config.id));
    if (image.get() == NULL)
      continue;

    // Request access to the raw pixel data via the image's DataProvider.
    CGDataProviderRef provider = CGImageGetDataProvider(image);
    base::mac::ScopedCFTypeRef<CFDataRef> data(
        CGDataProviderCopyData(provider));
    if (data.get() == NULL)
      continue;

    const uint8* display_base_address = CFDataGetBytePtr(data);
    int src_bytes_per_row = CGImageGetBytesPerRow(image);
    int src_bytes_per_pixel = CGImageGetBitsPerPixel(image) / 8;

    // Calculate where in the output buffer the display's origin is.
    uint8* out_ptr = frame.data() +
        (display_bounds.left() * src_bytes_per_pixel) +
        (display_bounds.top() * frame.stride());

    // Copy the dirty region from the display buffer into our desktop buffer.
    for (webrtc::DesktopRegion::Iterator i(copy_region);
         !i.IsAtEnd(); i.Advance()) {
      CopyRect(display_base_address,
               src_bytes_per_row,
               out_ptr,
               frame.stride(),
               src_bytes_per_pixel,
               i.rect());
    }
  }
}

void ScreenCapturerMac::ScreenConfigurationChanged() {
  // Release existing buffers, which will be of the wrong size.
  ReleaseBuffers();

  // Clear the dirty region, in case the display is down-sizing.
  helper_.ClearInvalidRegion();

  // Refresh the cached desktop configuration.
  desktop_config_ = MacDesktopConfiguration::GetCurrent(
      MacDesktopConfiguration::TopLeftOrigin);

  // Re-mark the entire desktop as dirty.
  helper_.InvalidateScreen(
      webrtc::DesktopSize(desktop_config_.pixel_bounds.width(),
                          desktop_config_.pixel_bounds.height()));

  // Make sure the frame buffers will be reallocated.
  queue_.Reset();

  // CgBlitPostLion uses CGDisplayCreateImage() to snapshot each display's
  // contents. Although the API exists in OS 10.6, it crashes the caller if
  // the machine has no monitor connected, so we fall back to depcreated APIs
  // when running on 10.6.
  if (base::mac::IsOSLionOrLater()) {
    LOG(INFO) << "Using CgBlitPostLion.";
    // No need for any OpenGL support on Lion
    return;
  }

  // Dynamically link to the deprecated pre-Lion capture APIs.
  std::string app_services_library_error;
  base::FilePath app_services_path(kApplicationServicesLibraryName);
  app_services_library_.Reset(
      base::LoadNativeLibrary(app_services_path, &app_services_library_error));
  CHECK(app_services_library_.is_valid()) << app_services_library_error;

  std::string opengl_library_error;
  base::FilePath opengl_path(kOpenGlLibraryName);
  opengl_library_.Reset(
      base::LoadNativeLibrary(opengl_path, &opengl_library_error));
  CHECK(opengl_library_.is_valid()) << opengl_library_error;

  cg_display_base_address_ = reinterpret_cast<CGDisplayBaseAddressFunc>(
      app_services_library_.GetFunctionPointer("CGDisplayBaseAddress"));
  cg_display_bytes_per_row_ = reinterpret_cast<CGDisplayBytesPerRowFunc>(
      app_services_library_.GetFunctionPointer("CGDisplayBytesPerRow"));
  cg_display_bits_per_pixel_ = reinterpret_cast<CGDisplayBitsPerPixelFunc>(
      app_services_library_.GetFunctionPointer("CGDisplayBitsPerPixel"));
  cgl_set_full_screen_ = reinterpret_cast<CGLSetFullScreenFunc>(
      opengl_library_.GetFunctionPointer("CGLSetFullScreen"));
  CHECK(cg_display_base_address_ && cg_display_bytes_per_row_ &&
        cg_display_bits_per_pixel_ && cgl_set_full_screen_);

  if (desktop_config_.displays.size() > 1) {
    LOG(INFO) << "Using CgBlitPreLion (Multi-monitor).";
    return;
  }

  CGDirectDisplayID mainDevice = CGMainDisplayID();
  if (!CGDisplayUsesOpenGLAcceleration(mainDevice)) {
    LOG(INFO) << "Using CgBlitPreLion (OpenGL unavailable).";
    return;
  }

  LOG(INFO) << "Using GlBlit";

  CGLPixelFormatAttribute attributes[] = {
    kCGLPFAFullScreen,
    kCGLPFADisplayMask,
    (CGLPixelFormatAttribute)CGDisplayIDToOpenGLDisplayMask(mainDevice),
    (CGLPixelFormatAttribute)0
  };
  CGLPixelFormatObj pixel_format = NULL;
  GLint matching_pixel_format_count = 0;
  CGLError err = CGLChoosePixelFormat(attributes,
                                      &pixel_format,
                                      &matching_pixel_format_count);
  DCHECK_EQ(err, kCGLNoError);
  err = CGLCreateContext(pixel_format, NULL, &cgl_context_);
  DCHECK_EQ(err, kCGLNoError);
  CGLDestroyPixelFormat(pixel_format);
  (*cgl_set_full_screen_)(cgl_context_);
  CGLSetCurrentContext(cgl_context_);

  size_t buffer_size = desktop_config_.pixel_bounds.width() *
                       desktop_config_.pixel_bounds.height() *
                       sizeof(uint32_t);
  pixel_buffer_object_.Init(cgl_context_, buffer_size);
}

bool ScreenCapturerMac::RegisterRefreshAndMoveHandlers() {
  CGError err = CGRegisterScreenRefreshCallback(
      ScreenCapturerMac::ScreenRefreshCallback, this);
  if (err != kCGErrorSuccess) {
    LOG(ERROR) << "CGRegisterScreenRefreshCallback " << err;
    return false;
  }

  err = CGScreenRegisterMoveCallback(
      ScreenCapturerMac::ScreenUpdateMoveCallback, this);
  if (err != kCGErrorSuccess) {
    LOG(ERROR) << "CGScreenRegisterMoveCallback " << err;
    return false;
  }

  return true;
}

void ScreenCapturerMac::UnregisterRefreshAndMoveHandlers() {
  CGUnregisterScreenRefreshCallback(
      ScreenCapturerMac::ScreenRefreshCallback, this);
  CGScreenUnregisterMoveCallback(
      ScreenCapturerMac::ScreenUpdateMoveCallback, this);
}

void ScreenCapturerMac::ScreenRefresh(CGRectCount count,
                                      const CGRect* rect_array) {
  if (desktop_config_.pixel_bounds.is_empty())
    return;

  webrtc::DesktopRegion region;

  for (CGRectCount i = 0; i < count; ++i) {
    // Convert from Density-Independent Pixel to physical pixel coordinates.
    webrtc::DesktopRect rect =
      ScaleAndRoundCGRect(rect_array[i], desktop_config_.dip_to_pixel_scale);

    // Translate from local desktop to capturer framebuffer coordinates.
    rect.Translate(-desktop_config_.pixel_bounds.left(),
                   -desktop_config_.pixel_bounds.top());

    region.AddRect(rect);
  }

  helper_.InvalidateRegion(region);
}

void ScreenCapturerMac::ScreenUpdateMove(CGScreenUpdateMoveDelta delta,
                                             size_t count,
                                             const CGRect* rect_array) {
  // Translate |rect_array| to identify the move's destination.
  CGRect refresh_rects[count];
  for (CGRectCount i = 0; i < count; ++i) {
    refresh_rects[i] = CGRectOffset(rect_array[i], delta.dX, delta.dY);
  }

  // Currently we just treat move events the same as refreshes.
  ScreenRefresh(count, refresh_rects);
}

void ScreenCapturerMac::DisplaysReconfigured(
    CGDirectDisplayID display,
    CGDisplayChangeSummaryFlags flags) {
  if (flags & kCGDisplayBeginConfigurationFlag) {
    if (reconfiguring_displays_.empty()) {
      // If this is the first display to start reconfiguring then wait on
      // |display_configuration_capture_event_| to block the capture thread
      // from accessing display memory until the reconfiguration completes.
      CHECK(display_configuration_capture_event_.TimedWait(
                base::TimeDelta::FromSeconds(
                    kDisplayConfigurationEventTimeoutInSeconds)));
    }

    reconfiguring_displays_.insert(display);
  } else {
    reconfiguring_displays_.erase(display);

    if (reconfiguring_displays_.empty()) {
      // If no other displays are reconfiguring then refresh capturer data
      // structures and un-block the capturer thread. Occasionally, the
      // refresh and move handlers are lost when the screen mode changes,
      // so re-register them here (the same does not appear to be true for
      // the reconfiguration handler itself).
      UnregisterRefreshAndMoveHandlers();
      RegisterRefreshAndMoveHandlers();
      ScreenConfigurationChanged();
      display_configuration_capture_event_.Signal();
    }
  }
}

void ScreenCapturerMac::ScreenRefreshCallback(CGRectCount count,
                                              const CGRect* rect_array,
                                              void* user_parameter) {
  ScreenCapturerMac* capturer = reinterpret_cast<ScreenCapturerMac*>(
      user_parameter);
  if (capturer->desktop_config_.pixel_bounds.is_empty()) {
    capturer->ScreenConfigurationChanged();
  }
  capturer->ScreenRefresh(count, rect_array);
}

void ScreenCapturerMac::ScreenUpdateMoveCallback(
    CGScreenUpdateMoveDelta delta,
    size_t count,
    const CGRect* rect_array,
    void* user_parameter) {
  ScreenCapturerMac* capturer = reinterpret_cast<ScreenCapturerMac*>(
      user_parameter);
  capturer->ScreenUpdateMove(delta, count, rect_array);
}

void ScreenCapturerMac::DisplaysReconfiguredCallback(
    CGDirectDisplayID display,
    CGDisplayChangeSummaryFlags flags,
    void* user_parameter) {
  ScreenCapturerMac* capturer = reinterpret_cast<ScreenCapturerMac*>(
      user_parameter);
  capturer->DisplaysReconfigured(display, flags);
}

}  // namespace

// static
scoped_ptr<ScreenCapturer> ScreenCapturer::Create() {
  scoped_ptr<ScreenCapturerMac> capturer(new ScreenCapturerMac());
  if (!capturer->Init())
    capturer.reset();
  return capturer.PassAs<ScreenCapturer>();
}

}  // namespace media
