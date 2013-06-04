// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_MANAGER_H_

#include <set>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/gpu_export.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template <>
  struct hash<gpu::gles2::TextureRef*> {
  size_t operator()(gpu::gles2::TextureRef* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

namespace gfx {
class GLContext;
}

namespace gpu {
class AsyncPixelTransferDelegate;
class AsyncPixelTransferState;
struct AsyncTexImage2DParams;

class GPU_EXPORT AsyncPixelTransferManager
    : public gles2::TextureManager::DestructionObserver {
 public:
  static AsyncPixelTransferManager* Create(gfx::GLContext* context);

  virtual ~AsyncPixelTransferManager();

  void Initialize(gles2::TextureManager* texture_manager);

  virtual AsyncPixelTransferDelegate* GetAsyncPixelTransferDelegate() = 0;

  AsyncPixelTransferState* CreatePixelTransferState(
      gles2::TextureRef* ref,
      const AsyncTexImage2DParams& define_params);

  AsyncPixelTransferState* GetPixelTransferState(
      gles2::TextureRef* ref);

  void ClearPixelTransferStateForTest(gles2::TextureRef* ref);

  bool AsyncTransferIsInProgress(gles2::TextureRef* ref);

  // gles2::TextureRef::DestructionObserver implementation:
  virtual void OnTextureManagerDestroying(gles2::TextureManager* manager)
      OVERRIDE;
  virtual void OnTextureRefDestroying(gles2::TextureRef* texture) OVERRIDE;

 protected:
  AsyncPixelTransferManager();

 private:
  gles2::TextureManager* manager_;

  typedef base::hash_map<gles2::TextureRef*,
                         scoped_refptr<AsyncPixelTransferState> >
      TextureToStateMap;
  TextureToStateMap state_map_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_MANAGER_H_
