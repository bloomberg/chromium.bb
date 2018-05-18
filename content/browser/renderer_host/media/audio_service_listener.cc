// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_service_listener.h"

#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/audio/public/mojom/constants.mojom.h"

namespace content {

AudioServiceListener::AudioServiceListener(
    std::unique_ptr<service_manager::Connector> connector)
    : binding_(this), connector_(std::move(connector)) {
  if (!connector_)
    return;  // Happens in unittests.

  service_manager::mojom::ServiceManagerPtr service_manager;
  connector_->BindInterface(service_manager::mojom::kServiceName,
                            &service_manager);
  service_manager::mojom::ServiceManagerListenerPtr listener;
  service_manager::mojom::ServiceManagerListenerRequest request(
      mojo::MakeRequest(&listener));
  service_manager->AddListener(std::move(listener));
  binding_.Bind(std::move(request));
}

AudioServiceListener::~AudioServiceListener() = default;

base::ProcessId AudioServiceListener::GetProcessId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  return process_id_;
}

void AudioServiceListener::OnInit(
    std::vector<service_manager::mojom::RunningServiceInfoPtr>
        running_services) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  for (const service_manager::mojom::RunningServiceInfoPtr& instance :
       running_services) {
    if (instance->pid == base::kNullProcessId)
      continue;
    if (instance->identity.name() == audio::mojom::kServiceName) {
      process_id_ = instance->pid;
      break;
    }
  }
}

void AudioServiceListener::OnServiceCreated(
    service_manager::mojom::RunningServiceInfoPtr service) {}

void AudioServiceListener::OnServiceStarted(
    const ::service_manager::Identity& identity,
    uint32_t pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (identity.name() != audio::mojom::kServiceName)
    return;
  process_id_ = pid;
}

void AudioServiceListener::OnServicePIDReceived(
    const ::service_manager::Identity& identity,
    uint32_t pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (identity.name() != audio::mojom::kServiceName)
    return;
  process_id_ = pid;
}

void AudioServiceListener::OnServiceFailedToStart(
    const ::service_manager::Identity& identity) {}

void AudioServiceListener::OnServiceStopped(
    const ::service_manager::Identity& identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (identity.name() != audio::mojom::kServiceName)
    return;
  process_id_ = base::kNullProcessId;
}

}  // namespace content
