// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_TOOLS_OMX_TEST_FILE_SINK_H_
#define MEDIA_TOOLS_OMX_TEST_FILE_SINK_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/scoped_handle.h"
#include "base/scoped_ptr.h"

namespace media {

// This class writes output of a frame decoded by OmxCodec and save it to
// a file.
class FileSink {
 public:
  FileSink(const FilePath& output_path,
           bool simulate_copy,
           bool enable_csc);

  virtual ~FileSink();

  virtual void BufferReady(int size, uint8* buffer);

  // Initialize this object. Returns true if successful.
  bool Initialize();

  // Update the output frame size.
  void UpdateSize(int wdith, int height);

  // Write the frame buffer reference by |buffer|.
  void Write(uint8* buffer, int size);

 private:
  FilePath output_path_;
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

  DISALLOW_COPY_AND_ASSIGN(FileSink);
};

}  // namespace media

#endif  // MEDIA_TOOLS_OMX_TEST_FILE_SINK_H_
