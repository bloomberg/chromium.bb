// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <unordered_map>

#include "base/atomic_sequence_num.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_math.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/resources/resource_util.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "skia/ext/texture_handle.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gl/trace_util.h"

using gpu::gles2::GLES2Interface;

namespace cc {

namespace {

// Generates process-unique IDs to use for tracing a ResourceProvider's
// resources.
base::AtomicSequenceNumber g_next_resource_provider_tracing_id;

}  // namespace

ResourceProvider::ResourceProvider(
    viz::ContextProvider* compositor_context_provider)
    : compositor_context_provider_(compositor_context_provider),
      next_child_(1),
      lost_context_provider_(false),
      tracing_id_(g_next_resource_provider_tracing_id.GetNext()) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  // TODO(ericrk): Get this working in Android Webview. crbug.com/517156
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::ResourceProvider", base::ThreadTaskRunnerHandle::Get());
  }

  if (!compositor_context_provider)
    return;
}

ResourceProvider::~ResourceProvider() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  while (!resources_.empty())
    DeleteResourceInternal(resources_.begin(), FOR_SHUTDOWN);

  if (!compositor_context_provider_)
    return;

#if DCHECK_IS_ON()
  // Check that all GL resources has been deleted.
  for (const auto& pair : resources_)
    DCHECK(!pair.second.is_gpu_resource_type());
#endif  // DCHECK_IS_ON()
}

bool ResourceProvider::IsLost(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  return resource->lost;
}

void ResourceProvider::LoseResourceForTesting(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  DCHECK(resource);
  resource->lost = true;
}

void ResourceProvider::DeleteResourceInternal(ResourceMap::iterator it,
                                              DeleteStyle style) {
  TRACE_EVENT0("cc", "ResourceProvider::DeleteResourceInternal");
  viz::internal::Resource* resource = &it->second;
  DCHECK(resource->exported_count == 0 || style != NORMAL);

#if defined(OS_ANDROID)
  // If this resource was interested in promotion hints, then remove it from
  // the set of resources that we'll notify.
  if (resource->wants_promotion_hint)
    wants_promotion_hints_set_.erase(it->first);
#endif

  // Exported resources are lost on shutdown.
  bool exported_resource_lost =
      style == FOR_SHUTDOWN && resource->exported_count > 0;
  // GPU resources are lost when context is lost.
  bool gpu_resource_lost =
      resource->is_gpu_resource_type() && lost_context_provider_;
  bool lost_resource =
      resource->lost || exported_resource_lost || gpu_resource_lost;

  // Wait on sync token before deleting resources we own.
  if (!lost_resource && resource->origin == viz::internal::Resource::INTERNAL)
    WaitSyncTokenInternal(resource);

  if (resource->image_id) {
    DCHECK_EQ(resource->origin, viz::internal::Resource::INTERNAL);
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->DestroyImageCHROMIUM(resource->image_id);
  }

  if (resource->gl_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->DeleteTextures(1, &resource->gl_id);
    resource->gl_id = 0;
  }

  if (resource->owned_shared_bitmap) {
    DCHECK_EQ(viz::ResourceType::kBitmap, resource->type);
    resource->shared_bitmap = nullptr;
    resource->pixels = nullptr;
    resource->owned_shared_bitmap = nullptr;
  }

  if (resource->gpu_memory_buffer) {
    DCHECK_EQ(viz::ResourceType::kGpuMemoryBuffer, resource->type);
    resource->gpu_memory_buffer = nullptr;
  }

  resources_.erase(it);
}

GLenum ResourceProvider::GetResourceTextureTarget(viz::ResourceId id) {
  return GetResource(id)->target;
}

viz::internal::Resource* ResourceProvider::InsertResource(
    viz::ResourceId id,
    viz::internal::Resource resource) {
  std::pair<ResourceMap::iterator, bool> result =
      resources_.insert(ResourceMap::value_type(id, std::move(resource)));
  DCHECK(result.second);
  return &result.first->second;
}

viz::internal::Resource* ResourceProvider::GetResource(viz::ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(id);
  ResourceMap::iterator it = resources_.find(id);
  DCHECK(it != resources_.end());
  return &it->second;
}

