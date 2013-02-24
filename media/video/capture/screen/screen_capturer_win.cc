// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capturer.h"

#include <windows.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "media/video/capture/screen/differ.h"
#include "media/video/capture/screen/mouse_cursor_shape.h"
#include "media/video/capture/screen/screen_capture_data.h"
#include "media/video/capture/screen/screen_capture_frame.h"
#include "media/video/capture/screen/screen_capture_frame_queue.h"
#include "media/video/capture/screen/screen_capturer_helper.h"
#include "media/video/capture/screen/win/desktop.h"
#include "media/video/capture/screen/win/scoped_thread_desktop.h"
#include "third_party/skia/include/core/SkColorPriv.h"

namespace media {

namespace {

// Constants from dwmapi.h.
const UINT DWM_EC_DISABLECOMPOSITION = 0;
const UINT DWM_EC_ENABLECOMPOSITION = 1;

typedef HRESULT (WINAPI * DwmEnableCompositionFunc)(UINT);

const char kDwmapiLibraryName[] = "dwmapi";

// Pixel colors used when generating cursor outlines.
const uint32 kPixelBgraBlack = 0xff000000;
const uint32 kPixelBgraWhite = 0xffffffff;
const uint32 kPixelBgraTransparent = 0x00000000;

// A class representing a full-frame pixel buffer.
class ScreenCaptureFrameWin : public ScreenCaptureFrame {
 public:
  ScreenCaptureFrameWin(HDC desktop_dc, const SkISize& size,
                        ScreenCapturer::Delegate* delegate);
  virtual ~ScreenCaptureFrameWin();

  // Returns handle of the device independent bitmap representing this frame
  // buffer to GDI.
  HBITMAP GetBitmap();

 private:
  // Allocates a device independent bitmap representing this frame buffer to
  // GDI.
  void AllocateBitmap(HDC desktop_dc, const SkISize& size);

  // Handle of the device independent bitmap representing this frame buffer to
  // GDI.
  base::win::ScopedBitmap bitmap_;

  // Used to work with shared memory buffers.
  ScreenCapturer::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureFrameWin);
};

// ScreenCapturerWin captures 32bit RGB using GDI.
//
// ScreenCapturerWin is double-buffered as required by ScreenCapturer.
class ScreenCapturerWin : public ScreenCapturer {
 public:
  ScreenCapturerWin();
  virtual ~ScreenCapturerWin();

  // Overridden from ScreenCapturer:
  virtual void Start(Delegate* delegate) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void InvalidateRegion(const SkRegion& invalid_region) OVERRIDE;
  virtual void CaptureFrame() OVERRIDE;

 private:
  // Make sure that the device contexts match the screen configuration.
  void PrepareCaptureResources();

  // Creates a ScreenCaptureData instance wrapping the current framebuffer and
  // notifies |delegate_|.
  void CaptureRegion(const SkRegion& region,
                     const base::Time& capture_start_time);

  // Captures the current screen contents into the current buffer.
  void CaptureImage();

  // Expand the cursor shape to add a white outline for visibility against
  // dark backgrounds.
  void AddCursorOutline(int width, int height, uint32* dst);

  // Capture the current cursor shape.
  void CaptureCursor();

  Delegate* delegate_;

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  ScreenCapturerHelper helper_;

  // Snapshot of the last cursor bitmap we sent to the client. This is used
  // to diff against the current cursor so we only send a cursor-change
  // message when the shape has changed.
  MouseCursorShape last_cursor_;

  ScopedThreadDesktop desktop_;

  // GDI resources used for screen capture.
  scoped_ptr<base::win::ScopedGetDC> desktop_dc_;
  base::win::ScopedCreateDC memory_dc_;

  // Queue of the frames buffers.
  ScreenCaptureFrameQueue queue_;

  // Rectangle describing the bounds of the desktop device context.
  SkIRect desktop_dc_rect_;

  // Class to calculate the difference between two screen bitmaps.
  scoped_ptr<Differ> differ_;

  base::ScopedNativeLibrary dwmapi_library_;
  DwmEnableCompositionFunc composition_func_;

  // Used to suppress duplicate logging of SetThreadExecutionState errors.
  bool set_thread_execution_state_failed_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCapturerWin);
};

// 3780 pixels per meter is equivalent to 96 DPI, typical on desktop monitors.
static const int kPixelsPerMeter = 3780;

