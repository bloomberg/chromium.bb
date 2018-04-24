// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/resource_coordinator_render_process_probe.h"

#include <vector>

#include "base/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/service_manager_connection.h"
#include "services/resource_coordinator/public/cpp/process_resource_coordinator.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/cpp/system_resource_coordinator.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

#if defined(OS_MACOSX)
#include "content/public/browser/browser_child_process_host.h"
#endif

namespace resource_coordinator {

namespace {

constexpr base::TimeDelta kDefaultMeasurementInterval =
    base::TimeDelta::FromMinutes(10);

base::LazyInstance<ResourceCoordinatorRenderProcessProbe>::DestructorAtExit
    g_probe = LAZY_INSTANCE_INITIALIZER;

}  // namespace

RenderProcessInfo::RenderProcessInfo() = default;

RenderProcessInfo::~RenderProcessInfo() = default;

ResourceCoordinatorRenderProcessProbe::ResourceCoordinatorRenderProcessProbe()
    : interval_(kDefaultMeasurementInterval) {
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
  // TODO(siggi): It irks me to have this bit of policy embedded here.
  //     I feel this should be moved to the caller...
  if (!ResourceCoordinatorRenderProcessProbe::IsEnabled()) {
    return;
  }

  DCHECK(!is_gather_cycle_started_);

  is_gather_cycle_started_ = true;
  if (!is_gathering_) {
    timer_.Start(FROM_HERE, base::TimeDelta(), this,
                 &ResourceCoordinatorRenderProcessProbe::
                     RegisterAliveRenderProcessesOnUIThread);
  }
}

void ResourceCoordinatorRenderProcessProbe::StartSingleGather() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_gathering_)
    return;

  // If the gather cycle is started this measurement will go through early,
  // and the interval between measurements will be shortened.
  timer_.Start(FROM_HERE, base::TimeDelta(), this,
               &ResourceCoordinatorRenderProcessProbe::
                   RegisterAliveRenderProcessesOnUIThread);
}

void ResourceCoordinatorRenderProcessProbe::
    RegisterAliveRenderProcessesOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!is_gathering_);

  ++current_gather_cycle_;

  for (content::RenderProcessHost::iterator rph_iter =
           content::RenderProcessHost::AllHostsIterator();
       !rph_iter.IsAtEnd(); rph_iter.Advance()) {
    content::RenderProcessHost* host = rph_iter.GetCurrentValue();
    // Process may not be valid yet.
    if (!host->GetProcess().IsValid()) {
      continue;
    }

    auto& render_process_info = render_process_info_map_[host->GetID()];
    render_process_info.last_gather_cycle_active = current_gather_cycle_;
    if (render_process_info.metrics.get() == nullptr) {
      DCHECK(!render_process_info.process.IsValid());

      // Duplicate the process to retain ownership of it through the thread
      // bouncing.
      render_process_info.process = host->GetProcess().Duplicate();

#if defined(OS_MACOSX)
      render_process_info.metrics = base::ProcessMetrics::CreateProcessMetrics(
          render_process_info.process.Handle(),
          content::BrowserChildProcessHost::GetPortProvider());
#else
      render_process_info.metrics = base::ProcessMetrics::CreateProcessMetrics(
          render_process_info.process.Handle());
#endif
      render_process_info.render_process_host_id = host->GetID();
    }
  }

  is_gathering_ = true;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ResourceCoordinatorRenderProcessProbe::
                         CollectRenderProcessMetricsOnIOThread,
                     base::Unretained(this)));
}

void ResourceCoordinatorRenderProcessProbe::
    CollectRenderProcessMetricsOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(is_gathering_);

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
  DCHECK(is_gathering_);
  is_gathering_ = false;

  if (DispatchMetrics() && is_gather_cycle_started_) {
    timer_.Start(FROM_HERE, interval_, this,
                 &ResourceCoordinatorRenderProcessProbe::
                     RegisterAliveRenderProcessesOnUIThread);
  } else {
    is_gather_cycle_started_ = false;
  }
}

void ResourceCoordinatorRenderProcessProbe::UpdateWithFieldTrialParams() {
  int64_t interval_ms = GetGRCRenderProcessCPUProfilingIntervalInMs();

  if (interval_ms > 0) {
    interval_ = base::TimeDelta::FromMilliseconds(interval_ms);
  }
}

SystemResourceCoordinator*
ResourceCoordinatorRenderProcessProbe::EnsureSystemResourceCoordinator() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!system_resource_coordinator_) {
    content::ServiceManagerConnection* connection =
        content::ServiceManagerConnection::GetForProcess();
    if (connection)
      system_resource_coordinator_ =
          std::make_unique<SystemResourceCoordinator>(
              connection->GetConnector());
  }

  return system_resource_coordinator_.get();
}

bool ResourceCoordinatorRenderProcessProbe::DispatchMetrics() {
  SystemResourceCoordinator* system_resource_coordinator =
      EnsureSystemResourceCoordinator();

  if (system_resource_coordinator) {
    bool dispatched_measurement = false;
    for (auto& render_process_info_map_entry : render_process_info_map_) {
      auto& render_process_info = render_process_info_map_entry.second;
      // TODO(oysteine): Move the multiplier used to avoid precision loss
      // into a shared location, when this property gets used.

      // Note that the RPH may have been deleted while the CPU metrics were
      // acquired on a blocking thread.
      content::RenderProcessHost* host = content::RenderProcessHost::FromID(
          render_process_info.render_process_host_id);
      if (host) {
        dispatched_measurement = true;
        host->GetProcessResourceCoordinator()->SetCPUUsage(
            render_process_info.cpu_usage);
      }
    }

    if (dispatched_measurement)
      system_resource_coordinator->OnProcessCPUUsageReady();
  }

  return true;
}

}  // namespace resource_coordinator
