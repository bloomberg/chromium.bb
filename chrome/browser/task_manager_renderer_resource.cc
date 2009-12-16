// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager_renderer_resource.h"

#include "base/basictypes.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/common/render_messages.h"

////////////////////////////////////////////////////////////////////////////////
// TaskManagerTabContentsResource class
////////////////////////////////////////////////////////////////////////////////

TaskManagerRendererResource::TaskManagerRendererResource(
    RenderWidgetHost* render_widget_host)
    : render_widget_host_(render_widget_host),
      pending_stats_update_(false),
      v8_memory_allocated_(0),
      v8_memory_used_(0),
      pending_v8_memory_allocated_update_(false) {
  // We cache the process as when the TabContents is closed the process
  // becomes NULL and the TaskManager still needs it.
  stats_.images.size = 0;
  stats_.cssStyleSheets.size = 0;
  stats_.scripts.size = 0;
  stats_.xslStyleSheets.size = 0;
  stats_.fonts.size = 0;
}

TaskManagerRendererResource::~TaskManagerRendererResource() {
}

void TaskManagerRendererResource::Refresh() {
  if (!pending_stats_update_) {
    render_widget_host_->Send(new ViewMsg_GetCacheResourceStats);
    pending_stats_update_ = true;
  }
  if (!pending_v8_memory_allocated_update_) {
    render_widget_host_->Send(new ViewMsg_GetV8HeapStats);
    pending_v8_memory_allocated_update_ = true;
  }
}

WebKit::WebCache::ResourceTypeStats
    TaskManagerRendererResource::GetWebCoreCacheStats() const {
  return stats_;
}

size_t TaskManagerRendererResource::GetV8MemoryAllocated() const {
  return v8_memory_allocated_;
}

size_t TaskManagerRendererResource::GetV8MemoryUsed() const {
  return v8_memory_used_;
}

void TaskManagerRendererResource::NotifyResourceTypeStats(
    const WebKit::WebCache::ResourceTypeStats& stats) {
  stats_ = stats;
  pending_stats_update_ = false;
}

void TaskManagerRendererResource::NotifyV8HeapStats(
    size_t v8_memory_allocated, size_t v8_memory_used) {
  v8_memory_allocated_ = v8_memory_allocated;
  v8_memory_used_ = v8_memory_used;
  pending_v8_memory_allocated_update_ = false;
}

