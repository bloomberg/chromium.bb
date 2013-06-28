// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_DELEGATE_H_
#define GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_DELEGATE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "gpu/gpu_export.h"
#include "ui/gl/gl_bindings.h"

namespace base {
class SharedMemory;
}

namespace gpu {

class ScopedSafeSharedMemory;

struct AsyncTexImage2DParams {
  GLenum target;
  GLint level;
  GLenum internal_format;
  GLsizei width;
  GLsizei height;
  GLint border;
  GLenum format;
  GLenum type;
};

struct AsyncTexSubImage2DParams {
  GLenum target;
  GLint level;
  GLint xoffset;
  GLint yoffset;
  GLsizei width;
  GLsizei height;
  GLenum format;
  GLenum type;
};

struct AsyncMemoryParams {
  base::SharedMemory* shared_memory;
  uint32 shm_size;
  uint32 shm_data_offset;
  uint32 shm_data_size;
};

class AsyncPixelTransferUploadStats
    : public base::RefCountedThreadSafe<AsyncPixelTransferUploadStats> {
 public:
  AsyncPixelTransferUploadStats();

  void AddUpload(base::TimeDelta transfer_time);
  int GetStats(base::TimeDelta* total_texture_upload_time);

 private:
  friend class base::RefCountedThreadSafe<AsyncPixelTransferUploadStats>;

  ~AsyncPixelTransferUploadStats();

  int texture_upload_count_;
  base::TimeDelta total_texture_upload_time_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferUploadStats);
};

class GPU_EXPORT AsyncPixelTransferDelegate {
 public:
  virtual ~AsyncPixelTransferDelegate();

  // The callback occurs on the caller thread, once the texture is
  // safe/ready to be used.
  virtual void AsyncTexImage2D(
      const AsyncTexImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params,
      const base::Closure& bind_callback) = 0;

  virtual void AsyncTexSubImage2D(
      const AsyncTexSubImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params) = 0;

  // Returns true if there is a transfer in progress.
  virtual bool TransferIsInProgress() = 0;

  // Block until the specified transfer completes.
  virtual void WaitForTransferCompletion() = 0;

  // Gets the address of the data from shared memory.
  static void* GetAddress(const AsyncMemoryParams& mem_params);

  // Sometimes the |safe_shared_memory| is duplicate to prevent use after free.
  static void* GetAddress(ScopedSafeSharedMemory* safe_shared_memory,
                          const AsyncMemoryParams& mem_params);

 protected:
  AsyncPixelTransferDelegate();

 private:
  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferDelegate);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_DELEGATE_H_

