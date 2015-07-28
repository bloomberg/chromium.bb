// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/renderer_resource.h"

#include "base/basictypes.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/process_resource_usage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager_util.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/service_registry.h"

namespace task_manager {

RendererResource::RendererResource(base::ProcessHandle process,
                                   content::RenderViewHost* render_view_host)
    : process_(process), render_view_host_(render_view_host) {
  // We cache the process and pid as when a Tab/BackgroundContents is closed the
  // process reference becomes NULL and the TaskManager still needs it.
  unique_process_id_ = render_view_host_->GetProcess()->GetID();
  ResourceUsageReporterPtr service;
  content::ServiceRegistry* service_registry =
      render_view_host_->GetProcess()->GetServiceRegistry();
  if (service_registry)
    service_registry->ConnectToRemoteService(mojo::GetProxy(&service));
  process_resource_usage_.reset(new ProcessResourceUsage(service.Pass()));
}

RendererResource::~RendererResource() {
}

void RendererResource::Refresh() {
  process_resource_usage_->Refresh(base::Closure());
}

blink::WebCache::ResourceTypeStats
RendererResource::GetWebCoreCacheStats() const {
  return process_resource_usage_->GetWebCoreCacheStats();
}

size_t RendererResource::GetV8MemoryAllocated() const {
  return process_resource_usage_->GetV8MemoryAllocated();
}

size_t RendererResource::GetV8MemoryUsed() const {
  return process_resource_usage_->GetV8MemoryUsed();
}

base::string16 RendererResource::GetProfileName() const {
  return util::GetProfileNameFromInfoCache(Profile::FromBrowserContext(
      render_view_host_->GetProcess()->GetBrowserContext()));
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

bool RendererResource::ReportsV8MemoryStats() const {
  return true;
}

bool RendererResource::SupportNetworkUsage() const {
  return true;
}

}  // namespace task_manager
