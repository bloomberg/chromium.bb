// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_RENDERER_RESOURCE_H_
#define CHROME_BROWSER_TASK_MANAGER_RENDERER_RESOURCE_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "chrome/browser/task_manager/resource_provider.h"

class ProcessResourceUsage;

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
  ~RendererResource() override;

  // Resource methods:
  base::string16 GetProfileName() const override;
  base::ProcessHandle GetProcess() const override;
  int GetUniqueChildProcessId() const override;
  Type GetType() const override;
  int GetRoutingID() const override;

  bool ReportsCacheStats() const override;
  blink::WebCache::ResourceTypeStats GetWebCoreCacheStats() const override;
  bool ReportsV8MemoryStats() const override;
  size_t GetV8MemoryAllocated() const override;
  size_t GetV8MemoryUsed() const override;

  // RenderResources always provide the network usage.
  bool SupportNetworkUsage() const override;
  void SetSupportNetworkUsage() override {}

  void Refresh() override;

  content::RenderViewHost* render_view_host() const {
    return render_view_host_;
  }

 private:
  base::ProcessHandle process_;
  int unique_process_id_;

  // RenderViewHost we use to fetch stats.
  content::RenderViewHost* render_view_host_;

  std::unique_ptr<ProcessResourceUsage> process_resource_usage_;

  DISALLOW_COPY_AND_ASSIGN(RendererResource);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_RENDERER_RESOURCE_H_
