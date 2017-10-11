// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/leak_detector/leak_detector_remote_client.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/metrics/leak_detector/protobuf_to_mojo_converter.h"
#include "components/metrics/proto/memory_leak_report.pb.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connector.h"

LeakDetectorRemoteClient::LeakDetectorRemoteClient() {
  // Connect to Mojo service.
  content::RenderThread::Get()->GetConnector()->BindInterface(
      content::mojom::kBrowserServiceName, &remote_service_);

  // It is safe to use "base::Unretained(this)" because |this| owns
  // |remote_service_|. See example at:
  // https://www.chromium.org/developers/design-documents/mojo/validation
  remote_service_->GetParams(base::Bind(
      &LeakDetectorRemoteClient::OnParamsReceived, base::Unretained(this)));
}

LeakDetectorRemoteClient::~LeakDetectorRemoteClient() {
  metrics::LeakDetector::GetInstance()->RemoveObserver(this);
}

void LeakDetectorRemoteClient::OnLeaksFound(
    const std::vector<metrics::MemoryLeakReportProto>& reports) {
  std::vector<mojo::StructPtr<metrics::mojom::MemoryLeakReport>> result;

  for (const metrics::MemoryLeakReportProto& report : reports) {
    metrics::mojom::MemoryLeakReportPtr mojo_report =
        metrics::mojom::MemoryLeakReport::New();
    metrics::leak_detector::protobuf_to_mojo_converter::ReportToMojo(
        report, mojo_report.get());
    result.push_back(std::move(mojo_report));
  }

  remote_service_->SendLeakReports(std::move(result));
}

void LeakDetectorRemoteClient::OnParamsReceived(
    mojo::StructPtr<metrics::mojom::LeakDetectorParams> result) {
  metrics::MemoryLeakReportProto::Params params;
  metrics::leak_detector::protobuf_to_mojo_converter::MojoToParams(*result,
                                                                   &params);

  if (params.sampling_rate() > 0) {
    metrics::LeakDetector* detector = metrics::LeakDetector::GetInstance();
    detector->AddObserver(this);
    detector->Init(params, base::ThreadTaskRunnerHandle::Get());
  }
}
