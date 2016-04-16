// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/metrics/arc_metrics_service.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"

namespace {

const int kRequestProcessListPeriodInMinutes = 5;
const char kArcProcessNamePrefix[] = "org.chromium.arc.";
const char kGmsProcessNamePrefix[] = "com.google.android.gms";

} // namespace

namespace arc {

ArcMetricsService::ArcMetricsService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), weak_ptr_factory_(this) {
  arc_bridge_service()->AddObserver(this);
  low_memory_killer_minotor_.Start();
}

ArcMetricsService::~ArcMetricsService() {
  DCHECK(CalledOnValidThread());
  arc_bridge_service()->RemoveObserver(this);
}

bool ArcMetricsService::CalledOnValidThread() {
  // Make sure access to the Chrome clipboard is happening in the UI thread.
  return thread_checker_.CalledOnValidThread();
}

void ArcMetricsService::OnProcessInstanceReady() {
  VLOG(2) << "Start updating process list.";
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMinutes(kRequestProcessListPeriodInMinutes),
      this,
      &ArcMetricsService::RequestProcessList);
}

void ArcMetricsService::OnProcessInstanceClosed() {
  VLOG(2) << "Stop updating process list.";
  timer_.Stop();
}

void ArcMetricsService::RequestProcessList() {
  mojom::ProcessInstance* process_instance =
      arc_bridge_service()->process_instance();
  if (!process_instance) {
    LOG(ERROR) << "No process instance found before RequestProcessList";
    return;
  }

  VLOG(2) << "RequestProcessList";
  process_instance->RequestProcessList(
      base::Bind(&ArcMetricsService::ParseProcessList,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcMetricsService::ParseProcessList(
    mojo::Array<arc::mojom::RunningAppProcessInfoPtr> processes) {
  int running_app_count = 0;
  for (const auto& process : processes) {
    const mojo::String& process_name = process->process_name;
    const mojom::ProcessState& process_state = process->process_state;

    // Processes like the ARC launcher and intent helper are always running
    // and not counted as apps running by users. With the same reasoning,
    // GMS (Google Play Services) and its related processes are skipped as
    // well. The process_state check below filters out system processes,
    // services, apps that are cached because they've run before.
    if (base::StartsWith(process_name.get(), kArcProcessNamePrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(process_name.get(), kGmsProcessNamePrefix,
                         base::CompareCase::SENSITIVE) ||
        process_state != mojom::ProcessState::TOP) {
      VLOG(2) << "Skipped " << process_name << " " << process_state;
    } else {
      ++running_app_count;
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Arc.AppCount", running_app_count);
}

}  // namespace arc