ScreenCaptureFrameWin::ScreenCaptureFrameWin(
    HDC desktop_dc,
    const SkISize& size,
    ScreenCapturer::Delegate* delegate)
    : delegate_(delegate) {
  // Try to allocate a shared memory buffer.
  uint32 buffer_size =
    size.width() * size.height() * ScreenCaptureData::kBytesPerPixel;
  scoped_refptr<SharedBuffer> shared_buffer =
      delegate_->CreateSharedBuffer(buffer_size);
  if (shared_buffer) {
    CHECK(shared_buffer->ptr() != NULL);
    set_shared_buffer(shared_buffer);
  }

  AllocateBitmap(desktop_dc, size);
}

ScreenCaptureFrameWin::~ScreenCaptureFrameWin() {
  if (shared_buffer())
    delegate_->ReleaseSharedBuffer(shared_buffer());
}

HBITMAP ScreenCaptureFrameWin::GetBitmap() {
  return bitmap_;
}

void ScreenCaptureFrameWin::AllocateBitmap(HDC desktop_dc,
                                           const SkISize& size) {
  int bytes_per_row = size.width() * ScreenCaptureData::kBytesPerPixel;

  // Describe a device independent bitmap (DIB) that is the size of the desktop.
  BITMAPINFO bmi;
  memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biHeight = -size.height();
  bmi.bmiHeader.biWidth = size.width();
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = ScreenCaptureData::kBytesPerPixel * 8;
  bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
  bmi.bmiHeader.biSizeImage = bytes_per_row * size.height();
  bmi.bmiHeader.biXPelsPerMeter = kPixelsPerMeter;
  bmi.bmiHeader.biYPelsPerMeter = kPixelsPerMeter;

  // Create the DIB, and store a pointer to its pixel buffer.
  HANDLE section_handle = NULL;
  if (shared_buffer())
    section_handle = shared_buffer()->handle();
  void* data = NULL;
  bitmap_ = CreateDIBSection(desktop_dc, &bmi, DIB_RGB_COLORS, &data,
                             section_handle, 0);

  // TODO(wez): Cope gracefully with failure (crbug.com/157170).
  CHECK(bitmap_ != NULL);
  CHECK(data != NULL);

  set_pixels(reinterpret_cast<uint8*>(data));
  set_dimensions(SkISize::Make(bmi.bmiHeader.biWidth,
                               std::abs(bmi.bmiHeader.biHeight)));
  set_bytes_per_row(
      bmi.bmiHeader.biSizeImage / std::abs(bmi.bmiHeader.biHeight));
}

ScreenCapturerWin::ScreenCapturerWin()
    : delegate_(NULL),
      desktop_dc_rect_(SkIRect::MakeEmpty()),
      composition_func_(NULL),
      set_thread_execution_state_failed_(false) {
}

ScreenCapturerWin::~ScreenCapturerWin() {
}

void ScreenCapturerWin::InvalidateRegion(const SkRegion& invalid_region) {
  helper_.InvalidateRegion(invalid_region);
}

void ScreenCapturerWin::CaptureFrame() {
  base::Time capture_start_time = base::Time::Now();

  // Request that the system not power-down the system, or the display hardware.
  if (!SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED)) {
    if (!set_thread_execution_state_failed_) {
      set_thread_execution_state_failed_ = true;
      LOG_GETLASTERROR(WARNING)
          << "Failed to make system & display power assertion";
    }
  }

  // Make sure the GDI capture resources are up-to-date.
  PrepareCaptureResources();

  // Copy screen bits to the current buffer.
  CaptureImage();

  const ScreenCaptureFrame* current_buffer = queue_.current_frame();
  const ScreenCaptureFrame* last_buffer = queue_.previous_frame();
  if (last_buffer) {
    // Make sure the differencer is set up correctly for these previous and
    // current screens.
    if (!differ_.get() ||
        (differ_->width() != current_buffer->dimensions().width()) ||
        (differ_->height() != current_buffer->dimensions().height()) ||
        (differ_->bytes_per_row() != current_buffer->bytes_per_row())) {
      differ_.reset(new Differ(current_buffer->dimensions().width(),
                               current_buffer->dimensions().height(),
                               ScreenCaptureData::kBytesPerPixel,
                               current_buffer->bytes_per_row()));
    }

    // Calculate difference between the two last captured frames.
    SkRegion region;
    differ_->CalcDirtyRegion(last_buffer->pixels(), current_buffer->pixels(),
                             &region);
    InvalidateRegion(region);
  } else {
    // No previous frame is available. Invalidate the whole screen.
    helper_.InvalidateScreen(current_buffer->dimensions());
  }

  // Wrap the captured frame into ScreenCaptureData structure and invoke
  // the completion callback.
  SkRegion invalid_region;
  helper_.SwapInvalidRegion(&invalid_region);
  CaptureRegion(invalid_region, capture_start_time);

  // Check for cursor shape update.
  CaptureCursor();
}

