// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_discardable_manager.h"

#include "base/memory/singleton.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/texture_manager.h"

namespace gpu {
namespace {
size_t DiscardableCacheSizeLimit() {
// Cache size values are designed to roughly correspond to existing image cache
// sizes for 1-1.5 renderers. These will be updated as more types of data are
// moved to this cache.
#if defined(OS_ANDROID)
  const size_t kLowEndCacheSizeBytes = 512 * 1024;
  const size_t kNormalCacheSizeBytes = 128 * 1024 * 1024;
#else
  const size_t kNormalCacheSizeBytes = 192 * 1024 * 1024;
  const size_t kLargeCacheSizeBytes = 256 * 1024 * 1024;
  // Device ram threshold at which we move from a normal cache to a large cache.
  // While this is a GPU memory cache, we can't read GPU memory reliably, so we
  // use system ram as a proxy.
  const int kLargeCacheSizeMemoryThresholdMB = 4 * 1024;
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
  if (base::SysInfo::IsLowEndDevice()) {
    return kLowEndCacheSizeBytes;
  } else {
    return kNormalCacheSizeBytes;
  }
#else
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <
      kLargeCacheSizeMemoryThresholdMB) {
    return kNormalCacheSizeBytes;
  } else {
    return kLargeCacheSizeBytes;
  }
#endif
}

}  // namespace

ServiceDiscardableManager::GpuDiscardableEntry::GpuDiscardableEntry(
    ServiceDiscardableHandle handle,
    size_t size)
    : handle(handle), size(size) {}
ServiceDiscardableManager::GpuDiscardableEntry::GpuDiscardableEntry(
    const GpuDiscardableEntry& other) = default;
ServiceDiscardableManager::GpuDiscardableEntry::GpuDiscardableEntry(
    GpuDiscardableEntry&& other) = default;
ServiceDiscardableManager::GpuDiscardableEntry::~GpuDiscardableEntry() =
    default;

ServiceDiscardableManager::ServiceDiscardableManager()
    : entries_(EntryCache::NO_AUTO_EVICT),
      cache_size_limit_(DiscardableCacheSizeLimit()) {}

ServiceDiscardableManager::~ServiceDiscardableManager() {
#if DCHECK_IS_ON()
  for (const auto& entry : entries_) {
    DCHECK(nullptr == entry.second.unlocked_texture_ref);
  }
#endif
}

void ServiceDiscardableManager::InsertLockedTexture(
    uint32_t texture_id,
    size_t texture_size,
    gles2::TextureManager* texture_manager,
    ServiceDiscardableHandle handle) {
  auto found = entries_.Get({texture_id, texture_manager});
  if (found != entries_.end()) {
    // We have somehow initialized a texture twice. The client *shouldn't* send
    // this command, but if it does, we will clean up the old entry and use
    // the new one.
    total_size_ -= found->second.size;
    if (found->second.unlocked_texture_ref) {
      texture_manager->ReturnTexture(
          std::move(found->second.unlocked_texture_ref));
    }
    entries_.Erase(found);
  }

  total_size_ += texture_size;
  entries_.Put({texture_id, texture_manager},
               GpuDiscardableEntry{handle, texture_size});
  EnforceCacheSizeLimit(cache_size_limit_);
}

bool ServiceDiscardableManager::UnlockTexture(
    uint32_t texture_id,
    gles2::TextureManager* texture_manager,
    gles2::TextureRef** texture_to_unbind) {
  *texture_to_unbind = nullptr;

  auto found = entries_.Get({texture_id, texture_manager});
  if (found == entries_.end())
    return false;

  found->second.handle.Unlock();
  if (--found->second.service_ref_count_ == 0) {
    found->second.unlocked_texture_ref =
        texture_manager->TakeTexture(texture_id);
    *texture_to_unbind = found->second.unlocked_texture_ref.get();
  }

  return true;
}

bool ServiceDiscardableManager::LockTexture(
    uint32_t texture_id,
    gles2::TextureManager* texture_manager) {
  auto found = entries_.Peek({texture_id, texture_manager});
  if (found == entries_.end())
    return false;

  ++found->second.service_ref_count_;
  if (found->second.unlocked_texture_ref) {
    texture_manager->ReturnTexture(
        std::move(found->second.unlocked_texture_ref));
  }

  return true;
}

void ServiceDiscardableManager::OnTextureManagerDestruction(
    gles2::TextureManager* texture_manager) {
  for (auto& entry : entries_) {
    if (entry.first.texture_manager == texture_manager &&
        entry.second.unlocked_texture_ref) {
      texture_manager->ReturnTexture(
          std::move(entry.second.unlocked_texture_ref));
    }
  }
}

void ServiceDiscardableManager::OnTextureDeleted(
    uint32_t texture_id,
    gles2::TextureManager* texture_manager) {
  auto found = entries_.Get({texture_id, texture_manager});
  if (found == entries_.end())
    return;

  found->second.handle.ForceDelete();
  total_size_ -= found->second.size;
  entries_.Erase(found);
}

void ServiceDiscardableManager::OnTextureSizeChanged(
    uint32_t texture_id,
    gles2::TextureManager* texture_manager,
    size_t new_size) {
  auto found = entries_.Get({texture_id, texture_manager});
  if (found == entries_.end())
    return;

  total_size_ -= found->second.size;
  found->second.size = new_size;
  total_size_ += found->second.size;

  EnforceCacheSizeLimit(cache_size_limit_);
}

void ServiceDiscardableManager::HandleMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  size_t limit = 0;
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      // This function is only called with moderate or critical pressure.
      NOTREACHED();
      return;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      // With moderate pressure, shrink to 1/4 our normal size.
      limit = cache_size_limit_ / 4;
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // With critical pressure, purge as much as possible.
      limit = 0;
      break;
  }

  EnforceCacheSizeLimit(limit);
}

void ServiceDiscardableManager::EnforceCacheSizeLimit(size_t limit) {
  for (auto it = entries_.rbegin(); it != entries_.rend();) {
    if (total_size_ <= limit) {
      return;
    }
    if (!it->second.handle.Delete()) {
      ++it;
      continue;
    }

    total_size_ -= it->second.size;

    gles2::TextureManager* texture_manager = it->first.texture_manager;
    uint32_t texture_id = it->first.texture_id;

    // While unlocked, we hold the texture ref. Return this to the texture
    // manager for cleanup.
    texture_manager->ReturnTexture(std::move(it->second.unlocked_texture_ref));

    // Erase before calling texture_manager->RemoveTexture, to avoid attempting
    // to remove the texture from entries_ twice.
    it = entries_.Erase(it);
    texture_manager->RemoveTexture(texture_id);
  }
}

bool ServiceDiscardableManager::IsEntryLockedForTesting(
    uint32_t texture_id,
    gles2::TextureManager* texture_manager) const {
  auto found = entries_.Peek({texture_id, texture_manager});
  DCHECK(found != entries_.end());

  return found->second.handle.IsLockedForTesting();
}

gles2::TextureRef* ServiceDiscardableManager::UnlockedTextureRefForTesting(
    uint32_t texture_id,
    gles2::TextureManager* texture_manager) const {
  auto found = entries_.Peek({texture_id, texture_manager});
  DCHECK(found != entries_.end());

  return found->second.unlocked_texture_ref.get();
}

}  // namespace gpu
