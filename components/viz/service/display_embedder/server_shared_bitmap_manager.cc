// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"

#include <stdint.h>

#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

class BitmapData : public base::RefCountedThreadSafe<BitmapData> {
 public:
  explicit BitmapData(size_t buffer_size) : buffer_size(buffer_size) {}
  std::unique_ptr<base::SharedMemory> memory;
  std::unique_ptr<uint8_t[]> pixels;
  size_t buffer_size;

 private:
  friend class base::RefCountedThreadSafe<BitmapData>;
  ~BitmapData() {}
  DISALLOW_COPY_AND_ASSIGN(BitmapData);
};

namespace {

class ServerSharedBitmap : public SharedBitmap {
 public:
  ServerSharedBitmap(uint8_t* pixels,
                     scoped_refptr<BitmapData> bitmap_data,
                     const SharedBitmapId& id,
                     ServerSharedBitmapManager* manager)
      : SharedBitmap(pixels, id, 0 /* sequence_number */),
        bitmap_data_(bitmap_data),
        manager_(manager) {}

  ~ServerSharedBitmap() override {
    if (manager_)
      manager_->FreeSharedMemoryFromMap(id());
  }

  // SharedBitmap:
  base::SharedMemoryHandle GetSharedMemoryHandle() const override {
    if (!bitmap_data_->memory)
      return base::SharedMemoryHandle();
    return bitmap_data_->memory->handle();
  }

 private:
  scoped_refptr<BitmapData> bitmap_data_;
  ServerSharedBitmapManager* manager_;
};

}  // namespace

base::LazyInstance<ServerSharedBitmapManager>::DestructorAtExit
    g_shared_memory_manager = LAZY_INSTANCE_INITIALIZER;

ServerSharedBitmapManager::ServerSharedBitmapManager() = default;

ServerSharedBitmapManager::~ServerSharedBitmapManager() {
  DCHECK(handle_map_.empty());
}

ServerSharedBitmapManager* ServerSharedBitmapManager::current() {
  return g_shared_memory_manager.Pointer();
}

std::unique_ptr<SharedBitmap> ServerSharedBitmapManager::AllocateSharedBitmap(
    const gfx::Size& size) {
  base::AutoLock lock(lock_);
  size_t bitmap_size;
  if (!SharedBitmap::SizeInBytes(size, &bitmap_size))
    return nullptr;

  scoped_refptr<BitmapData> data(new BitmapData(bitmap_size));
  // Bitmaps allocated in server don't need to be shared to other processes, so
  // allocate them with new instead.
  data->pixels = std::unique_ptr<uint8_t[]>(new uint8_t[bitmap_size]);

  SharedBitmapId id = SharedBitmap::GenerateId();
  handle_map_[id] = data;
  return base::MakeUnique<ServerSharedBitmap>(data->pixels.get(), data, id,
                                              this);
}

std::unique_ptr<SharedBitmap> ServerSharedBitmapManager::GetSharedBitmapFromId(
    const gfx::Size& size,
    const SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  auto it = handle_map_.find(id);
  if (it == handle_map_.end())
    return nullptr;

  BitmapData* data = it->second.get();

  size_t bitmap_size;
  if (!SharedBitmap::SizeInBytes(size, &bitmap_size) ||
      bitmap_size > data->buffer_size)
    return nullptr;

  if (data->pixels) {
    return base::MakeUnique<ServerSharedBitmap>(data->pixels.get(), data, id,
                                                nullptr);
  }
  if (!data->memory->memory()) {
    return nullptr;
  }

  return base::MakeUnique<ServerSharedBitmap>(
      static_cast<uint8_t*>(data->memory->memory()), data, id, nullptr);
}

bool ServerSharedBitmapManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(lock_);

  for (const auto& bitmap : handle_map_) {
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(base::StringPrintf(
            "sharedbitmap/%s",
            base::HexEncode(bitmap.first.name, sizeof(bitmap.first.name))
                .c_str()));
    if (!dump)
      return false;

    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    bitmap.second->buffer_size);

    if (bitmap.second->memory) {
      base::UnguessableToken shared_memory_guid =
          bitmap.second->memory->mapped_id();
      if (!shared_memory_guid.is_empty()) {
        pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), shared_memory_guid,
                                             0 /* importance*/);
      }
    } else {
      // Generate a global GUID used to share this allocation with renderer
      // processes.
      auto guid = GetSharedBitmapGUIDForTracing(bitmap.first);
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid);
    }
  }

  return true;
}

bool ServerSharedBitmapManager::ChildAllocatedSharedBitmap(
    size_t buffer_size,
    const base::SharedMemoryHandle& handle,
    const SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  if (handle_map_.find(id) != handle_map_.end())
    return false;
  auto data = base::MakeRefCounted<BitmapData>(buffer_size);
  data->memory = base::MakeUnique<base::SharedMemory>(handle, false);
  data->memory->Map(data->buffer_size);
  data->memory->Close();
  handle_map_[id] = std::move(data);
  return true;
}

void ServerSharedBitmapManager::ChildDeletedSharedBitmap(
    const SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  handle_map_.erase(id);
}

size_t ServerSharedBitmapManager::AllocatedBitmapCount() const {
  base::AutoLock lock(lock_);
  return handle_map_.size();
}

void ServerSharedBitmapManager::FreeSharedMemoryFromMap(
    const SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  handle_map_.erase(id);
}

}  // namespace viz
