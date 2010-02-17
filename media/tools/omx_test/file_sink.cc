// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/tools/omx_test/file_sink.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "media/tools/omx_test/color_space_util.h"

namespace media {

bool FileSink::AllocateEGLImages(int width, int height,
                                 std::vector<EGLImageKHR>* images) {
  NOTREACHED() << "This method is never used";
  return false;
}

void FileSink::ReleaseEGLImages(const std::vector<EGLImageKHR>& images) {
  NOTREACHED() << "This method is never used";
}

void FileSink::UseThisBuffer(int buffer_id, OMX_BUFFERHEADERTYPE* buffer) {
  CHECK(omx_buffers_.find(buffer_id) == omx_buffers_.end());
  omx_buffers_[buffer_id] = buffer;
}

void FileSink::StopUsingThisBuffer(int id) {
  omx_buffers_.erase(id);
}

void FileSink::BufferReady(int buffer_id, BufferUsedCallback* callback) {
  CHECK(omx_buffers_.find(buffer_id) != omx_buffers_.end());
  CHECK(callback);

  OMX_BUFFERHEADERTYPE* omx_buffer = omx_buffers_[buffer_id];
  uint8* buffer = omx_buffer->pBuffer;
  int size = omx_buffer->nFilledLen;

  // We never receive an end-of-stream buffer here.
  CHECK(!(omx_buffer->nFlags & OMX_BUFFERFLAG_EOS));

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

  // Always make the callback.
  callback->Run(buffer_id);
  delete callback;
}

bool FileSink::Initialize() {
  // Opens the output file for writing.
  if (!output_filename_.empty()) {
    output_file_.Set(file_util::OpenFile(output_filename_, "wb"));
    if (!output_file_.get()) {
      LOG(ERROR) << "can't open dump file %s" << output_filename_;
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
