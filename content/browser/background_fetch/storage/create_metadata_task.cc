// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/create_metadata_task.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_data_manager_observer.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/background_fetch/storage/image_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {

namespace background_fetch {

CreateMetadataTask::CreateMetadataTask(
    DatabaseTaskHost* host,
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    CreateMetadataCallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      requests_(requests),
      options_(options),
      icon_(icon),
      callback_(std::move(callback)),
      weak_factory_(this) {}

CreateMetadataTask::~CreateMetadataTask() = default;

void CreateMetadataTask::Start() {
  // Check if there is enough quota to download the data first.
  if (options_.download_total > 0) {
    IsQuotaAvailable(registration_id_.origin(), options_.download_total,
                     base::BindOnce(&CreateMetadataTask::DidGetIsQuotaAvailable,
                                    weak_factory_.GetWeakPtr()));
  } else {
    // Proceed with the fetch.
    GetRegistrationUniqueId();
  }
}

void CreateMetadataTask::DidGetIsQuotaAvailable(bool is_available) {
  if (!is_available)
    FinishWithError(blink::mojom::BackgroundFetchError::QUOTA_EXCEEDED);
  else
    GetRegistrationUniqueId();
}

void CreateMetadataTask::GetRegistrationUniqueId() {
  service_worker_context()->GetRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      {ActiveRegistrationUniqueIdKey(registration_id_.developer_id())},
      base::BindOnce(&CreateMetadataTask::DidGetUniqueId,
                     weak_factory_.GetWeakPtr()));
}

void CreateMetadataTask::DidGetUniqueId(const std::vector<std::string>& data,
                                        blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
      break;
    case DatabaseStatus::kOk:
      // Can't create a registration since there is already an active
      // registration with the same |developer_id|. It must be deactivated
      // (completed/failed/aborted) first.
      FinishWithError(
          blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID);
      return;
    case DatabaseStatus::kFailed:
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      return;
  }

  InitializeMetadataProto();

  if (ShouldPersistIcon(icon_)) {
    // Serialize the icon, then store all the metadata.
    SerializeIcon(icon_, base::BindOnce(&CreateMetadataTask::DidSerializeIcon,
                                        weak_factory_.GetWeakPtr()));
  } else {
    // Directly store the metadata.
    StoreMetadata();
  }
}

void CreateMetadataTask::InitializeMetadataProto() {
  metadata_proto_ = std::make_unique<proto::BackgroundFetchMetadata>();

  // Set BackgroundFetchRegistration fields.
  auto* registration_proto = metadata_proto_->mutable_registration();
  registration_proto->set_unique_id(registration_id_.unique_id());
  registration_proto->set_developer_id(registration_id_.developer_id());
  registration_proto->set_download_total(options_.download_total);

  // Set Options fields.
  auto* options_proto = metadata_proto_->mutable_options();
  options_proto->set_title(options_.title);
  options_proto->set_download_total(options_.download_total);
  for (const auto& icon : options_.icons) {
    auto* image_resource_proto = options_proto->add_icons();

    image_resource_proto->set_src(icon.src.spec());

    for (const auto& size : icon.sizes) {
      auto* size_proto = image_resource_proto->add_sizes();
      size_proto->set_width(size.width());
      size_proto->set_height(size.height());
    }

    image_resource_proto->set_type(base::UTF16ToASCII(icon.type));

    for (const auto& purpose : icon.purpose) {
      switch (purpose) {
        case blink::Manifest::ImageResource::Purpose::ANY:
          image_resource_proto->add_purpose(
              proto::BackgroundFetchOptions_ImageResource_Purpose_ANY);
          break;
        case blink::Manifest::ImageResource::Purpose::BADGE:
          image_resource_proto->add_purpose(
              proto::BackgroundFetchOptions_ImageResource_Purpose_BADGE);
          break;
      }
    }
  }

  // Set other metadata fields.
  metadata_proto_->set_origin(registration_id_.origin().Serialize());
  metadata_proto_->set_creation_microseconds_since_unix_epoch(
      (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds());
  metadata_proto_->set_num_fetches(requests_.size());
}

void CreateMetadataTask::DidSerializeIcon(std::string serialized_icon) {
  serialized_icon_ = std::move(serialized_icon);
  StoreMetadata();
}

void CreateMetadataTask::StoreMetadata() {
  DCHECK(metadata_proto_);
  std::vector<std::pair<std::string, std::string>> entries;
  // - One BackgroundFetchPendingRequest per request
  // - DeveloperId -> UniqueID
  // - BackgroundFetchMetadata
  // - BackgroundFetchUIOptions
  entries.reserve(requests_.size() + 3);

  std::string serialized_metadata_proto;

  if (!metadata_proto_->SerializeToString(&serialized_metadata_proto)) {
    SetStorageErrorAndFinish(
        BackgroundFetchStorageError::kServiceWorkerStorageError);
    return;
  }

  std::string serialized_ui_options_proto;
  proto::BackgroundFetchUIOptions ui_options;
  ui_options.set_title(options_.title);
  if (!serialized_icon_.empty())
    ui_options.set_icon(std::move(serialized_icon_));

  if (!ui_options.SerializeToString(&serialized_ui_options_proto)) {
    SetStorageErrorAndFinish(
        BackgroundFetchStorageError::kServiceWorkerStorageError);
    return;
  }

  entries.emplace_back(
      ActiveRegistrationUniqueIdKey(registration_id_.developer_id()),
      registration_id_.unique_id());
  entries.emplace_back(RegistrationKey(registration_id_.unique_id()),
                       std::move(serialized_metadata_proto));
  entries.emplace_back(UIOptionsKey(registration_id_.unique_id()),
                       serialized_ui_options_proto);

  // Signed integers are used for request indexes to avoid unsigned gotchas.
  for (int i = 0; i < base::checked_cast<int>(requests_.size()); i++) {
    proto::BackgroundFetchPendingRequest pending_request_proto;
    pending_request_proto.set_unique_id(registration_id_.unique_id());
    pending_request_proto.set_request_index(i);
    pending_request_proto.set_serialized_request(requests_[i].Serialize());
    entries.emplace_back(PendingRequestKey(registration_id_.unique_id(), i),
                         pending_request_proto.SerializeAsString());
  }

  service_worker_context()->StoreRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      registration_id_.origin().GetURL(), entries,
      base::BindOnce(&CreateMetadataTask::DidStoreMetadata,
                     weak_factory_.GetWeakPtr()));
}

void CreateMetadataTask::DidStoreMetadata(
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      break;
    case DatabaseStatus::kFailed:
    case DatabaseStatus::kNotFound:
      SetStorageErrorAndFinish(
          BackgroundFetchStorageError::kServiceWorkerStorageError);
      return;
  }

  FinishWithError(blink::mojom::BackgroundFetchError::NONE);
}

void CreateMetadataTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  BackgroundFetchRegistration registration;

  if (error == blink::mojom::BackgroundFetchError::NONE) {
    DCHECK(metadata_proto_);

    registration = ToBackgroundFetchRegistration(*metadata_proto_);

    for (auto& observer : data_manager()->observers()) {
      observer.OnRegistrationCreated(registration_id_, registration, options_,
                                     icon_, requests_.size());
    }
  }

  ReportStorageError();

  std::move(callback_).Run(error, registration);
  Finished();  // Destroys |this|.
}

std::string CreateMetadataTask::HistogramName() const {
  return "CreateMetadataTask";
}

}  // namespace background_fetch

}  // namespace content
