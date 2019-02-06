// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/chrome_browser_main_extra_parts_resource_coordinator.h"

#include <utility>

#include "base/process/process.h"
#include "chrome/browser/performance_manager/performance_manager.h"
#include "chrome/browser/performance_manager/process_resource_coordinator.h"
#include "chrome/browser/resource_coordinator/browser_child_process_watcher.h"
#include "chrome/browser/resource_coordinator/page_signal_receiver.h"
#include "chrome/browser/resource_coordinator/render_process_probe.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

ChromeBrowserMainExtraPartsResourceCoordinator::
    ChromeBrowserMainExtraPartsResourceCoordinator() = default;
ChromeBrowserMainExtraPartsResourceCoordinator::
    ~ChromeBrowserMainExtraPartsResourceCoordinator() = default;

void ChromeBrowserMainExtraPartsResourceCoordinator::
    ServiceManagerConnectionStarted(
        content::ServiceManagerConnection* connection) {
  performance_manager_ = performance_manager::PerformanceManager::Create();

  process_resource_coordinator_ =
      std::make_unique<performance_manager::ProcessResourceCoordinator>(
          performance_manager_.get());

  process_resource_coordinator_->OnProcessLaunched(base::Process::Current());

  browser_child_process_watcher_ =
      std::make_unique<resource_coordinator::BrowserChildProcessWatcher>();
}

void ChromeBrowserMainExtraPartsResourceCoordinator::PreBrowserStart() {
  if (base::FeatureList::IsEnabled(features::kPerformanceMeasurement)) {
    DCHECK(resource_coordinator::RenderProcessProbe::IsEnabled());
    resource_coordinator::PageSignalReceiver* page_signal_receiver =
        resource_coordinator::GetPageSignalReceiver();

    DCHECK_NE(nullptr, performance_manager_.get());
    resource_coordinator::RenderProcessProbe* render_process_probe =
        resource_coordinator::RenderProcessProbe::GetInstance();

    performance_measurement_manager_ =
        std::make_unique<resource_coordinator::PerformanceMeasurementManager>(
            page_signal_receiver, render_process_probe);
  }
}

void ChromeBrowserMainExtraPartsResourceCoordinator::PostMainMessageLoopRun() {
  performance_manager::PerformanceManager::Destroy(
      std::move(performance_manager_));
}
