// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

class GPU_GLES2_EXPORT SharedImageManager {
 public:
  SharedImageManager();
  ~SharedImageManager();

  // Registers a SharedImageBacking with the manager and returns true on
  // success. On success, the backing has one ref which may be released by
  // calling Unregister.
  bool Register(const Mailbox& mailbox,
                std::unique_ptr<SharedImageBacking> backing);

  // Releases the registration ref. If a backing reaches zero refs, it is
  // destroyed.
  void Unregister(const Mailbox& mailbox, bool have_context);

  // TODO(ericrk): Add the ability to get a backing as a
  // SharedImageRepresentation. Representations also take a ref on the
  // mailbox, releasing it when the representation is destroyed.

 private:
  struct BackingAndRefCount {
    BackingAndRefCount(std::unique_ptr<SharedImageBacking> backing,
                       uint32_t ref_count);
    BackingAndRefCount(BackingAndRefCount&& other);
    BackingAndRefCount& operator=(BackingAndRefCount&& rhs);
    ~BackingAndRefCount();
    std::unique_ptr<SharedImageBacking> backing;
    uint32_t ref_count = 0;
  };
  base::flat_map<Mailbox, BackingAndRefCount> images_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_MANAGER_H_
