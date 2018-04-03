// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_registration_task.h"

#include <utility>

#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "url/gurl.h"

namespace content {

namespace background_fetch {

GetRegistrationTask::GetRegistrationTask(
    BackgroundFetchDataManager* data_manager,
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& developer_id,
    GetRegistrationCallback callback)
    : DatabaseTask(data_manager),
      service_worker_registration_id_(service_worker_registration_id),
      origin_(origin),
      developer_id_(developer_id),
      callback_(std::move(callback)),
      weak_factory_(this) {}

GetRegistrationTask::~GetRegistrationTask() = default;

void GetRegistrationTask::Start() {
  service_worker_context()->GetRegistrationUserData(
      service_worker_registration_id_,
      {ActiveRegistrationUniqueIdKey(developer_id_)},
      base::BindOnce(&GetRegistrationTask::DidGetUniqueId,
                     weak_factory_.GetWeakPtr()));
}

void GetRegistrationTask::DidGetUniqueId(const std::vector<std::string>& data,
                                         ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
      std::move(callback_).Run(blink::mojom::BackgroundFetchError::INVALID_ID,
                               nullptr /* registration */);
      Finished();  // Destroys |this|.
      return;
    case DatabaseStatus::kOk:
      DCHECK_EQ(1u, data.size());
      service_worker_context()->GetRegistrationUserData(
          service_worker_registration_id_, {RegistrationKey(data[0])},
          base::BindOnce(&GetRegistrationTask::DidGetRegistration,
                         weak_factory_.GetWeakPtr()));
      return;
    case DatabaseStatus::kFailed:
      std::move(callback_).Run(
          blink::mojom::BackgroundFetchError::STORAGE_ERROR,
          nullptr /* registration */);
      Finished();  // Destroys |this|.
      return;
  }
}

void GetRegistrationTask::DidGetRegistration(
    const std::vector<std::string>& data,
    ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
      // The database is corrupt as there's no registration data despite there
      // being an active developer_id pointing to it.
      std::move(callback_).Run(
          blink::mojom::BackgroundFetchError::STORAGE_ERROR,
          nullptr /* registration */);
      Finished();  // Destroys |this|.
      return;
    case DatabaseStatus::kOk:
      DCHECK_EQ(1u, data.size());
      CreateRegistration(data[0]);
      return;
    case DatabaseStatus::kFailed:
      std::move(callback_).Run(
          blink::mojom::BackgroundFetchError::STORAGE_ERROR,
          nullptr /* registration */);
      Finished();  // Destroys |this|.
      return;
  }
}

void GetRegistrationTask::InitializeRegistration(
    const proto::BackgroundFetchRegistration& registration_proto,
    BackgroundFetchRegistration* registration) {
  DCHECK(registration);

  // per-registration data.
  registration->developer_id = registration_proto.developer_id();
  registration->unique_id = registration_proto.unique_id();

  // Download/Upload stats.
  registration->upload_total = registration_proto.upload_total();
  registration->uploaded = registration_proto.uploaded();
  registration->download_total = registration_proto.download_total();
  registration->downloaded = registration_proto.downloaded();
}

void GetRegistrationTask::CreateRegistration(const std::string& metadata) {
  proto::BackgroundFetchMetadata metadata_proto;
  if (!metadata_proto.ParseFromString(metadata)) {
    std::move(callback_).Run(blink::mojom::BackgroundFetchError::STORAGE_ERROR,
                             nullptr /* registration */);
    Finished();
    return;
  }

  const auto& registration_proto = metadata_proto.registration();
  if (registration_proto.developer_id() != developer_id_ ||
      !origin_.IsSameOriginWith(
          url::Origin::Create(GURL(metadata_proto.origin())))) {
    std::move(callback_).Run(blink::mojom::BackgroundFetchError::STORAGE_ERROR,
                             nullptr /* registration */);
    Finished();
    return;
  }

  // TODO(rayankans): Initialize all the other parts of the metadata.
  auto registration = std::make_unique<BackgroundFetchRegistration>();
  InitializeRegistration(metadata_proto.registration(), registration.get());

  std::move(callback_).Run(blink::mojom::BackgroundFetchError::NONE,
                           std::move(registration));
  Finished();
}

}  // namespace background_fetch

}  // namespace content
