// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_SHARED_BITMAP_H_
#define COMPONENTS_VIZ_COMMON_QUADS_SHARED_BITMAP_H_

#include <stddef.h>
#include <stdint.h>

#include "base/hash.h"
#include "base/macros.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SharedMemoryHandle;
}

namespace viz {
using SharedBitmapId = gpu::Mailbox;

struct SharedBitmapIdHash {
  size_t operator()(const SharedBitmapId& id) const {
    return base::Hash(id.name, sizeof(id.name));
  }
};

VIZ_COMMON_EXPORT base::trace_event::MemoryAllocatorDumpGuid
GetSharedBitmapGUIDForTracing(const SharedBitmapId& bitmap_id);

class VIZ_COMMON_EXPORT SharedBitmap {
 public:
  SharedBitmap(uint8_t* pixels,
               const SharedBitmapId& id,
               uint32_t sequence_number);

  virtual ~SharedBitmap();

  uint8_t* pixels() { return pixels_; }

  const SharedBitmapId& id() { return id_; }

  // The sequence number that ClientSharedBitmapManager assigned to this
  // SharedBitmap.
  uint32_t sequence_number() const { return sequence_number_; }

  // Returns the shared memory's handle when the back end is base::SharedMemory.
  // Otherwise, this returns an invalid handle.
  virtual base::SharedMemoryHandle GetSharedMemoryHandle() const = 0;

  // Returns true if the size is valid and false otherwise.
  static bool SizeInBytes(const gfx::Size& size, size_t* size_in_bytes);
  // Dies with a CRASH() if the size can not be represented as a positive number
  // of bytes.
  static size_t CheckedSizeInBytes(const gfx::Size& size);
  // Returns the size in bytes but may overflow or return 0. Only do this for
  // sizes that have already been checked.
  static size_t UncheckedSizeInBytes(const gfx::Size& size);
  // Returns true if the size is valid and false otherwise.
  static bool VerifySizeInBytes(const gfx::Size& size);

  static SharedBitmapId GenerateId();

 private:
  uint8_t* pixels_;
  SharedBitmapId id_;
  const uint32_t sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(SharedBitmap);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_SHARED_BITMAP_H_