void ScreenCapturerWin::Start(Delegate* delegate) {
  DCHECK(delegate_ == NULL);

  delegate_ = delegate;

  // Load dwmapi.dll dynamically since it is not available on XP.
  if (!dwmapi_library_.is_valid()) {
    base::FilePath path(base::GetNativeLibraryName(
        UTF8ToUTF16(kDwmapiLibraryName)));
    dwmapi_library_.Reset(base::LoadNativeLibrary(path, NULL));
  }

  if (dwmapi_library_.is_valid() && composition_func_ == NULL) {
    composition_func_ = static_cast<DwmEnableCompositionFunc>(
        dwmapi_library_.GetFunctionPointer("DwmEnableComposition"));
  }

  // Vote to disable Aero composited desktop effects while capturing. Windows
  // will restore Aero automatically if the process exits. This has no effect
  // under Windows 8 or higher.  See crbug.com/124018.
  if (composition_func_ != NULL) {
    (*composition_func_)(DWM_EC_DISABLECOMPOSITION);
  }
}

void ScreenCapturerWin::Stop() {
  // Restore Aero.
  if (composition_func_ != NULL) {
    (*composition_func_)(DWM_EC_ENABLECOMPOSITION);
  }

  delegate_ = NULL;
}

void ScreenCapturerWin::PrepareCaptureResources() {
  // Switch to the desktop receiving user input if different from the current
  // one.
  scoped_ptr<Desktop> input_desktop = Desktop::GetInputDesktop();
  if (input_desktop.get() != NULL && !desktop_.IsSame(*input_desktop)) {
    // Release GDI resources otherwise SetThreadDesktop will fail.
    desktop_dc_.reset();
    memory_dc_.Set(NULL);

    // If SetThreadDesktop() fails, the thread is still assigned a desktop.
    // So we can continue capture screen bits, just from the wrong desktop.
    desktop_.SetThreadDesktop(input_desktop.Pass());

    // Re-assert our vote to disable Aero.
    // See crbug.com/124018 and crbug.com/129906.
    if (composition_func_ != NULL) {
      (*composition_func_)(DWM_EC_DISABLECOMPOSITION);
    }
  }

  // If the display bounds have changed then recreate GDI resources.
  // TODO(wez): Also check for pixel format changes.
  SkIRect screen_rect(SkIRect::MakeXYWH(
      GetSystemMetrics(SM_XVIRTUALSCREEN),
      GetSystemMetrics(SM_YVIRTUALSCREEN),
      GetSystemMetrics(SM_CXVIRTUALSCREEN),
      GetSystemMetrics(SM_CYVIRTUALSCREEN)));
  if (screen_rect != desktop_dc_rect_) {
    desktop_dc_.reset();
    memory_dc_.Set(NULL);
    desktop_dc_rect_.setEmpty();
  }

  if (desktop_dc_.get() == NULL) {
    DCHECK(memory_dc_.Get() == NULL);

    // Create GDI device contexts to capture from the desktop into memory.
    desktop_dc_.reset(new base::win::ScopedGetDC(NULL));
    memory_dc_.Set(CreateCompatibleDC(*desktop_dc_));
    desktop_dc_rect_ = screen_rect;

    // Make sure the frame buffers will be reallocated.
    queue_.SetAllFramesNeedUpdate();

    helper_.ClearInvalidRegion();
  }
}

void ScreenCapturerWin::CaptureRegion(
    const SkRegion& region,
    const base::Time& capture_start_time) {
  const ScreenCaptureFrame* current_buffer = queue_.current_frame();

  scoped_refptr<ScreenCaptureData> data(new ScreenCaptureData(
      current_buffer->pixels(), current_buffer->bytes_per_row(),
      current_buffer->dimensions()));
  data->mutable_dirty_region() = region;
  data->set_shared_buffer(current_buffer->shared_buffer());

  helper_.set_size_most_recent(data->size());

  queue_.DoneWithCurrentFrame();

  data->set_capture_time_ms(
      (base::Time::Now() - capture_start_time).InMillisecondsRoundedUp());
  delegate_->OnCaptureCompleted(data);
}

