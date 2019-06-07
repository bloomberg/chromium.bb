// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wilco_dtc_supportd/mojo_utils.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace chromeos {

base::StringPiece GetStringPieceFromMojoHandle(
    mojo::ScopedHandle handle,
    base::ReadOnlySharedMemoryMapping* shared_memory) {
  mojo::ScopedSharedBufferHandle buffer_handle(
      mojo::SharedBufferHandle(handle.release().value()));
  base::ReadOnlySharedMemoryRegion memory_region =
      UnwrapReadOnlySharedMemoryRegion(std::move(buffer_handle));

  *shared_memory = memory_region.Map();
  if (!shared_memory->IsValid())
    return base::StringPiece();

  return base::StringPiece(static_cast<const char*>(shared_memory->memory()),
                           shared_memory->size());
}

mojo::ScopedHandle CreateReadOnlySharedMemoryMojoHandle(
    const std::string& content) {
  if (content.empty())
    return mojo::ScopedHandle();

  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(content.size());
  if (!shm.IsValid())
    return mojo::ScopedHandle();
  memcpy(shm.mapping.memory(), content.data(), content.length());

  mojo::ScopedSharedBufferHandle buffer_handle =
      mojo::WrapReadOnlySharedMemoryRegion(std::move(shm.region));
  mojo::ScopedHandle handle(mojo::Handle(buffer_handle.release().value()));
  return handle;
}

}  // namespace chromeos
