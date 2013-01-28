// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_CAPTURE_DATA_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_CAPTURE_DATA_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"
#include "media/video/capture/screen/shared_buffer.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace media {

class SharedBuffer;

// Stores the data and information of a capture to pass off to the
// encoding thread.
class MEDIA_EXPORT ScreenCaptureData
    : public base::RefCountedThreadSafe<ScreenCaptureData> {
 public:
  // 32 bit RGB is 4 bytes per pixel.
  static const int kBytesPerPixel = 4;

  ScreenCaptureData(uint8* data, int stride, const SkISize& size);

  // Data buffer.
  uint8* data() const { return data_; }

  // Distance in bytes between neighboring lines in the data buffer.
  int stride() const { return stride_; }

  // Gets the dirty region from the previous capture.
  const SkRegion& dirty_region() const { return dirty_region_; }

  // Returns the size of the image captured.
  SkISize size() const { return size_; }

  SkRegion& mutable_dirty_region() { return dirty_region_; }

  // Returns the time spent on capturing.
  int capture_time_ms() const { return capture_time_ms_; }

  // Sets the time spent on capturing.
  void set_capture_time_ms(int capture_time_ms) {
    capture_time_ms_ = capture_time_ms;
  }

  int64 client_sequence_number() const { return client_sequence_number_; }

  void set_client_sequence_number(int64 client_sequence_number) {
    client_sequence_number_ = client_sequence_number;
  }

  SkIPoint dpi() const { return dpi_; }

  void set_dpi(const SkIPoint& dpi) { dpi_ = dpi; }

  // Returns the shared memory buffer pointed to by |data|.
  scoped_refptr<SharedBuffer> shared_buffer() const { return shared_buffer_; }

  // Sets the shared memory buffer pointed to by |data|.
  void set_shared_buffer(scoped_refptr<SharedBuffer> shared_buffer) {
    shared_buffer_ = shared_buffer;
  }

 private:
  friend class base::RefCountedThreadSafe<ScreenCaptureData>;
  virtual ~ScreenCaptureData();

  uint8* data_;
  int stride_;
  SkRegion dirty_region_;
  SkISize size_;

  // Time spent in capture. Unit is in milliseconds.
  int capture_time_ms_;

  // Sequence number supplied by client for performance tracking.
  int64 client_sequence_number_;

  // DPI for this frame.
  SkIPoint dpi_;

  scoped_refptr<SharedBuffer> shared_buffer_;
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_SCREEN_CAPTURE_DATA_H_
