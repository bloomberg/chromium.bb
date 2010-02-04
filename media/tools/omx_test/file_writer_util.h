// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_TOOLS_OMX_TEST_FILE_WRITER_UTIL_H_
#define MEDIA_TOOLS_OMX_TEST_FILE_WRITER_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_handle.h"
#include "base/scoped_ptr.h"

namespace media {

// This class writes output of a frame decoded by OmxCodec and save it to
// a file.
class FileWriter {
 public:
  FileWriter(std::string output_filename,
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
  int width_;
  int height_;
  scoped_array<uint8> copy_buf_;
  int copy_buf_size_;
  scoped_array<uint8> csc_buf_;
  int csc_buf_size_;

  DISALLOW_COPY_AND_ASSIGN(FileWriter);
};

}  // namespace media

#endif  // MEDIA_TOOLS_OMX_TEST_FILE_WRITER_UTIL_H_