void ResourceProvider::EnableReadLockFencesForTesting(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  DCHECK(resource);
  resource->read_lock_fences_enabled = true;
}

void ResourceProvider::PopulateSkBitmapWithResource(
    SkBitmap* sk_bitmap,
    const viz::internal::Resource* resource) {
  DCHECK_EQ(viz::RGBA_8888, resource->format);
  SkImageInfo info = SkImageInfo::MakeN32Premul(resource->size.width(),
                                                resource->size.height());
  sk_bitmap->installPixels(info, resource->pixels, info.minRowBytes());
}

void ResourceProvider::WaitSyncTokenInternal(
    viz::internal::Resource* resource) {
  DCHECK(resource);
  if (!resource->ShouldWaitSyncToken())
    return;
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  // In the case of context lost, this sync token may be empty (see comment in
  // the UpdateSyncToken() function). The WaitSyncTokenCHROMIUM() function
  // handles empty sync tokens properly so just wait anyways and update the
  // state the synchronized.
  gl->WaitSyncTokenCHROMIUM(resource->sync_token().GetConstData());
  resource->SetSynchronized();
}

GLES2Interface* ResourceProvider::ContextGL() const {
  viz::ContextProvider* context_provider = compositor_context_provider_;
  return context_provider ? context_provider->ContextGL() : nullptr;
}

bool ResourceProvider::IsGLContextLost() const {
  return ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

bool ResourceProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const uint64_t tracing_process_id =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->GetTracingProcessId();

  for (const auto& resource_entry : resources_) {
    const auto& resource = resource_entry.second;

    bool backing_memory_allocated = false;
    switch (resource.type) {
      case viz::ResourceType::kGpuMemoryBuffer:
        backing_memory_allocated = !!resource.gpu_memory_buffer;
        break;
      case viz::ResourceType::kTexture:
        backing_memory_allocated = !!resource.gl_id;
        break;
      case viz::ResourceType::kBitmap:
        backing_memory_allocated = resource.has_shared_bitmap_id;
        break;
    }

    if (!backing_memory_allocated) {
      // Don't log unallocated resources - they have no backing memory.
      continue;
    }

    // ResourceIds are not process-unique, so log with the ResourceProvider's
    // unique id.
    std::string dump_name =
        base::StringPrintf("cc/resource_memory/provider_%d/resource_%d",
                           tracing_id_, resource_entry.first);
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(dump_name);

    uint64_t total_bytes = ResourceUtil::UncheckedSizeInBytesAligned<size_t>(
        resource.size, resource.format);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    static_cast<uint64_t>(total_bytes));

    // Resources may be shared across processes and require a shared GUID to
    // prevent double counting the memory.
    base::trace_event::MemoryAllocatorDumpGuid guid;
    base::UnguessableToken shared_memory_guid;
    switch (resource.type) {
      case viz::ResourceType::kGpuMemoryBuffer:
        guid =
            resource.gpu_memory_buffer->GetGUIDForTracing(tracing_process_id);
        shared_memory_guid =
            resource.gpu_memory_buffer->GetHandle().handle.GetGUID();
        break;
      case viz::ResourceType::kTexture:
        DCHECK(resource.gl_id);
        guid = gl::GetGLTextureClientGUIDForTracing(
            compositor_context_provider_->ContextSupport()
                ->ShareGroupTracingGUID(),
            resource.gl_id);
        break;
      case viz::ResourceType::kBitmap:
        DCHECK(resource.has_shared_bitmap_id);
        guid = viz::GetSharedBitmapGUIDForTracing(resource.shared_bitmap_id);
        if (resource.shared_bitmap) {
          shared_memory_guid =
              resource.shared_bitmap->GetSharedMemoryHandle().GetGUID();
        }
        break;
    }

    DCHECK(!guid.empty());

    const int kImportance = 2;
    if (!shared_memory_guid.is_empty()) {
      pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), shared_memory_guid,
                                           kImportance);
    } else {
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  }

  return true;
}

}  // namespace cc
