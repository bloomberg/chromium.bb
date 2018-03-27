// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_PROVIDER_H_
#define CC_RESOURCES_RESOURCE_PROVIDER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/small_map.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/cc_export.h"
#include "cc/resources/return_callback.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/resource.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace viz {
class ContextProvider;
}  // namespace viz

namespace cc {

// This class provides abstractions for allocating and transferring resources
// between modules/threads/processes. It abstracts away GL textures vs
// GpuMemoryBuffers vs software bitmaps behind a single ResourceId so that
// code in common can hold onto ResourceIds, as long as the code using them
// knows the correct type.
//
// The resource's underlying type is accessed through Read and Write locks that
// help to safeguard correct usage with DCHECKs. All resources held in
// ResourceProvider are immutable - they can not change format or size once
// they are created, only their contents.
//
// This class is not thread-safe and can only be called from the thread it was
// created on (in practice, the impl thread).
class CC_EXPORT ResourceProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  using ResourceIdArray = std::vector<viz::ResourceId>;
  using ResourceIdMap = std::unordered_map<viz::ResourceId, viz::ResourceId>;

  explicit ResourceProvider(viz::ContextProvider* compositor_context_provider);
  ~ResourceProvider() override;

  bool IsSoftware() const { return !compositor_context_provider_; }

  void DidLoseContextProvider() { lost_context_provider_ = true; }

  size_t num_resources() const { return resources_.size(); }

  GLenum GetResourceTextureTarget(viz::ResourceId id);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  int tracing_id() const { return tracing_id_; }

 protected:
  using ResourceMap =
      std::unordered_map<viz::ResourceId, viz::internal::Resource>;

  viz::internal::Resource* InsertResource(viz::ResourceId id,
                                          viz::internal::Resource resource);
  viz::internal::Resource* GetResource(viz::ResourceId id);

  // TODO(ericrk): TryGetResource is part of a temporary workaround for cases
  // where resources which should be available are missing. This version may
  // return nullptr if a resource is not found. https://crbug.com/811858
  viz::internal::Resource* TryGetResource(viz::ResourceId id);

  void PopulateSkBitmapWithResource(SkBitmap* sk_bitmap,
                                    const viz::internal::Resource* resource);

  enum DeleteStyle {
    NORMAL,
    FOR_SHUTDOWN,
  };

  void DeleteResourceInternal(ResourceMap::iterator it, DeleteStyle style);

  void WaitSyncTokenInternal(viz::internal::Resource* resource);

  // Returns null if we do not have a viz::ContextProvider.
  gpu::gles2::GLES2Interface* ContextGL() const;

  ResourceMap resources_;

  viz::ContextProvider* compositor_context_provider_;

  bool lost_context_provider_;

  THREAD_CHECKER(thread_checker_);

 private:
  // A process-unique ID used for disambiguating memory dumps from different
  // resource providers.
  int tracing_id_;

  DISALLOW_COPY_AND_ASSIGN(ResourceProvider);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_PROVIDER_H_
