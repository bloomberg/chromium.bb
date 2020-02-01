/*
 * Copyright 2020 The libgav1 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBGAV1_EXAMPLES_GAV1_DECODE_CV_PIXEL_BUFFER_POOL_H_
#define LIBGAV1_EXAMPLES_GAV1_DECODE_CV_PIXEL_BUFFER_POOL_H_

#include <CoreVideo/CoreVideo.h>

#include <cstddef>
#include <memory>

#include "gav1/frame_buffer2.h"

extern "C" int Gav1DecodeOnCVPixelBufferSizeChanged(
    void* callback_private_data, int bitdepth,
    libgav1::ImageFormat image_format, int width, int height, int left_border,
    int right_border, int top_border, int bottom_border, int stride_alignment);

extern "C" int Gav1DecodeGetCVPixelBuffer(
    void* callback_private_data, int bitdepth,
    libgav1::ImageFormat image_format, int width, int height, int left_border,
    int right_border, int top_border, int bottom_border, int stride_alignment,
    libgav1::FrameBuffer2* frame_buffer);

extern "C" void Gav1DecodeReleaseCVPixelBuffer(void* callback_private_data,
                                               void* buffer_private_data);

class Gav1DecodeCVPixelBufferPool {
 public:
  static std::unique_ptr<Gav1DecodeCVPixelBufferPool> Create(
      size_t num_buffers);

  // Not copyable or movable.
  Gav1DecodeCVPixelBufferPool(const Gav1DecodeCVPixelBufferPool&) = delete;
  Gav1DecodeCVPixelBufferPool& operator=(const Gav1DecodeCVPixelBufferPool&) =
      delete;

  ~Gav1DecodeCVPixelBufferPool();

  int OnCVPixelBufferSizeChanged(int bitdepth,
                                 libgav1::ImageFormat image_format, int width,
                                 int height, int left_border, int right_border,
                                 int top_border, int bottom_border,
                                 int stride_alignment);

  int GetCVPixelBuffer(int bitdepth, libgav1::ImageFormat image_format,
                       int width, int height, int left_border, int right_border,
                       int top_border, int bottom_border, int stride_alignment,
                       libgav1::FrameBuffer2* frame_buffer);
  void ReleaseCVPixelBuffer(void* buffer_private_data);

 private:
  Gav1DecodeCVPixelBufferPool(size_t num_buffers);

  CVPixelBufferPoolRef pool_ = nullptr;
  const int num_buffers_;
};

#endif  // LIBGAV1_EXAMPLES_GAV1_DECODE_CV_PIXEL_BUFFER_POOL_H_
