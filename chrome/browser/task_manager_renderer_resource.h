// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_RENDERER_RESOURCE_H_
#define CHROME_BROWSER_TASK_MANAGER_RENDERER_RESOURCE_H_

#include "base/basictypes.h"
#include "chrome/browser/task_manager.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCache.h"

// Functionality shared by all TaskManager::Resource classes that represent
// a render process (TabContents and Extensions).

class RenderWidgetHost;

class TaskManagerRendererResource : public TaskManager::Resource {
 public:
  explicit TaskManagerRendererResource(RenderWidgetHost* render_widget_host);

  virtual ~TaskManagerRendererResource();

  virtual bool ReportsCacheStats() const { return true; }
  virtual WebKit::WebCache::ResourceTypeStats GetWebCoreCacheStats() const;

  virtual bool ReportsV8MemoryStats() const { return true; }
  virtual size_t GetV8MemoryAllocated() const;
  virtual size_t GetV8MemoryUsed() const;

  // TabContents always provide the network usage.
  bool SupportNetworkUsage() const { return true; }
  void SetSupportNetworkUsage() { }

  virtual void Refresh();

  virtual void NotifyResourceTypeStats(
      const WebKit::WebCache::ResourceTypeStats& stats);

  virtual void NotifyV8HeapStats(size_t v8_memory_allocated,
                                 size_t v8_memory_used);

 private:
  RenderWidgetHost* render_widget_host_;

  // The stats_ field holds information about resource usage in the renderer
  // process and so it is updated asynchronously by the Refresh() call.
  WebKit::WebCache::ResourceTypeStats stats_;

  // This flag is true if we are waiting for the renderer to report its stats.
  bool pending_stats_update_;

  // We do a similar dance to gather the V8 memory usage in a process.
  size_t v8_memory_allocated_;
  size_t v8_memory_used_;
  bool pending_v8_memory_allocated_update_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerRendererResource);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_RENDERER_RESOURCE_H_

