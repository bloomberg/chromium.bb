// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_
#define GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_

#include <vector>

#include "base/callback.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace gpu {

// An ImageDecodeAcceleratorWorker handles the actual hardware-accelerated
// decode of an image of a specific type (e.g., JPEG, WebP, etc.).
class ImageDecodeAcceleratorWorker {
 public:
  virtual ~ImageDecodeAcceleratorWorker() {}

  // Enqueue a decode of |encoded_data|. The |decode_cb| is called
  // asynchronously when the decode completes passing as a parameter a vector
  // containing the decoded image in RGBA format (the stride of the output is
  // |output_size|.width() * 4). If the decode fails, |decode_cb| is called
  // asynchronously with an empty vector. Callbacks should be called in the
  // order that this method is called.
  virtual void Decode(
      std::vector<uint8_t> encoded_data,
      const gfx::Size& output_size,
      base::OnceCallback<void(std::vector<uint8_t>)> decode_cb) = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_DECODE_ACCELERATOR_WORKER_H_
