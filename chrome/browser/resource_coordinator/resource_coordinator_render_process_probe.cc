// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/resource_coordinator_render_process_probe.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/service_manager_connection.h"
#include "services/resource_coordinator/public/cpp/process_resource_coordinator.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

#if defined(OS_MACOSX)
#include "content/public/browser/browser_child_process_host.h"
#endif

namespace resource_coordinator {

namespace {

const int kDefaultMeasurementIntervalInSeconds = 1;

base::LazyInstance<ResourceCoordinatorRenderProcessProbe>::DestructorAtExit
    g_probe = LAZY_INSTANCE_INITIALIZER;

}  // namespace

RenderProcessInfo::RenderProcessInfo() = default;

RenderProcessInfo::~RenderProcessInfo() = default;

RenderProcessMetricsHandler::RenderProcessMetricsHandler() = default;

RenderProcessMetricsHandler::~RenderProcessMetricsHandler() = default;

class ResourceCoordinatorRenderProcessMetricsHandler
    : public RenderProcessMetricsHandler {
 public:
  ResourceCoordinatorRenderProcessMetricsHandler() = default;
  ~ResourceCoordinatorRenderProcessMetricsHandler() override = default;

  // Send collected metrics back to the |resource_coordinator| service
  // and initiates another render process metrics gather cycle.
  bool HandleMetrics(
      const RenderProcessInfoMap& render_process_info_map) override {
    for (auto& render_process_info_map_entry : render_process_info_map) {
      auto& render_process_info = render_process_info_map_entry.second;
      // TODO(oysteine): Move the multiplier used to avoid precision loss
      // into a shared location, when this property gets used.
      render_process_info.host->GetProcessResourceCoordinator()->SetCPUUsage(
          render_process_info.cpu_usage);
    }

    return true;
  }
};

ResourceCoordinatorRenderProcessProbe::ResourceCoordinatorRenderProcessProbe()
    : metrics_handler_(
          std::make_unique<ResourceCoordinatorRenderProcessMetricsHandler>()),
      interval_ms_(
          base::TimeDelta::FromSeconds(kDefaultMeasurementIntervalInSeconds)) {
  UpdateWithFieldTrialParams();
}

ResourceCoordinatorRenderProcessProbe::
    ~ResourceCoordinatorRenderProcessProbe() = default;

// static
ResourceCoordinatorRenderProcessProbe*
ResourceCoordinatorRenderProcessProbe::GetInstance() {
  return g_probe.Pointer();
}

// static
bool ResourceCoordinatorRenderProcessProbe::IsEnabled() {
  // Check that service_manager is active, GRC is enabled,
  // and render process CPU profiling is enabled.
  return content::ServiceManagerConnection::GetForProcess() != nullptr &&
         resource_coordinator::IsResourceCoordinatorEnabled() &&
         base::FeatureList::IsEnabled(features::kGRCRenderProcessCPUProfiling);
}

void ResourceCoordinatorRenderProcessProbe::StartGatherCycle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!ResourceCoordinatorRenderProcessProbe::IsEnabled()) {
    return;
  }

  timer_.Start(FROM_HERE, base::TimeDelta(), this,
               &ResourceCoordinatorRenderProcessProbe::
                   RegisterAliveRenderProcessesOnUIThread);
}

void ResourceCoordinatorRenderProcessProbe::
    RegisterAliveRenderProcessesOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ++current_gather_cycle_;

  for (content::RenderProcessHost::iterator rph_iter =
           content::RenderProcessHost::AllHostsIterator();
       !rph_iter.IsAtEnd(); rph_iter.Advance()) {
    content::RenderProcessHost* host = rph_iter.GetCurrentValue();
    base::ProcessHandle handle = host->GetHandle();
    // Process may not be valid yet.
    if (handle == base::kNullProcessHandle) {
      continue;
    }

    auto& render_process_info = render_process_info_map_[handle];
    render_process_info.last_gather_cycle_active = current_gather_cycle_;

    if (render_process_info.metrics.get() == nullptr) {
#if defined(OS_MACOSX)
      render_process_info.metrics = base::ProcessMetrics::CreateProcessMetrics(
          handle, content::BrowserChildProcessHost::GetPortProvider());
#else
      render_process_info.metrics =
          base::ProcessMetrics::CreateProcessMetrics(handle);
#endif
      render_process_info.host = host;
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ResourceCoordinatorRenderProcessProbe::
                         CollectRenderProcessMetricsOnIOThread,
                     base::Unretained(this)));
}

void ResourceCoordinatorRenderProcessProbe::
    CollectRenderProcessMetricsOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  RenderProcessInfoMap::iterator iter = render_process_info_map_.begin();
  while (iter != render_process_info_map_.end()) {
    auto& render_process_info = iter->second;

    // If the last gather cycle the render process was marked as active is
    // not current then it is assumed dead and should not be measured anymore.
    if (render_process_info.last_gather_cycle_active == current_gather_cycle_) {
      render_process_info.cpu_usage =
          render_process_info.metrics->GetPlatformIndependentCPUUsage();
      ++iter;
    } else {
      render_process_info_map_.erase(iter++);
      continue;
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&ResourceCoordinatorRenderProcessProbe::
                         HandleRenderProcessMetricsOnUIThread,
                     base::Unretained(this)));
}

void ResourceCoordinatorRenderProcessProbe::
    HandleRenderProcessMetricsOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (metrics_handler_->HandleMetrics(render_process_info_map_)) {
    timer_.Start(FROM_HERE, interval_ms_, this,
                 &ResourceCoordinatorRenderProcessProbe::
                     RegisterAliveRenderProcessesOnUIThread);
  }
}

bool ResourceCoordinatorRenderProcessProbe::
    AllRenderProcessMeasurementsAreCurrentForTesting() const {
  for (auto& render_process_info_map_entry : render_process_info_map_) {
    auto& render_process_info = render_process_info_map_entry.second;
    if (render_process_info.last_gather_cycle_active != current_gather_cycle_ ||
        render_process_info.cpu_usage < 0.0) {
      return false;
    }
  }
  return true;
}

void ResourceCoordinatorRenderProcessProbe::UpdateWithFieldTrialParams() {
  int64_t interval_ms = GetGRCRenderProcessCPUProfilingIntervalInMs();

  if (interval_ms > 0) {
    interval_ms_ = base::TimeDelta::FromMilliseconds(interval_ms);
  }
}

}  // namespace resource_coordinator