void ScreenCapturerWin::CaptureImage() {
  // If the current buffer is from an older generation then allocate a new one.
  // Note that we can't reallocate other buffers at this point, since the caller
  // may still be reading from them.
  if (queue_.current_frame_needs_update()) {
    DCHECK(desktop_dc_.get() != NULL);
    DCHECK(memory_dc_.Get() != NULL);

    SkISize size = SkISize::Make(desktop_dc_rect_.width(),
                                 desktop_dc_rect_.height());
    scoped_ptr<ScreenCaptureFrameWin> buffer(
        new ScreenCaptureFrameWin(*desktop_dc_, size, delegate_));
    queue_.ReplaceCurrentFrame(buffer.PassAs<ScreenCaptureFrame>());
  }

  // Select the target bitmap into the memory dc and copy the rect from desktop
  // to memory.
  ScreenCaptureFrameWin* current = static_cast<ScreenCaptureFrameWin*>(
      queue_.current_frame());
  HGDIOBJ previous_object = SelectObject(memory_dc_, current->GetBitmap());
  if (previous_object != NULL) {
    BitBlt(memory_dc_,
           0, 0, desktop_dc_rect_.width(), desktop_dc_rect_.height(),
           *desktop_dc_,
           desktop_dc_rect_.x(), desktop_dc_rect_.y(),
           SRCCOPY | CAPTUREBLT);

    // Select back the previously selected object to that the device contect
    // could be destroyed independently of the bitmap if needed.
    SelectObject(memory_dc_, previous_object);
  }
}

void ScreenCapturerWin::AddCursorOutline(int width,
                                         int height,
                                         uint32* dst) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      // If this is a transparent pixel (bgr == 0 and alpha = 0), check the
      // neighbor pixels to see if this should be changed to an outline pixel.
      if (*dst == kPixelBgraTransparent) {
        // Change to white pixel if any neighbors (top, bottom, left, right)
        // are black.
        if ((y > 0 && dst[-width] == kPixelBgraBlack) ||
            (y < height - 1 && dst[width] == kPixelBgraBlack) ||
            (x > 0 && dst[-1] == kPixelBgraBlack) ||
            (x < width - 1 && dst[1] == kPixelBgraBlack)) {
          *dst = kPixelBgraWhite;
        }
      }
      dst++;
    }
  }
}

