// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_RENDERER_RESOURCE_H_
#define CHROME_BROWSER_TASK_MANAGER_RENDERER_RESOURCE_H_

#include "base/basictypes.h"
#include "chrome/browser/task_manager/resource_provider.h"

namespace content {
class RenderViewHost;
}

namespace task_manager {

// Base class for various types of render process resources that provides common
// functionality like stats tracking.
class RendererResource : public Resource {
 public:
  RendererResource(base::ProcessHandle process,
                   content::RenderViewHost* render_view_host);
  virtual ~RendererResource();

  // Resource methods:
  virtual base::string16 GetProfileName() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual int GetUniqueChildProcessId() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual int GetRoutingID() const OVERRIDE;

  virtual bool ReportsCacheStats() const OVERRIDE;
  virtual blink::WebCache::ResourceTypeStats GetWebCoreCacheStats() const
      OVERRIDE;
  virtual bool ReportsV8MemoryStats() const OVERRIDE;
  virtual size_t GetV8MemoryAllocated() const OVERRIDE;
  virtual size_t GetV8MemoryUsed() const OVERRIDE;

  // RenderResources always provide the network usage.
  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE { }

  virtual void Refresh() OVERRIDE;

  virtual void NotifyResourceTypeStats(
      const blink::WebCache::ResourceTypeStats& stats) OVERRIDE;

  virtual void NotifyV8HeapStats(size_t v8_memory_allocated,
                                 size_t v8_memory_used) OVERRIDE;

  content::RenderViewHost* render_view_host() const {
    return render_view_host_;
  }

 private:
  base::ProcessHandle process_;
  int pid_;
  int unique_process_id_;

  // RenderViewHost we use to fetch stats.
  content::RenderViewHost* render_view_host_;
  // The stats_ field holds information about resource usage in the renderer
  // process and so it is updated asynchronously by the Refresh() call.
  blink::WebCache::ResourceTypeStats stats_;
  // This flag is true if we are waiting for the renderer to report its stats.
  bool pending_stats_update_;

  // We do a similar dance to gather the V8 memory usage in a process.
  size_t v8_memory_allocated_;
  size_t v8_memory_used_;
  bool pending_v8_memory_allocated_update_;

  DISALLOW_COPY_AND_ASSIGN(RendererResource);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_RENDERER_RESOURCE_H_
