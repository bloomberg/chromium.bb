// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/leak_detector/leak_detector_remote_controller.h"

#include "content/public/browser/browser_thread.h"

namespace metrics {

namespace {

// All instances of LeakDetectorRemoteController will need to reference a single
// LocalController instance, referenced by this pointer. All remote LeakDetector
// clients will get their params from and send leak reports to this instance.
LeakDetectorRemoteController::LocalController* g_local_controller = nullptr;

}  // namespace

LeakDetectorRemoteController::~LeakDetectorRemoteController() {}

// static
void LeakDetectorRemoteController::Create(LeakDetectorRemoteRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  new LeakDetectorRemoteController(std::move(request));
}

void LeakDetectorRemoteController::GetParams(
    const LeakDetectorRemote::GetParamsCallback& callback) {
  // If no controller exists, send an empty param protobuf. The remote caller
  // should not initialize anything if the params are empty.
  MemoryLeakReportProto_Params params;
  if (g_local_controller) {
    params = g_local_controller->GetParams();
  }

  // Serialize the params before sending it to the Mojo client.
  std::string serialized_params;
  params.SerializeToString(&serialized_params);

  callback.Run(serialized_params);
}

void LeakDetectorRemoteController::SendLeakReports(
    mojo::Array<mojo::String> reports) {
  std::vector<MemoryLeakReportProto> report_protos;
  report_protos.reserve(reports.size());

  for (const mojo::String& serialized_report : reports) {
    report_protos.push_back(MemoryLeakReportProto());
    report_protos.back().ParseFromString(serialized_report);
  }
  DCHECK(g_local_controller);
  g_local_controller->SendLeakReports(report_protos);
}

LeakDetectorRemoteController::LeakDetectorRemoteController(
    LeakDetectorRemoteRequest request)
    : binding_(this, std::move(request)) {}

// static
void LeakDetectorRemoteController::SetLocalControllerInstance(
    LocalController* controller) {
  g_local_controller = controller;
}

}  // namespace metrics
