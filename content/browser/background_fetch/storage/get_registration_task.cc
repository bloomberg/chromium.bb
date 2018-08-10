// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_registration_task.h"

#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/background_fetch/storage/get_metadata_task.h"

namespace content {

namespace background_fetch {

GetRegistrationTask::GetRegistrationTask(DatabaseTaskHost* host,
                                         int64_t service_worker_registration_id,
                                         const url::Origin& origin,
                                         const std::string& developer_id,
                                         GetRegistrationCallback callback)
    : DatabaseTask(host),
      service_worker_registration_id_(service_worker_registration_id),
      origin_(origin),
      developer_id_(developer_id),
      callback_(std::move(callback)),
      weak_factory_(this) {}

GetRegistrationTask::~GetRegistrationTask() = default;

void GetRegistrationTask::Start() {
  AddSubTask(std::make_unique<GetMetadataTask>(
      this, service_worker_registration_id_, origin_, developer_id_,
      base::BindOnce(&GetRegistrationTask::DidGetMetadata,
                     weak_factory_.GetWeakPtr())));
}

void GetRegistrationTask::DidGetMetadata(
    blink::mojom::BackgroundFetchError error,
    std::unique_ptr<proto::BackgroundFetchMetadata> metadata_proto) {
  metadata_proto_ = std::move(metadata_proto);
  if (error == blink::mojom::BackgroundFetchError::STORAGE_ERROR)
    SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
  FinishWithError(error);
}

void GetRegistrationTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  BackgroundFetchRegistration registration;

  if (error == blink::mojom::BackgroundFetchError::NONE) {
    DCHECK(metadata_proto_);

    registration = ToBackgroundFetchRegistration(*metadata_proto_);
  }

  ReportStorageError();

  std::move(callback_).Run(error, registration);
  Finished();  // Destroys |this|.
}

std::string GetRegistrationTask::HistogramName() const {
  return "GetRegistrationTask";
}

}  // namespace background_fetch

}  // namespace content
