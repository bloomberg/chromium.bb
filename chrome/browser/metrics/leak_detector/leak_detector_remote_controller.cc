// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/leak_detector/leak_detector_remote_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "components/metrics/leak_detector/protobuf_to_mojo_converter.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace metrics {

namespace {

// All instances of LeakDetectorRemoteController will need to reference a single
// LocalController instance, referenced by this pointer. All remote LeakDetector
// clients will get their params from and send leak reports to this instance.
LeakDetectorRemoteController::LocalController* g_local_controller = nullptr;

}  // namespace

LeakDetectorRemoteController::~LeakDetectorRemoteController() {}

// static
void LeakDetectorRemoteController::Create(mojom::LeakDetectorRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<LeakDetectorRemoteController> controller =
      base::WrapUnique(new LeakDetectorRemoteController);
  base::Closure error_handler =
      base::Bind(&LeakDetectorRemoteController::OnRemoteProcessShutdown,
                 base::Unretained(controller.get()));
  auto binding =
      mojo::MakeStrongBinding(std::move(controller), std::move(request));
  binding->set_connection_error_handler(error_handler);
}

void LeakDetectorRemoteController::GetParams(
    const mojom::LeakDetector::GetParamsCallback& callback) {
  // If no controller exists, send an empty param protobuf. The remote caller
  // should not initialize anything if the params are empty.
  MemoryLeakReportProto_Params params;
  if (g_local_controller) {
    params = g_local_controller->GetParamsAndRecordRequest();
    // A non-zero sampling rate tells the remote process to enable the leak
    // detector. Otherwise, the remote process will not initialize it.
    leak_detector_enabled_on_remote_process_ = params.sampling_rate() > 0;
  }

  mojo::StructPtr<mojom::LeakDetectorParams> mojo_params =
      mojom::LeakDetectorParams::New();
  leak_detector::protobuf_to_mojo_converter::ParamsToMojo(params,
                                                          mojo_params.get());

  callback.Run(std::move(mojo_params));
}

void LeakDetectorRemoteController::SendLeakReports(
    std::vector<mojom::MemoryLeakReportPtr> reports) {
  std::vector<MemoryLeakReportProto> report_protos;
  report_protos.reserve(reports.size());

  for (const mojom::MemoryLeakReportPtr& report : reports) {
    report_protos.push_back(MemoryLeakReportProto());
    MemoryLeakReportProto* proto = &report_protos.back();

    leak_detector::protobuf_to_mojo_converter::MojoToReport(*report, proto);
  }
  if (g_local_controller) {
    g_local_controller->SendLeakReports(report_protos);
  }
}

void LeakDetectorRemoteController::OnRemoteProcessShutdown() {
  if (leak_detector_enabled_on_remote_process_ && g_local_controller) {
    g_local_controller->OnRemoteProcessShutdown();
  }
}

LeakDetectorRemoteController::LeakDetectorRemoteController()
    : leak_detector_enabled_on_remote_process_(false) {}

// static
void LeakDetectorRemoteController::SetLocalControllerInstance(
    LocalController* controller) {
  // This must be on the same thread as the Mojo-based functions of this class,
  // to avoid race conditions on |g_local_controller|.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  g_local_controller = controller;
}

}  // namespace metrics
