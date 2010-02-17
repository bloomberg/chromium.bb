// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_TOOLS_OMX_TEST_FILE_SINK_H_
#define MEDIA_TOOLS_OMX_TEST_FILE_SINK_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_handle.h"
#include "base/scoped_ptr.h"
#include "media/omx/omx_output_sink.h"

namespace media {

// This class writes output of a frame decoded by OmxCodec and save it to
// a file.
class FileSink : public OmxOutputSink {
 public:
  FileSink(std::string output_filename,
           bool simulate_copy,
           bool enable_csc)
      : output_filename_(output_filename),
        simulate_copy_(simulate_copy),
        enable_csc_(enable_csc),
        width_(0),
        height_(0),
        copy_buf_size_(0),
        csc_buf_size_(0) {
  }

  // OmxOutputSink implementations.
  virtual bool ProvidesEGLImages() const { return false; }
  virtual bool AllocateEGLImages(int width, int height,
                                 std::vector<EGLImageKHR>* images);
  virtual void ReleaseEGLImages(const std::vector<EGLImageKHR>& images);
  virtual void UseThisBuffer(int buffer_id, OMX_BUFFERHEADERTYPE* buffer);
  virtual void StopUsingThisBuffer(int id);
  virtual void BufferReady(int buffer_id, BufferUsedCallback* callback);

  // Initialize this object. Returns true if successful.
  bool Initialize();

  // Update the output frame size.
  void UpdateSize(int wdith, int height);

  // Write the frame buffer reference by |buffer|.
  void Write(uint8* buffer, int size);

 private:
  std::string output_filename_;
  bool simulate_copy_;
  bool enable_csc_;
  ScopedStdioHandle output_file_;

  // Image properties.
  int width_;
  int height_;

  // Buffers for copying and color space conversion.
  scoped_array<uint8> copy_buf_;
  int copy_buf_size_;
  scoped_array<uint8> csc_buf_;
  int csc_buf_size_;

  std::map<int, OMX_BUFFERHEADERTYPE*> omx_buffers_;

  DISALLOW_COPY_AND_ASSIGN(FileSink);
};

}  // namespace media

#endif  // MEDIA_TOOLS_OMX_TEST_FILE_SINK_H_
