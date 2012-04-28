// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
#pragma once

#if defined(ENABLE_GPU)

#include <vector>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

class GpuCommandBufferStubBase;

class CONTENT_EXPORT GpuMemoryManagerClient {
public:
  virtual ~GpuMemoryManagerClient() {}

  virtual void AppendAllCommandBufferStubs(
      std::vector<GpuCommandBufferStubBase*>& stubs) = 0;
};

class CONTENT_EXPORT GpuMemoryManager {
 public:
  enum { kDefaultMaxSurfacesWithFrontbufferSoftLimit = 8 };

  GpuMemoryManager(GpuMemoryManagerClient* client,
                   size_t max_surfaces_with_frontbuffer_soft_limit);
  ~GpuMemoryManager();

  void ScheduleManage();

 private:
  friend class GpuMemoryManagerTest;
  void Manage();

  class CONTENT_EXPORT StubWithSurfaceComparator {
   public:
    bool operator()(GpuCommandBufferStubBase* lhs,
                    GpuCommandBufferStubBase* rhs);
  };

  GpuMemoryManagerClient* client_;
  bool manage_scheduled_;
  size_t max_surfaces_with_frontbuffer_soft_limit_;
  base::WeakPtrFactory<GpuMemoryManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryManager);
};

#endif

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
