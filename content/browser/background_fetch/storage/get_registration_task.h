// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_REGISTRATION_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_REGISTRATION_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "url/origin.h"

namespace content {

namespace background_fetch {

// Gets an active Background Fetch registration entry from the database.
class GetRegistrationTask : public DatabaseTask {
 public:
  using GetRegistrationCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError,
                              std::unique_ptr<BackgroundFetchRegistration>)>;

  GetRegistrationTask(BackgroundFetchDataManager* data_manager,
                      int64_t service_worker_registration_id,
                      const url::Origin& origin,
                      const std::string& developer_id,
                      GetRegistrationCallback callback);

  ~GetRegistrationTask() override;

  // DatabaseTask implementation:
  void Start() override;

 private:
  void DidGetUniqueId(const std::vector<std::string>& data,
                      ServiceWorkerStatusCode status);

  void DidGetRegistration(const std::vector<std::string>& data,
                          ServiceWorkerStatusCode status);

  void InitializeRegistration(
      const proto::BackgroundFetchRegistration& registration_proto,
      BackgroundFetchRegistration* registration);

  void CreateRegistration(const std::string& metadata);

  int64_t service_worker_registration_id_;
  url::Origin origin_;
  std::string developer_id_;

  GetRegistrationCallback callback_;

  base::WeakPtrFactory<GetRegistrationTask> weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(GetRegistrationTask);
};

}  // namespace background_fetch

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_REGISTRATION_TASK_H_
