// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_shared_bitmap_manager.h"

#include <stddef.h>

#include <utility>

#include "base/debug/alias.h"
#include "base/memory/ptr_util.h"
#include "base/process/memory.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "content/common/child_process_messages.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

class ChildSharedBitmap : public cc::SharedBitmap {
 public:
  ChildSharedBitmap(
      const scoped_refptr<mojom::ThreadSafeRenderMessageFilterAssociatedPtr>&
          render_message_filter_ptr,
      base::SharedMemory* shared_memory,
      const cc::SharedBitmapId& id)
      : cc::SharedBitmap(static_cast<uint8_t*>(shared_memory->memory()), id),
        render_message_filter_ptr_(render_message_filter_ptr) {}

  ChildSharedBitmap(
      const scoped_refptr<mojom::ThreadSafeRenderMessageFilterAssociatedPtr>&
          render_message_filter_ptr,
      std::unique_ptr<base::SharedMemory> shared_memory_holder,
      const cc::SharedBitmapId& id)
      : ChildSharedBitmap(render_message_filter_ptr,
                          shared_memory_holder.get(),
                          id) {
    shared_memory_holder_ = std::move(shared_memory_holder);
  }

  ~ChildSharedBitmap() override {
    (*render_message_filter_ptr_)->DeletedSharedBitmap(id());
  }

 private:
  scoped_refptr<mojom::ThreadSafeRenderMessageFilterAssociatedPtr>
      render_message_filter_ptr_;
  std::unique_ptr<base::SharedMemory> shared_memory_holder_;
};

// Collect extra information for debugging bitmap creation failures.
void CollectMemoryUsageAndDie(const gfx::Size& size, size_t alloc_size) {
#if defined(OS_WIN)
  int width = size.width();
  int height = size.height();
  DWORD last_error = GetLastError();

  std::unique_ptr<base::ProcessMetrics> metrics(
      base::ProcessMetrics::CreateProcessMetrics(
          base::GetCurrentProcessHandle()));

  size_t private_bytes = 0;
  size_t shared_bytes = 0;
  metrics->GetMemoryBytes(&private_bytes, &shared_bytes);

  base::debug::Alias(&width);
  base::debug::Alias(&height);
  base::debug::Alias(&last_error);
  base::debug::Alias(&private_bytes);
  base::debug::Alias(&shared_bytes);
#endif
  base::TerminateBecauseOutOfMemory(alloc_size);
}

}  // namespace

ChildSharedBitmapManager::ChildSharedBitmapManager(
    const scoped_refptr<mojom::ThreadSafeRenderMessageFilterAssociatedPtr>&
        render_message_filter_ptr)
    : render_message_filter_ptr_(render_message_filter_ptr) {}

ChildSharedBitmapManager::~ChildSharedBitmapManager() {}

std::unique_ptr<cc::SharedBitmap>
ChildSharedBitmapManager::AllocateSharedBitmap(const gfx::Size& size) {
  TRACE_EVENT2("renderer", "ChildSharedBitmapManager::AllocateSharedBitmap",
               "width", size.width(), "height", size.height());
  size_t memory_size;
  if (!cc::SharedBitmap::SizeInBytes(size, &memory_size))
    return nullptr;
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
  std::unique_ptr<base::SharedMemory> memory =
      ChildThreadImpl::AllocateSharedMemory(memory_size);
  if (!memory || !memory->Map(memory_size))
    CollectMemoryUsageAndDie(size, memory_size);

  NotifyAllocatedSharedBitmap(memory.get(), id);

  // Close the associated FD to save resources, the previously mapped memory
  // remains available.
  memory->Close();

  return base::MakeUnique<ChildSharedBitmap>(render_message_filter_ptr_,
                                             std::move(memory), id);
}

std::unique_ptr<cc::SharedBitmap>
ChildSharedBitmapManager::GetSharedBitmapFromId(const gfx::Size&,
                                                const cc::SharedBitmapId&) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<cc::SharedBitmap>
ChildSharedBitmapManager::GetBitmapForSharedMemory(base::SharedMemory* mem) {
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
  NotifyAllocatedSharedBitmap(mem, cc::SharedBitmap::GenerateId());
  return base::MakeUnique<ChildSharedBitmap>(render_message_filter_ptr_,
                                             std::move(mem), id);
}

// Notifies the browser process that a shared bitmap with the given ID was
// allocated. Caller keeps ownership of |memory|.
void ChildSharedBitmapManager::NotifyAllocatedSharedBitmap(
    base::SharedMemory* memory,
    const cc::SharedBitmapId& id) {
  base::SharedMemoryHandle handle_to_send =
      base::SharedMemory::DuplicateHandle(memory->handle());
  if (!base::SharedMemory::IsHandleValid(handle_to_send)) {
    LOG(ERROR) << "Failed to duplicate shared memory handle for bitmap.";
    return;
  }

  mojo::ScopedSharedBufferHandle buffer_handle = mojo::WrapSharedMemoryHandle(
      handle_to_send, memory->mapped_size(), true /* read_only */);

  (*render_message_filter_ptr_)
      ->AllocatedSharedBitmap(std::move(buffer_handle), id);
}

}  // namespace content
