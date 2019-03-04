// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/background_service_handler.h"

#include "content/browser/devtools/devtools_background_services.pb.h"
#include "content/browser/devtools/devtools_background_services_context.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/render_process_host.h"

namespace content {
namespace protocol {

namespace {

devtools::proto::BackgroundService ServiceNameToEnum(
    const std::string& service_name) {
  if (service_name == BackgroundService::ServiceNameEnum::BackgroundFetch) {
    return devtools::proto::BackgroundService::BACKGROUND_FETCH;
  } else if (service_name ==
             BackgroundService::ServiceNameEnum::BackgroundSync) {
    return devtools::proto::BackgroundService::BACKGROUND_SYNC;
  }
  return devtools::proto::BackgroundService::UNKNOWN;
}

}  // namespace

BackgroundServiceHandler::BackgroundServiceHandler()
    : DevToolsDomainHandler(BackgroundService::Metainfo::domainName),
      devtools_context_(nullptr) {}

BackgroundServiceHandler::~BackgroundServiceHandler() = default;

void BackgroundServiceHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ =
      std::make_unique<BackgroundService::Frontend>(dispatcher->channel());
  BackgroundService::Dispatcher::wire(dispatcher, this);
}

void BackgroundServiceHandler::SetRenderer(int process_host_id,
                                           RenderFrameHostImpl* frame_host) {
  RenderProcessHost* process_host = RenderProcessHost::FromID(process_host_id);
  if (!process_host) {
    devtools_context_ = nullptr;
    return;
  }

  auto* storage_partition =
      static_cast<StoragePartitionImpl*>(process_host->GetStoragePartition());
  devtools_context_ = storage_partition->GetDevToolsBackgroundServicesContext();
  DCHECK(devtools_context_);
}

Response BackgroundServiceHandler::Disable() {
  return DevToolsDomainHandler::Disable();
}

Response BackgroundServiceHandler::Enable(const std::string& service) {
  DCHECK(devtools_context_);

  auto service_enum = ServiceNameToEnum(service);
  if (service_enum == devtools::proto::BackgroundService::UNKNOWN)
    return Response::InvalidParams("Invalid service name");

  bool is_recording = devtools_context_->IsRecording(service_enum);

  DCHECK(frontend_);
  frontend_->RecordingStateChanged(is_recording, service);

  return Response::OK();
}

Response BackgroundServiceHandler::Disable(const std::string& service) {
  auto service_enum = ServiceNameToEnum(service);
  if (service_enum == devtools::proto::BackgroundService::UNKNOWN)
    return Response::InvalidParams("Invalid service name");

  return Response::OK();
}

Response BackgroundServiceHandler::SetRecording(bool should_record,
                                                const std::string& service) {
  DCHECK(devtools_context_);

  auto service_enum = ServiceNameToEnum(service);
  if (service_enum == devtools::proto::BackgroundService::UNKNOWN)
    return Response::InvalidParams("Invalid service name");

  if (should_record)
    devtools_context_->StartRecording(service_enum);
  else
    devtools_context_->StopRecording(service_enum);

  frontend_->RecordingStateChanged(should_record, service);

  // TODO(rayankans): Inform all the other open sessions.
  return Response::OK();
}

}  // namespace protocol
}  // namespace content
