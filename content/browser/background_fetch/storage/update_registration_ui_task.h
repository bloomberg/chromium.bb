// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_UPDATE_REGISTRATION_UI_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_UPDATE_REGISTRATION_UI_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "content/common/service_worker/service_worker_status_code.h"

namespace content {

namespace background_fetch {

// Updates Background Fetch registration entries in the database.
class UpdateRegistrationUITask : public DatabaseTask {
 public:
  using UpdateRegistrationUICallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError)>;

  UpdateRegistrationUITask(BackgroundFetchDataManager* data_manager,
                           int64_t service_worker_registration_id,
                           const url::Origin& origin,
                           const std::string& unique_id,
                           const std::string& updated_title,
                           UpdateRegistrationUICallback callback);

  ~UpdateRegistrationUITask() override;

  void Start() override;

 private:
  void DidGetUniqueId(const std::vector<std::string>& data,
                      ServiceWorkerStatusCode status);

  void UpdateUI(const std::string& serialized_registration_proto);

  void DidUpdateUI(ServiceWorkerStatusCode status);

  int64_t service_worker_registration_id_;
  url::Origin origin_;
  std::string unique_id_;
  std::string updated_title_;

  UpdateRegistrationUICallback callback_;

  base::WeakPtrFactory<UpdateRegistrationUITask>
      weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(UpdateRegistrationUITask);
};

}  // namespace background_fetch

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_UPDATE_REGISTRATION_UI_TASK_H_