void ScreenCapturerWin::CaptureCursor() {
  CURSORINFO cursor_info;
  cursor_info.cbSize = sizeof(CURSORINFO);
  if (!GetCursorInfo(&cursor_info)) {
    VLOG(3) << "Unable to get cursor info. Error = " << GetLastError();
    return;
  }

  // Note that this does not need to be freed.
  HCURSOR hcursor = cursor_info.hCursor;
  ICONINFO iinfo;
  if (!GetIconInfo(hcursor, &iinfo)) {
    VLOG(3) << "Unable to get cursor icon info. Error = " << GetLastError();
    return;
  }
  int hotspot_x = iinfo.xHotspot;
  int hotspot_y = iinfo.yHotspot;

  // Get the cursor bitmap.
  base::win::ScopedBitmap hbitmap;
  BITMAP bitmap;
  bool color_bitmap;
  if (iinfo.hbmColor) {
    // Color cursor bitmap.
    color_bitmap = true;
    hbitmap.Set((HBITMAP)CopyImage(iinfo.hbmColor, IMAGE_BITMAP, 0, 0,
                                   LR_CREATEDIBSECTION));
    if (!hbitmap.Get()) {
      VLOG(3) << "Unable to copy color cursor image. Error = "
              << GetLastError();
      return;
    }

    // Free the color and mask bitmaps since we only need our copy.
    DeleteObject(iinfo.hbmColor);
    DeleteObject(iinfo.hbmMask);
  } else {
    // Black and white (xor) cursor.
    color_bitmap = false;
    hbitmap.Set(iinfo.hbmMask);
  }

  if (!GetObject(hbitmap.Get(), sizeof(BITMAP), &bitmap)) {
    VLOG(3) << "Unable to get cursor bitmap. Error = " << GetLastError();
    return;
  }

  int width = bitmap.bmWidth;
  int height = bitmap.bmHeight;
  // For non-color cursors, the mask contains both an AND and an XOR mask and
  // the height includes both. Thus, the width is correct, but we need to
  // divide by 2 to get the correct mask height.
  if (!color_bitmap) {
    height /= 2;
  }
  int data_size = height * width * ScreenCaptureData::kBytesPerPixel;

  scoped_ptr<MouseCursorShape> cursor(new MouseCursorShape());
  cursor->data.resize(data_size);
  uint8* cursor_dst_data =
      reinterpret_cast<uint8*>(string_as_array(&cursor->data));

  // Copy/convert cursor bitmap into format needed by chromotocol.
  int row_bytes = bitmap.bmWidthBytes;
  if (color_bitmap) {
    if (bitmap.bmPlanes != 1 || bitmap.bmBitsPixel != 32) {
      VLOG(3) << "Unsupported color cursor format. Error = " << GetLastError();
      return;
    }

    // Copy across colour cursor imagery.
    // MouseCursorShape stores imagery top-down, and premultiplied
    // by the alpha channel, whereas windows stores them bottom-up
    // and not premultiplied.
    uint8* cursor_src_data = reinterpret_cast<uint8*>(bitmap.bmBits);
    uint8* src = cursor_src_data + ((height - 1) * row_bytes);
    uint8* dst = cursor_dst_data;
    for (int row = 0; row < height; ++row) {
      for (int column = 0; column < width; ++column) {
        dst[0] = SkAlphaMul(src[0], src[3]);
        dst[1] = SkAlphaMul(src[1], src[3]);
        dst[2] = SkAlphaMul(src[2], src[3]);
        dst[3] = src[3];
        dst += ScreenCaptureData::kBytesPerPixel;
        src += ScreenCaptureData::kBytesPerPixel;
      }
      src -= row_bytes + (width * ScreenCaptureData::kBytesPerPixel);
    }
  } else {
    if (bitmap.bmPlanes != 1 || bitmap.bmBitsPixel != 1) {
      VLOG(3) << "Unsupported cursor mask format. Error = " << GetLastError();
      return;
    }

    // x2 because there are 2 masks in the bitmap: AND and XOR.
    int mask_bytes = height * row_bytes * 2;
    scoped_array<uint8> mask(new uint8[mask_bytes]);
    if (!GetBitmapBits(hbitmap.Get(), mask_bytes, mask.get())) {
      VLOG(3) << "Unable to get cursor mask bits. Error = " << GetLastError();
      return;
    }
    uint8* and_mask = mask.get();
    uint8* xor_mask = mask.get() + height * row_bytes;
    uint8* dst = cursor_dst_data;
    bool add_outline = false;
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        int byte = y * row_bytes + x / 8;
        int bit = 7 - x % 8;
        int and = and_mask[byte] & (1 << bit);
        int xor = xor_mask[byte] & (1 << bit);

        // The two cursor masks combine as follows:
        //  AND  XOR   Windows Result  Our result   RGB  Alpha
        //   0    0    Black           Black         00    ff
        //   0    1    White           White         ff    ff
        //   1    0    Screen          Transparent   00    00
        //   1    1    Reverse-screen  Black         00    ff
        // Since we don't support XOR cursors, we replace the "Reverse Screen"
        // with black. In this case, we also add an outline around the cursor
        // so that it is visible against a dark background.
        int rgb = (!and && xor) ? 0xff : 0x00;
        int alpha = (and && !xor) ? 0x00 : 0xff;
        *dst++ = rgb;
        *dst++ = rgb;
        *dst++ = rgb;
        *dst++ = alpha;
        if (and && xor) {
          add_outline = true;
        }
      }
    }
    if (add_outline) {
      AddCursorOutline(width, height,
                       reinterpret_cast<uint32*>(cursor_dst_data));
    }
  }

  cursor->size.set(width, height);
  cursor->hotspot.set(hotspot_x, hotspot_y);

  // Compare the current cursor with the last one we sent to the client. If
  // they're the same, then don't bother sending the cursor again.
  if (last_cursor_.size == cursor->size &&
      last_cursor_.hotspot == cursor->hotspot &&
      last_cursor_.data == cursor->data) {
    return;
  }

  VLOG(3) << "Sending updated cursor: " << width << "x" << height;

  // Record the last cursor image that we sent to the client.
  last_cursor_ = *cursor;

  delegate_->OnCursorShapeChanged(cursor.Pass());
}

}  // namespace

scoped_refptr<SharedBuffer> ScreenCapturer::Delegate::CreateSharedBuffer(
    uint32 size) {
  return scoped_refptr<SharedBuffer>();
}

void ScreenCapturer::Delegate::ReleaseSharedBuffer(
    scoped_refptr<SharedBuffer> buffer) {
}

// static
scoped_ptr<ScreenCapturer> ScreenCapturer::Create() {
  return scoped_ptr<ScreenCapturer>(new ScreenCapturerWin());
}

}  // namespace media
