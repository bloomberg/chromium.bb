// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/renderer_resource.h"

#include "base/basictypes.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"

namespace task_manager {

RendererResource::RendererResource(base::ProcessHandle process,
                                   content::RenderViewHost* render_view_host)
    : process_(process),
      render_view_host_(render_view_host),
      pending_stats_update_(false),
      fps_(0.0f),
      pending_fps_update_(false),
      v8_memory_allocated_(0),
      v8_memory_used_(0),
      pending_v8_memory_allocated_update_(false) {
  // We cache the process and pid as when a Tab/BackgroundContents is closed the
  // process reference becomes NULL and the TaskManager still needs it.
  pid_ = base::GetProcId(process_);
  unique_process_id_ = render_view_host_->GetProcess()->GetID();
  memset(&stats_, 0, sizeof(stats_));
}

RendererResource::~RendererResource() {
}

void RendererResource::Refresh() {
  if (!pending_stats_update_) {
    render_view_host_->Send(new ChromeViewMsg_GetCacheResourceStats);
    pending_stats_update_ = true;
  }
  if (!pending_fps_update_) {
    render_view_host_->Send(
        new ChromeViewMsg_GetFPS(render_view_host_->GetRoutingID()));
    pending_fps_update_ = true;
  }
  if (!pending_v8_memory_allocated_update_) {
    render_view_host_->Send(new ChromeViewMsg_GetV8HeapStats);
    pending_v8_memory_allocated_update_ = true;
  }
}

WebKit::WebCache::ResourceTypeStats
RendererResource::GetWebCoreCacheStats() const {
  return stats_;
}

float RendererResource::GetFPS() const {
  return fps_;
}

size_t RendererResource::GetV8MemoryAllocated() const {
  return v8_memory_allocated_;
}

size_t RendererResource::GetV8MemoryUsed() const {
  return v8_memory_used_;
}

void RendererResource::NotifyResourceTypeStats(
    const WebKit::WebCache::ResourceTypeStats& stats) {
  stats_ = stats;
  pending_stats_update_ = false;
}

void RendererResource::NotifyFPS(float fps) {
  fps_ = fps;
  pending_fps_update_ = false;
}

void RendererResource::NotifyV8HeapStats(
    size_t v8_memory_allocated, size_t v8_memory_used) {
  v8_memory_allocated_ = v8_memory_allocated;
  v8_memory_used_ = v8_memory_used;
  pending_v8_memory_allocated_update_ = false;
}

base::ProcessHandle RendererResource::GetProcess() const {
  return process_;
}

int RendererResource::GetUniqueChildProcessId() const {
  return unique_process_id_;
}

Resource::Type RendererResource::GetType() const {
  return RENDERER;
}

int RendererResource::GetRoutingID() const {
  return render_view_host_->GetRoutingID();
}

bool RendererResource::ReportsCacheStats() const {
  return true;
}

bool RendererResource::ReportsFPS() const {
  return true;
}

bool RendererResource::ReportsV8MemoryStats() const {
  return true;
}

bool RendererResource::CanInspect() const {
  return true;
}

void RendererResource::Inspect() const {
  DevToolsWindow::OpenDevToolsWindow(render_view_host_);
}

bool RendererResource::SupportNetworkUsage() const {
  return true;
}

}  // namespace task_manager
