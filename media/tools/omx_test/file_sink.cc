// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/tools/omx_test/file_sink.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "media/tools/omx_test/color_space_util.h"

namespace media {

FileSink::FileSink(const FilePath& output_path,
                   bool simulate_copy,
                   bool enable_csc)
    : output_path_(output_path),
      simulate_copy_(simulate_copy),
      enable_csc_(enable_csc),
      width_(0),
      height_(0),
      copy_buf_size_(0),
      csc_buf_size_(0) {
}

FileSink::~FileSink() {}

void FileSink::BufferReady(int size, uint8* buffer) {
  if (size > copy_buf_size_) {
    copy_buf_.reset(new uint8[size]);
    copy_buf_size_ = size;
  }
  if (size > csc_buf_size_) {
    csc_buf_.reset(new uint8[size]);
    csc_buf_size_ = size;
  }

  // Copy the output of the decoder to user memory.
  if (simulate_copy_ || output_file_.get())  // Implies a copy.
    memcpy(copy_buf_.get(), buffer, size);

  uint8* out_buffer = copy_buf_.get();
  if (enable_csc_) {
    // Now assume the raw output is NV21.
    media::NV21toIYUV(copy_buf_.get(), csc_buf_.get(), width_, height_);
    out_buffer = csc_buf_.get();
  }

  if (output_file_.get())
    fwrite(out_buffer, sizeof(uint8), size, output_file_.get());
}

bool FileSink::Initialize() {
  // Opens the output file for writing.
  if (!output_path_.empty()) {
    output_file_.Set(file_util::OpenFile(output_path_, "wb"));
    if (!output_file_.get()) {
      LOG(ERROR) << "can't open dump file %s" << output_path_.value();
      return false;
    }
  }
  return true;
}

void FileSink::UpdateSize(int width, int height) {
  width_ = width;
  height_ = height;
}

}  // namespace media
