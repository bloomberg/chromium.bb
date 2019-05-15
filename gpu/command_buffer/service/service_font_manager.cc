// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_font_manager.h"

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/common/discardable_handle.h"

namespace gpu {

namespace {
class Deserializer {
 public:
  Deserializer(const volatile char* memory, uint32_t memory_size)
      : memory_(memory), memory_size_(memory_size) {}
  ~Deserializer() = default;

  template <typename T>
  bool Read(T* val) {
    static_assert(base::is_trivially_copyable<T>::value,
                  "Not trivially copyable");
    if (!AlignMemory(sizeof(T), alignof(T)))
      return false;

    *val = *reinterpret_cast<const T*>(const_cast<const char*>(memory_));

    memory_ += sizeof(T);
    bytes_read_ += sizeof(T);
    return true;
  }

  bool ReadStrikeData(SkStrikeClient* strike_client, uint32_t size) {
    if (size == 0u)
      return true;

    if (!AlignMemory(size, 16))
      return false;

    if (size > memory_size_ - bytes_read_)
      return false;

    if (!strike_client->readStrikeData(memory_, size))
      return false;

    bytes_read_ += size;
    memory_ += size;
    return true;
  }

 private:
  bool AlignMemory(uint32_t size, size_t alignment) {
    // Due to the math below, alignment must be a power of two.
    DCHECK_GT(alignment, 0u);
    DCHECK_EQ(alignment & (alignment - 1), 0u);

    uintptr_t memory = reinterpret_cast<uintptr_t>(memory_);
    size_t padding = ((memory + alignment - 1) & ~(alignment - 1)) - memory;

    base::CheckedNumeric<uint32_t> checked_padded_size = bytes_read_;
    checked_padded_size += padding;
    checked_padded_size += size;
    uint32_t padded_size = 0;
    if (!checked_padded_size.AssignIfValid(&padded_size) ||
        padded_size > memory_size_)
      return false;

    memory_ += padding;
    bytes_read_ += padding;
    return true;
  }

  const volatile char* memory_;
  uint32_t memory_size_;
  uint32_t bytes_read_ = 0u;
};
}  // namespace

class ServiceFontManager::SkiaDiscardableManager
    : public SkStrikeClient::DiscardableHandleManager {
 public:
  SkiaDiscardableManager(scoped_refptr<ServiceFontManager> font_manager)
      : font_manager_(std::move(font_manager)) {}
  ~SkiaDiscardableManager() override = default;

  bool deleteHandle(SkDiscardableHandleId handle_id) override {
    if (!font_manager_)
      return true;
    return font_manager_->DeleteHandle(handle_id);
  }

  void notifyCacheMiss(SkStrikeClient::CacheMissType type) override {
    UMA_HISTOGRAM_ENUMERATION("GPU.OopRaster.GlyphCacheMiss", type,
                              SkStrikeClient::CacheMissType::kLast + 1);
    // In general, Skia analysis of glyphs should find all cases.
    // If this is not happening, please file a bug with a repro so
    // it can be fixed.
    NOTREACHED();

    const bool no_fallback = (type == SkStrikeClient::kGlyphMetrics ||
                              type == SkStrikeClient::kGlyphPath ||
                              type == SkStrikeClient::kGlyphImage);

    constexpr int kMaxDumps = 5;
    if (no_fallback && dump_count_ < kMaxDumps && base::RandInt(1, 100) == 1) {
      ++dump_count_;
      base::debug::DumpWithoutCrashing();
    }
  }

 private:
  int dump_count_ = 0;
  scoped_refptr<ServiceFontManager> font_manager_;
};

ServiceFontManager::ServiceFontManager(Client* client)
    : client_(client),
      strike_client_(std::make_unique<SkStrikeClient>(
          sk_make_sp<SkiaDiscardableManager>(this))) {}

ServiceFontManager::~ServiceFontManager() {
  DCHECK(destroyed_);
}

void ServiceFontManager::Destroy() {
  base::AutoLock hold(lock_);

  client_ = nullptr;
  strike_client_.reset();
  discardable_handle_map_.clear();
  destroyed_ = true;
}

bool ServiceFontManager::Deserialize(
    const volatile char* memory,
    uint32_t memory_size,
    std::vector<SkDiscardableHandleId>* locked_handles) {
  base::AutoLock hold(lock_);

  DCHECK(locked_handles->empty());
  DCHECK(!destroyed_);

  // All new handles.
  Deserializer deserializer(memory, memory_size);
  uint32_t new_handles_created;
  if (!deserializer.Read<uint32_t>(&new_handles_created))
    return false;

  for (uint32_t i = 0; i < new_handles_created; ++i) {
    SerializableSkiaHandle handle;
    if (!deserializer.Read<SerializableSkiaHandle>(&handle))
      return false;

    scoped_refptr<gpu::Buffer> buffer = client_->GetShmBuffer(handle.shm_id);
    if (!DiscardableHandleBase::ValidateParameters(buffer.get(),
                                                   handle.byte_offset))
      return false;

    if (!AddHandle(handle.handle_id,
                   ServiceDiscardableHandle(
                       std::move(buffer), handle.byte_offset, handle.shm_id))) {
      return false;
    }
  }

  // All locked handles
  uint32_t num_locked_handles;
  if (!deserializer.Read<uint32_t>(&num_locked_handles))
    return false;

  // Loosely avoid extremely large (but fake) numbers of locked handles.
  if (memory_size / sizeof(SkDiscardableHandleId) < num_locked_handles)
    return false;

  locked_handles->resize(num_locked_handles);
  for (uint32_t i = 0; i < num_locked_handles; ++i) {
    if (!deserializer.Read<SkDiscardableHandleId>(&locked_handles->at(i)))
      return false;
  }

  // Skia font data.
  uint32_t skia_data_size = 0u;
  if (!deserializer.Read<uint32_t>(&skia_data_size))
    return false;

  {
    base::AutoUnlock release(lock_);
    if (!deserializer.ReadStrikeData(strike_client_.get(), skia_data_size))
      return false;
  }

  return true;
}

bool ServiceFontManager::AddHandle(SkDiscardableHandleId handle_id,
                                   ServiceDiscardableHandle handle) {
  lock_.AssertAcquired();

  if (discardable_handle_map_.find(handle_id) != discardable_handle_map_.end())
    return false;
  discardable_handle_map_[handle_id] = std::move(handle);
  return true;
}

bool ServiceFontManager::Unlock(
    const std::vector<SkDiscardableHandleId>& handles) {
  base::AutoLock hold(lock_);
  DCHECK(!destroyed_);

  for (auto handle_id : handles) {
    auto it = discardable_handle_map_.find(handle_id);
    if (it == discardable_handle_map_.end())
      return false;
    it->second.Unlock();
  }
  return true;
}

bool ServiceFontManager::DeleteHandle(SkDiscardableHandleId handle_id) {
  base::AutoLock hold(lock_);

  if (destroyed_)
    return true;

  auto it = discardable_handle_map_.find(handle_id);
  if (it == discardable_handle_map_.end()) {
    LOG(ERROR) << "Tried to delete invalid SkDiscardableHandleId: "
               << handle_id;
    return true;
  }

  bool deleted = it->second.Delete();
  if (!deleted)
    return false;

  discardable_handle_map_.erase(it);
  return true;
}

}  // namespace gpu
