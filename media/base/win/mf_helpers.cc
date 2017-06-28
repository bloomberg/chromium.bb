// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_helpers.h"

#include "base/metrics/histogram_macros.h"

namespace media {

namespace mf {

void LogDXVAError(int line) {
  LOG(ERROR) << "Error in dxva_video_decode_accelerator_win.cc on line "
             << line;
  UMA_HISTOGRAM_SPARSE_SLOWLY("Media.DXVAVDA.ErrorLine", line);
}

base::win::ScopedComPtr<IMFSample> CreateEmptySampleWithBuffer(
    uint32_t buffer_length,
    int align) {
  CHECK_GT(buffer_length, 0U);

  base::win::ScopedComPtr<IMFSample> sample;
  HRESULT hr = MFCreateSample(sample.GetAddressOf());
  RETURN_ON_HR_FAILURE(hr, "MFCreateSample failed",
                       base::win::ScopedComPtr<IMFSample>());

  base::win::ScopedComPtr<IMFMediaBuffer> buffer;
  if (align == 0) {
    // Note that MFCreateMemoryBuffer is same as MFCreateAlignedMemoryBuffer
    // with the align argument being 0.
    hr = MFCreateMemoryBuffer(buffer_length, buffer.GetAddressOf());
  } else {
    hr = MFCreateAlignedMemoryBuffer(buffer_length, align - 1,
                                     buffer.GetAddressOf());
  }
  RETURN_ON_HR_FAILURE(hr, "Failed to create memory buffer for sample",
                       base::win::ScopedComPtr<IMFSample>());

  hr = sample->AddBuffer(buffer.Get());
  RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample",
                       base::win::ScopedComPtr<IMFSample>());

  buffer->SetCurrentLength(0);
  return sample;
}

MediaBufferScopedPointer::MediaBufferScopedPointer(IMFMediaBuffer* media_buffer)
    : media_buffer_(media_buffer),
      buffer_(nullptr),
      max_length_(0),
      current_length_(0) {
  HRESULT hr = media_buffer_->Lock(&buffer_, &max_length_, &current_length_);
  CHECK(SUCCEEDED(hr));
}

MediaBufferScopedPointer::~MediaBufferScopedPointer() {
  HRESULT hr = media_buffer_->Unlock();
  CHECK(SUCCEEDED(hr));
}

}  // namespace mf

}  // namespace media
