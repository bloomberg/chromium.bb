// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/update_registration_ui_task.h"

#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_data_manager_observer.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace content {

namespace background_fetch {

UpdateRegistrationUITask::UpdateRegistrationUITask(
    DatabaseTaskHost* host,
    const BackgroundFetchRegistrationId& registration_id,
    const std::string& updated_title,
    UpdateRegistrationUICallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      updated_title_(updated_title),
      callback_(std::move(callback)),
      weak_factory_(this) {}

UpdateRegistrationUITask::~UpdateRegistrationUITask() = default;

void UpdateRegistrationUITask::Start() {
  service_worker_context()->StoreRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      registration_id_.origin().GetURL(),
      {{TitleKey(registration_id_.unique_id()), updated_title_}},
      base::BindOnce(&UpdateRegistrationUITask::DidUpdateTitle,
                     weak_factory_.GetWeakPtr()));
}

void UpdateRegistrationUITask::DidUpdateTitle(
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      break;
    case DatabaseStatus::kFailed:
    case DatabaseStatus::kNotFound:
      FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
      return;
  }

  for (auto& observer : data_manager()->observers())
    observer.OnUpdatedUI(registration_id_, updated_title_);

  FinishWithError(blink::mojom::BackgroundFetchError::NONE);
}

void UpdateRegistrationUITask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  std::move(callback_).Run(error);
  Finished();  // Destroys |this|.
}

}  // namespace background_fetch

}  // namespace content
