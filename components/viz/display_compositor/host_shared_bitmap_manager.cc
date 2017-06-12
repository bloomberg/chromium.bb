// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/display_compositor/host_shared_bitmap_manager.h"

#include <stdint.h>

#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/platform_handle.h"
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

class HostSharedBitmap : public cc::SharedBitmap {
 public:
  HostSharedBitmap(uint8_t* pixels,
                   scoped_refptr<BitmapData> bitmap_data,
                   const cc::SharedBitmapId& id,
                   HostSharedBitmapManager* manager)
      : SharedBitmap(pixels, id),
        bitmap_data_(bitmap_data),
        manager_(manager) {}

  ~HostSharedBitmap() override {
    if (manager_)
      manager_->FreeSharedMemoryFromMap(id());
  }

  // cc::SharedBitmap:
  base::SharedMemoryHandle GetSharedMemoryHandle() const override {
    if (!bitmap_data_->memory)
      return base::SharedMemoryHandle();
    return bitmap_data_->memory->handle();
  }

 private:
  scoped_refptr<BitmapData> bitmap_data_;
  HostSharedBitmapManager* manager_;
};

}  // namespace

base::LazyInstance<HostSharedBitmapManager>::DestructorAtExit
    g_shared_memory_manager = LAZY_INSTANCE_INITIALIZER;

HostSharedBitmapManagerClient::HostSharedBitmapManagerClient(
    HostSharedBitmapManager* manager)
    : manager_(manager), binding_(this) {}

HostSharedBitmapManagerClient::~HostSharedBitmapManagerClient() {
  for (const auto& id : owned_bitmaps_)
    manager_->ChildDeletedSharedBitmap(id);
}

void HostSharedBitmapManagerClient::Bind(
    cc::mojom::SharedBitmapManagerAssociatedRequest request) {
  binding_.Bind(std::move(request));
}

void HostSharedBitmapManagerClient::DidAllocateSharedBitmap(
    mojo::ScopedSharedBufferHandle buffer,
    const cc::SharedBitmapId& id) {
  base::SharedMemoryHandle memory_handle;
  size_t size;
  MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(buffer), &memory_handle, &size, NULL);
  DCHECK_EQ(result, MOJO_RESULT_OK);
  this->ChildAllocatedSharedBitmap(size, memory_handle, id);
}

void HostSharedBitmapManagerClient::ChildAllocatedSharedBitmap(
    size_t buffer_size,
    const base::SharedMemoryHandle& handle,
    const cc::SharedBitmapId& id) {
  if (manager_->ChildAllocatedSharedBitmap(buffer_size, handle, id)) {
    base::AutoLock lock(lock_);
    owned_bitmaps_.insert(id);
  }
}

void HostSharedBitmapManagerClient::DidDeleteSharedBitmap(
    const cc::SharedBitmapId& id) {
  manager_->ChildDeletedSharedBitmap(id);
  {
    base::AutoLock lock(lock_);
    owned_bitmaps_.erase(id);
  }
}

HostSharedBitmapManager::HostSharedBitmapManager() {}
HostSharedBitmapManager::~HostSharedBitmapManager() {
  DCHECK(handle_map_.empty());
}

HostSharedBitmapManager* HostSharedBitmapManager::current() {
  return g_shared_memory_manager.Pointer();
}

std::unique_ptr<cc::SharedBitmap> HostSharedBitmapManager::AllocateSharedBitmap(
    const gfx::Size& size) {
  base::AutoLock lock(lock_);
  size_t bitmap_size;
  if (!cc::SharedBitmap::SizeInBytes(size, &bitmap_size))
    return nullptr;

  scoped_refptr<BitmapData> data(new BitmapData(bitmap_size));
  // Bitmaps allocated in host don't need to be shared to other processes, so
  // allocate them with new instead.
  data->pixels = std::unique_ptr<uint8_t[]>(new uint8_t[bitmap_size]);

  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
  handle_map_[id] = data;
  return base::MakeUnique<HostSharedBitmap>(data->pixels.get(), data, id, this);
}

std::unique_ptr<cc::SharedBitmap>
HostSharedBitmapManager::GetSharedBitmapFromId(const gfx::Size& size,
                                               const cc::SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  BitmapMap::iterator it = handle_map_.find(id);
  if (it == handle_map_.end())
    return nullptr;

  BitmapData* data = it->second.get();

  size_t bitmap_size;
  if (!cc::SharedBitmap::SizeInBytes(size, &bitmap_size) ||
      bitmap_size > data->buffer_size)
    return nullptr;

  if (data->pixels) {
    return base::MakeUnique<HostSharedBitmap>(data->pixels.get(), data, id,
                                              nullptr);
  }
  if (!data->memory->memory()) {
    return nullptr;
  }

  return base::MakeUnique<HostSharedBitmap>(
      static_cast<uint8_t*>(data->memory->memory()), data, id, nullptr);
}

bool HostSharedBitmapManager::OnMemoryDump(
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

    // Generate a global GUID used to share this allocation with renderer
    // processes.
    auto guid = cc::GetSharedBitmapGUIDForTracing(bitmap.first);
    base::UnguessableToken shared_memory_guid;
    if (bitmap.second->memory) {
      shared_memory_guid = bitmap.second->memory->handle().GetGUID();
      pmd->CreateSharedMemoryOwnershipEdge(
          dump->guid(), guid, shared_memory_guid, 0 /* importance*/);
    } else {
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid);
    }
  }

  return true;
}

bool HostSharedBitmapManager::ChildAllocatedSharedBitmap(
    size_t buffer_size,
    const base::SharedMemoryHandle& handle,
    const cc::SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  if (handle_map_.find(id) != handle_map_.end())
    return false;
  scoped_refptr<BitmapData> data(new BitmapData(buffer_size));

  handle_map_[id] = data;
  data->memory = base::MakeUnique<base::SharedMemory>(handle, false);
  data->memory->Map(data->buffer_size);
  data->memory->Close();
  return true;
}

void HostSharedBitmapManager::ChildDeletedSharedBitmap(
    const cc::SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  handle_map_.erase(id);
}

size_t HostSharedBitmapManager::AllocatedBitmapCount() const {
  base::AutoLock lock(lock_);
  return handle_map_.size();
}

void HostSharedBitmapManager::FreeSharedMemoryFromMap(
    const cc::SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  handle_map_.erase(id);
}

}  // namespace viz
