// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
#pragma once

#if defined(ENABLE_GPU)

#include <vector>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

class GpuCommandBufferStubBase;

class CONTENT_EXPORT GpuMemoryManagerClient {
public:
  virtual ~GpuMemoryManagerClient() {}

  virtual void AppendAllCommandBufferStubs(
      std::vector<GpuCommandBufferStubBase*>& stubs) = 0;
};

class CONTENT_EXPORT GpuMemoryManager :
    public base::SupportsWeakPtr<GpuMemoryManager> {
 public:
  enum { kDefaultMaxSurfacesWithFrontbufferSoftLimit = 8 };

  // These are predefined values (in bytes) for
  // GpuMemoryAllocation::gpuResourceSizeInBytes.
  // Maximum Allocation for all tabs is a soft limit that can be exceeded
  // during the time it takes for renderers to respect new allocations,
  // including when switching tabs or opening a new window.
  // To alleviate some pressure, we decrease our desired limit by "one tabs'
  // worth" of memory.
  enum {
#if defined(OS_ANDROID)
    kMinimumAllocationForTab = 32 * 1024 * 1024,
    kMaximumAllocationForTabs = 64 * 1024 * 1024,
#else
    kMinimumAllocationForTab = 64 * 1024 * 1024,
    kMaximumAllocationForTabs = 512 * 1024 * 1024 - kMinimumAllocationForTab,
#endif
  };

  GpuMemoryManager(GpuMemoryManagerClient* client,
                   size_t max_surfaces_with_frontbuffer_soft_limit);
  ~GpuMemoryManager();

  // Schedule a Manage() call. If immediate is true, we PostTask without delay.
  // Otherwise PostDelayedTask using a CancelableClosure and allow multiple
  // delayed calls to "queue" up. This way, we do not spam clients in certain
  // lower priority situations. An immediate schedule manage will cancel any
  // queued delayed manage.
  void ScheduleManage(bool immediate);

 private:
  friend class GpuMemoryManagerTest;
  void Manage();

  class CONTENT_EXPORT StubWithSurfaceComparator {
   public:
    bool operator()(GpuCommandBufferStubBase* lhs,
                    GpuCommandBufferStubBase* rhs);
  };

  GpuMemoryManagerClient* client_;

  base::CancelableClosure delayed_manage_callback_;
  bool manage_immediate_scheduled_;

  size_t max_surfaces_with_frontbuffer_soft_limit_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryManager);
};

#endif

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
