// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_
#define GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace gpu {

// An ImageDecodeAcceleratorWorker handles the actual hardware-accelerated
// decode of an image of a specific type (e.g., JPEG, WebP, etc.).
class ImageDecodeAcceleratorWorker {
 public:
  virtual ~ImageDecodeAcceleratorWorker() {}

  // Encapsulates the result of a decode request giving implementations the
  // chance to do custom resource management (e.g., some resources may need to
  // be released when the decoded data is no longer needed). Implementations
  // should not assume that destruction happens on a specific thread.
  class DecodeResult {
   public:
    virtual ~DecodeResult() {}
    virtual base::span<const uint8_t> GetData() const = 0;
    virtual size_t GetStride() const = 0;
    virtual SkImageInfo GetImageInfo() const = 0;
  };

  using CompletedDecodeCB =
      base::OnceCallback<void(std::unique_ptr<DecodeResult>)>;

  // Enqueue a decode of |encoded_data|. The |decode_cb| is called
  // asynchronously when the decode completes passing as parameter DecodeResult
  // containing the decoded image. For a successful decode, implementations must
  // guarantee that:
  //
  // 1) GetImageInfo().width() == |output_size|.width().
  // 2) GetImageInfo().height() == |output_size|.height().
  // 3) GetStride() >= GetImageInfo().minRowBytes().
  // 4) GetData().size() >= GetImageInfo().computeByteSize(stride()).
  //
  // If the decode fails, |decode_cb| is called asynchronously with nullptr.
  // Callbacks should be called in the order that this method is called.
  virtual void Decode(std::vector<uint8_t> encoded_data,
                      const gfx::Size& output_size,
                      CompletedDecodeCB decode_cb) = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_
