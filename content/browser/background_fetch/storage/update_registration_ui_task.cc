// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/update_registration_ui_task.h"

#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "third_party/WebKit/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace content {

namespace background_fetch {

UpdateRegistrationUITask::UpdateRegistrationUITask(
    BackgroundFetchDataManager* data_manager,
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& unique_id,
    const std::string& updated_title,
    UpdateRegistrationUICallback callback)
    : DatabaseTask(data_manager),
      service_worker_registration_id_(service_worker_registration_id),
      origin_(origin),
      unique_id_(unique_id),
      updated_title_(updated_title),
      callback_(std::move(callback)),
      weak_factory_(this) {}

UpdateRegistrationUITask::~UpdateRegistrationUITask() = default;

void UpdateRegistrationUITask::Start() {
  service_worker_context()->GetRegistrationUserData(
      service_worker_registration_id_, {RegistrationKey(unique_id_)},
      base::BindOnce(&UpdateRegistrationUITask::DidGetUniqueId,
                     weak_factory_.GetWeakPtr()));
}

void UpdateRegistrationUITask::DidGetUniqueId(
    const std::vector<std::string>& data,
    ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
    case DatabaseStatus::kFailed:
      std::move(callback_).Run(
          blink::mojom::BackgroundFetchError::STORAGE_ERROR);
      Finished();  // Destroys |this|.
      return;
    case DatabaseStatus::kOk:
      DCHECK_EQ(1u, data.size());
      if (data.empty()) {
        std::move(callback_).Run(
            blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        Finished();  // Destroys |this|.
        return;
      }
      UpdateUI(data[0]);
      return;
  }
}

void UpdateRegistrationUITask::UpdateUI(
    const std::string& serialized_metadata_proto) {
  proto::BackgroundFetchMetadata metadata_proto;
  if (!metadata_proto.ParseFromString(serialized_metadata_proto)) {
    std::move(callback_).Run(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
    Finished();  // Destroys |this|.
    return;
  }

  metadata_proto.set_ui_title(updated_title_);

  service_worker_context()->StoreRegistrationUserData(
      service_worker_registration_id_, origin_.GetURL(),
      {{RegistrationKey(unique_id_), metadata_proto.SerializeAsString()}},
      base::BindRepeating(&UpdateRegistrationUITask::DidUpdateUI,
                          weak_factory_.GetWeakPtr()));
}

void UpdateRegistrationUITask::DidUpdateUI(ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      break;
    case DatabaseStatus::kFailed:
    case DatabaseStatus::kNotFound:
      std::move(callback_).Run(
          blink::mojom::BackgroundFetchError::STORAGE_ERROR);
      Finished();  // Destroys |this|.
      return;
  }

  std::move(callback_).Run(blink::mojom::BackgroundFetchError::NONE);
  Finished();  // Destroys |this|.
}

}  // namespace background_fetch

}  // namespace content
