// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/create_registration_task.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/service_worker_status_code.h"

namespace content {

namespace background_fetch {

std::string RequestKey(const std::string& unique_id, int request_index) {
  // Allows looking up a request by registration id and index within that.
  return RequestKeyPrefix(unique_id) + base::IntToString(request_index);
}

CreateRegistrationTask::CreateRegistrationTask(
    BackgroundFetchDataManager* data_manager,
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    CreateRegistrationCallback callback)
    : DatabaseTask(data_manager),
      registration_id_(registration_id),
      requests_(requests),
      options_(options),
      callback_(std::move(callback)),
      weak_factory_(this) {}

CreateRegistrationTask::~CreateRegistrationTask() = default;

void CreateRegistrationTask::Start() {
  service_worker_context()->GetRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      {ActiveRegistrationUniqueIdKey(registration_id_.developer_id())},
      base::BindOnce(&CreateRegistrationTask::DidGetUniqueId,
                     weak_factory_.GetWeakPtr()));
}

void CreateRegistrationTask::DidGetUniqueId(
    const std::vector<std::string>& data,
    ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
      StoreMetadata();
      return;
    case DatabaseStatus::kOk:
      // Can't create a registration since there is already an active
      // registration with the same |developer_id|. It must be deactivated
      // (completed/failed/aborted) first.
      std::move(callback_).Run(
          blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID,
          nullptr /* registration */);
      Finished();  // Destroys |this|.
      return;
    case DatabaseStatus::kFailed:
      std::move(callback_).Run(
          blink::mojom::BackgroundFetchError::STORAGE_ERROR,
          nullptr /* registration */);
      Finished();  // Destroys |this|.
      return;
  }
}

proto::BackgroundFetchMetadata CreateRegistrationTask::CreateMetadataProto()
    const {
  proto::BackgroundFetchMetadata metadata_proto;

  // Set BackgroundFetchRegistration fields.
  // Upload/Download stats default to correct initial values.
  auto* registration_proto = metadata_proto.mutable_registration();
  registration_proto->set_unique_id(registration_->unique_id);
  registration_proto->set_developer_id(registration_->developer_id);

  // Set Options fields.
  auto* options_proto = metadata_proto.mutable_options();
  options_proto->set_title(options_.title);
  options_proto->set_download_total(options_.download_total);
  for (const auto& icon : options_.icons) {
    auto* icon_definition_proto = options_proto->add_icons();
    icon_definition_proto->set_src(icon.src);
    icon_definition_proto->set_sizes(icon.sizes);
    icon_definition_proto->set_type(icon.type);
  }

  // Set other metadata fields.
  metadata_proto.set_origin(registration_id_.origin().Serialize());
  metadata_proto.set_creation_microseconds_since_unix_epoch(
      (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds());
  metadata_proto.set_ui_title(options_.title);

  return metadata_proto;
}

void CreateRegistrationTask::StoreMetadata() {
  DCHECK(!registration_);
  DCHECK(!registration_id_.origin().unique());

  registration_ = std::make_unique<BackgroundFetchRegistration>();
  registration_->developer_id = registration_id_.developer_id();
  registration_->unique_id = registration_id_.unique_id();
  // TODO(crbug.com/774054): Uploads are not yet supported.
  registration_->upload_total = 0;
  registration_->uploaded = 0;
  registration_->download_total = options_.download_total;
  registration_->downloaded = 0;

  std::vector<std::pair<std::string, std::string>> entries;
  entries.reserve(requests_.size() * 2 + 1);

  const proto::BackgroundFetchMetadata metadata_proto = CreateMetadataProto();
  std::string serialized_metadata_proto;

  if (!metadata_proto.SerializeToString(&serialized_metadata_proto)) {
    // TODO(crbug.com/780025): Log failures to UMA.
    std::move(callback_).Run(blink::mojom::BackgroundFetchError::STORAGE_ERROR,
                             nullptr /* registration */);
    Finished();  // Destroys |this|.
    return;
  }

  entries.emplace_back(
      ActiveRegistrationUniqueIdKey(registration_id_.developer_id()),
      registration_id_.unique_id());
  entries.emplace_back(RegistrationKey(registration_id_.unique_id()),
                       std::move(serialized_metadata_proto));

  // Signed integers are used for request indexes to avoid unsigned gotchas.
  for (int i = 0; i < base::checked_cast<int>(requests_.size()); i++) {
    // TODO(crbug.com/757760): Serialize actual values for these entries.
    entries.emplace_back(RequestKey(registration_id_.unique_id(), i),
                         "TODO: Serialize FetchAPIRequest as value");
    entries.emplace_back(
        PendingRequestKey(
            metadata_proto.creation_microseconds_since_unix_epoch(),
            registration_id_.unique_id(), i),
        std::string());
  }

  service_worker_context()->StoreRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      registration_id_.origin().GetURL(), entries,
      base::BindRepeating(&CreateRegistrationTask::DidStoreMetadata,
                          weak_factory_.GetWeakPtr()));
}

void CreateRegistrationTask::DidStoreMetadata(ServiceWorkerStatusCode status) {
  DCHECK(registration_);

  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      break;
    case DatabaseStatus::kFailed:
    case DatabaseStatus::kNotFound:
      std::move(callback_).Run(
          blink::mojom::BackgroundFetchError::STORAGE_ERROR,
          nullptr /* registration */);
      Finished();  // Destroys |this|.
      return;
  }

  std::move(callback_).Run(blink::mojom::BackgroundFetchError::NONE,
                           std::move(registration_));
  Finished();  // Destroys |this|.
}
}  // namespace background_fetch

}  // namespace content
