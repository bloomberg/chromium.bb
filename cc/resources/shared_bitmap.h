// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_SHARED_BITMAP_H_
#define CC_RESOURCES_SHARED_BITMAP_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "cc/cc_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SharedMemoryHandle;
}

namespace cc {
typedef gpu::Mailbox SharedBitmapId;

CC_EXPORT base::trace_event::MemoryAllocatorDumpGuid
GetSharedBitmapGUIDForTracing(const SharedBitmapId& bitmap_id);

class CC_EXPORT SharedBitmap {
 public:
  SharedBitmap(uint8_t* pixels, const SharedBitmapId& id);

  virtual ~SharedBitmap();

  uint8_t* pixels() { return pixels_; }

  const SharedBitmapId& id() { return id_; }

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

  DISALLOW_COPY_AND_ASSIGN(SharedBitmap);
};

}  // namespace cc

#endif  // CC_RESOURCES_SHARED_BITMAP_H_
