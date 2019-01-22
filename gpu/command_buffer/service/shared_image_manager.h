// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_MANAGER_H_

#include "base/containers/flat_set.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class SharedImageRepresentationFactoryRef;

class GPU_GLES2_EXPORT SharedImageManager {
 public:
  SharedImageManager();
  ~SharedImageManager();

  // Registers a SharedImageBacking with the manager and returns a
  // SharedImageRepresentationFactoryRef which holds a ref on the SharedImage.
  // The factory should delete this object to release the ref.
  std::unique_ptr<SharedImageRepresentationFactoryRef> Register(
      std::unique_ptr<SharedImageBacking> backing,
      MemoryTypeTracker* ref);

  // Marks the backing associated with a mailbox as context lost.
  void OnContextLost(const Mailbox& mailbox);

  // Accessors which return a SharedImageRepresentation. Representations also
  // take a ref on the mailbox, releasing it when the representation is
  // destroyed.
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref);
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(const Mailbox& mailbox, MemoryTypeTracker* ref);
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      const Mailbox& mailbox,
      MemoryTypeTracker* ref);

  // Called by SharedImageRepresentation in the destructor.
  void OnRepresentationDestroyed(const Mailbox& mailbox,
                                 SharedImageRepresentation* representation);

  // Dump memory for the given mailbox.
  void OnMemoryDump(const Mailbox& mailbox,
                    base::trace_event::ProcessMemoryDump* pmd,
                    int client_id,
                    uint64_t client_tracing_id);

 private:
  base::flat_set<std::unique_ptr<SharedImageBacking>> images_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_MANAGER_H_
