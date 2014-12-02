// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_shared_bitmap_manager.h"

#include "content/child/child_thread.h"
#include "content/common/child_process_messages.h"
#include "ui/gfx/size.h"

namespace content {

namespace {

class ChildSharedBitmap : public cc::SharedBitmap {
 public:
  ChildSharedBitmap(scoped_refptr<ThreadSafeSender> sender,
                    base::SharedMemory* shared_memory,
                    const cc::SharedBitmapId& id)
      : SharedBitmap(static_cast<uint8*>(shared_memory->memory()), id),
        sender_(sender),
        shared_memory_(shared_memory) {}

  ChildSharedBitmap(scoped_refptr<ThreadSafeSender> sender,
                    scoped_ptr<base::SharedMemory> shared_memory_holder,
                    const cc::SharedBitmapId& id)
      : ChildSharedBitmap(sender, shared_memory_holder.get(), id) {
    shared_memory_holder_ = shared_memory_holder.Pass();
  }

  ~ChildSharedBitmap() override {
    sender_->Send(new ChildProcessHostMsg_DeletedSharedBitmap(id()));
  }

  base::SharedMemory* memory() override { return shared_memory_; }

 private:
  scoped_refptr<ThreadSafeSender> sender_;
  base::SharedMemory* shared_memory_;
  scoped_ptr<base::SharedMemory> shared_memory_holder_;
};

}  // namespace

ChildSharedBitmapManager::ChildSharedBitmapManager(
    scoped_refptr<ThreadSafeSender> sender)
    : sender_(sender) {
}

ChildSharedBitmapManager::~ChildSharedBitmapManager() {}

scoped_ptr<cc::SharedBitmap> ChildSharedBitmapManager::AllocateSharedBitmap(
    const gfx::Size& size) {
  TRACE_EVENT2("renderer",
               "ChildSharedBitmapManager::AllocateSharedMemory",
               "width",
               size.width(),
               "height",
               size.height());
  size_t memory_size;
  if (!cc::SharedBitmap::SizeInBytes(size, &memory_size))
    return scoped_ptr<cc::SharedBitmap>();
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
  scoped_ptr<base::SharedMemory> memory;
#if defined(OS_POSIX)
  base::SharedMemoryHandle handle;
  sender_->Send(new ChildProcessHostMsg_SyncAllocateSharedBitmap(
      memory_size, id, &handle));
  memory = make_scoped_ptr(new base::SharedMemory(handle, false));
  if (!memory->Map(memory_size))
    CHECK(false);
#else
  memory = ChildThread::AllocateSharedMemory(memory_size, sender_.get());
  CHECK(memory);
  if (!memory->Map(memory_size))
    CHECK(false);
  base::SharedMemoryHandle handle_to_send = memory->handle();
  sender_->Send(new ChildProcessHostMsg_AllocatedSharedBitmap(
      memory_size, handle_to_send, id));
#endif
  return make_scoped_ptr(new ChildSharedBitmap(sender_, memory.Pass(), id));
}

scoped_ptr<cc::SharedBitmap> ChildSharedBitmapManager::GetSharedBitmapFromId(
    const gfx::Size&,
    const cc::SharedBitmapId&) {
  NOTREACHED();
  return scoped_ptr<cc::SharedBitmap>();
}

scoped_ptr<cc::SharedBitmap> ChildSharedBitmapManager::GetBitmapForSharedMemory(
    base::SharedMemory* mem) {
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
  base::SharedMemoryHandle handle_to_send = mem->handle();
#if defined(OS_POSIX)
  if (!mem->ShareToProcess(base::GetCurrentProcessHandle(), &handle_to_send))
    return scoped_ptr<cc::SharedBitmap>();
#endif
  sender_->Send(new ChildProcessHostMsg_AllocatedSharedBitmap(
      mem->mapped_size(), handle_to_send, id));

  return make_scoped_ptr(new ChildSharedBitmap(sender_, mem, id));
}

}  // namespace content
