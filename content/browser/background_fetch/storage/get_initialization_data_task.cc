// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_initialization_data_task.h"

#include "base/barrier_closure.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

namespace content {

namespace background_fetch {

namespace {

// Base class with all the common implementation for the SubTasks
// needed in this file.
class InitializationSubTask : public DatabaseTask {
 public:
  // Holds data used by all SubTasks.
  struct SubTaskInit {
    SubTaskInit() = delete;
    ~SubTaskInit() = default;

    // Service Worker Database metadata.
    int64_t service_worker_registration_id;
    std::string unique_id;

    // The results to report.
    BackgroundFetchInitializationData* initialization_data;
    blink::mojom::BackgroundFetchError* error;
  };

  InitializationSubTask(DatabaseTaskHost* host,
                        const SubTaskInit& sub_task_init,
                        base::OnceClosure done_closure)
      : DatabaseTask(host),
        sub_task_init_(sub_task_init),
        done_closure_(std::move(done_closure)) {
    DCHECK(sub_task_init_.initialization_data);
    DCHECK(sub_task_init_.error);
  }

  ~InitializationSubTask() override = default;

 protected:
  void FinishTask() {
    std::move(done_closure_).Run();
    Finished();  // Destroys |this|.
  }

  SubTaskInit& sub_task_init() { return sub_task_init_; }

 private:
  SubTaskInit sub_task_init_;
  base::OnceClosure done_closure_;

  DISALLOW_COPY_AND_ASSIGN(InitializationSubTask);
};

// Fills the BackgroundFetchInitializationData with the most recent UI title.
class GetTitleTask : public InitializationSubTask {
 public:
  GetTitleTask(DatabaseTaskHost* host,
               const SubTaskInit& sub_task_init,
               base::OnceClosure done_closure)
      : InitializationSubTask(host, sub_task_init, std::move(done_closure)),
        weak_factory_(this) {}

  ~GetTitleTask() override = default;

  void Start() override {
    service_worker_context()->GetRegistrationUserData(
        sub_task_init().service_worker_registration_id,
        {TitleKey(sub_task_init().unique_id)},
        base::BindOnce(&GetTitleTask::DidGetTitle, weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetTitle(const std::vector<std::string>& data,
                   blink::ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kFailed:
        *sub_task_init().error =
            blink::mojom::BackgroundFetchError::STORAGE_ERROR;
        FinishTask();
        return;
      case DatabaseStatus::kNotFound:
      case DatabaseStatus::kOk:
        break;
    }

    if (!data.empty())
      sub_task_init().initialization_data->ui_title = data.front();
    FinishTask();
  }

  base::WeakPtrFactory<GetTitleTask> weak_factory_;  // Keep as last.
};

// Fills the BackgroundFetchInitializationData with the number of completed
// requests.
class GetCompletedRequestsTask : public InitializationSubTask {
 public:
  GetCompletedRequestsTask(DatabaseTaskHost* host,
                           const SubTaskInit& sub_task_init,
                           base::OnceClosure done_closure)
      : InitializationSubTask(host, sub_task_init, std::move(done_closure)),
        weak_factory_(this) {}

  ~GetCompletedRequestsTask() override = default;

  void Start() override {
    service_worker_context()->GetRegistrationUserDataByKeyPrefix(
        sub_task_init().service_worker_registration_id,
        CompletedRequestKeyPrefix(sub_task_init().unique_id),
        base::BindOnce(&GetCompletedRequestsTask::DidGetCompletedRequests,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetCompletedRequests(const std::vector<std::string>& data,
                               blink::ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kFailed:
        *sub_task_init().error =
            blink::mojom::BackgroundFetchError::STORAGE_ERROR;
        FinishTask();
        return;
      case DatabaseStatus::kNotFound:
      case DatabaseStatus::kOk:
        break;
    }

    sub_task_init().initialization_data->num_completed_requests = data.size();
    FinishTask();
  }

  base::WeakPtrFactory<GetCompletedRequestsTask>
      weak_factory_;  // Keep as last.
};

// Fills the BackgroundFetchInitializationData with the guids of active
// (previously started) requests.
class GetActiveRequestsTask : public InitializationSubTask {
 public:
  GetActiveRequestsTask(DatabaseTaskHost* host,
                        const SubTaskInit& sub_task_init,
                        base::OnceClosure done_closure)
      : InitializationSubTask(host, sub_task_init, std::move(done_closure)),
        weak_factory_(this) {}

  ~GetActiveRequestsTask() override = default;

  void Start() override {
    service_worker_context()->GetRegistrationUserDataByKeyPrefix(
        sub_task_init().service_worker_registration_id,
        ActiveRequestKeyPrefix(sub_task_init().unique_id),
        base::BindOnce(&GetActiveRequestsTask::DidGetActiveRequests,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetActiveRequests(const std::vector<std::string>& data,
                            blink::ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kFailed:
        *sub_task_init().error =
            blink::mojom::BackgroundFetchError::STORAGE_ERROR;
        FinishTask();
        return;
      case DatabaseStatus::kNotFound:
      case DatabaseStatus::kOk:
        break;
    }

    for (const std::string& serialized_active_request : data) {
      proto::BackgroundFetchActiveRequest active_request;
      if (!active_request.ParseFromString(serialized_active_request)) {
        *sub_task_init().error =
            blink::mojom::BackgroundFetchError::STORAGE_ERROR;
        continue;
      }
      DCHECK_EQ(sub_task_init().unique_id, active_request.unique_id());
      sub_task_init().initialization_data->active_fetch_guids.push_back(
          active_request.download_guid());
    }

    FinishTask();
  }

  base::WeakPtrFactory<GetActiveRequestsTask> weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(GetActiveRequestsTask);
};

// Deserializes the icon and creates an SkBitmap from it.
class DeserializeIconTask : public InitializationSubTask {
 public:
  DeserializeIconTask(DatabaseTaskHost* host,
                      const SubTaskInit& sub_task_init,
                      base::OnceClosure done_closure,
                      std::string* serialized_icon)
      : InitializationSubTask(host, sub_task_init, std::move(done_closure)),
        serialized_icon_(serialized_icon),
        weak_factory_(this) {}

  ~DeserializeIconTask() override = default;

  void Start() override {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
         base::TaskPriority::BACKGROUND},
        base::BindOnce(&DeserializeIcon, std::move(serialized_icon_)),
        base::BindOnce(&DeserializeIconTask::StoreIcon,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  static SkBitmap DeserializeIcon(
      std::unique_ptr<std::string> serialized_icon) {
    return gfx::Image::CreateFrom1xPNGBytes(
               reinterpret_cast<const unsigned char*>(serialized_icon->c_str()),
               serialized_icon->size())
        .AsBitmap();
  }

  void StoreIcon(SkBitmap icon) {
    sub_task_init().initialization_data->icon = std::move(icon);
    FinishTask();
  }

  std::unique_ptr<std::string> serialized_icon_;

  base::WeakPtrFactory<DeserializeIconTask> weak_factory_;  // Keep as last.
};

// Fills the BackgroundFetchInitializationData with all the relevant information
// stored in the BackgroundFetchMetadata proto.
class FillFromMetadataTask : public InitializationSubTask {
 public:
  FillFromMetadataTask(DatabaseTaskHost* host,
                       const SubTaskInit& sub_task_init,
                       base::OnceClosure done_closure)
      : InitializationSubTask(host, sub_task_init, std::move(done_closure)),
        weak_factory_(this) {}

  ~FillFromMetadataTask() override = default;

  void Start() override {
    service_worker_context()->GetRegistrationUserDataByKeyPrefix(
        sub_task_init().service_worker_registration_id,
        {RegistrationKey(sub_task_init().unique_id)},
        base::BindOnce(&FillFromMetadataTask::DidGetMetadata,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetMetadata(const std::vector<std::string>& data,
                      blink::ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kFailed:
      case DatabaseStatus::kNotFound:
        *sub_task_init().error =
            blink::mojom::BackgroundFetchError::STORAGE_ERROR;
        FinishTask();
        return;
      case DatabaseStatus::kOk:
        break;
    }

    if (data.size() != 1u) {
      *sub_task_init().error =
          blink::mojom::BackgroundFetchError::STORAGE_ERROR;
      FinishTask();
      return;
    }

    proto::BackgroundFetchMetadata metadata;
    if (!metadata.ParseFromString(data[0])) {
      *sub_task_init().error =
          blink::mojom::BackgroundFetchError::STORAGE_ERROR;
      FinishTask();
      return;
    }

    if (sub_task_init().unique_id != metadata.registration().unique_id()) {
      *sub_task_init().error =
          blink::mojom::BackgroundFetchError::STORAGE_ERROR;
      FinishTask();
      return;
    }

    // Fill BackgroundFetchRegistrationId.
    sub_task_init().initialization_data->registration_id =
        BackgroundFetchRegistrationId(
            sub_task_init().service_worker_registration_id,
            url::Origin::Create(GURL(metadata.origin())),
            metadata.registration().developer_id(),
            metadata.registration().unique_id());

    // Fill BackgroundFetchRegistration.
    auto& registration = sub_task_init().initialization_data->registration;
    // TODO(crbug.com/853874): Unify conversion logic.
    registration.developer_id = metadata.registration().developer_id();
    registration.unique_id = metadata.registration().unique_id();
    registration.upload_total = metadata.registration().upload_total();
    registration.uploaded = metadata.registration().uploaded();
    registration.download_total = metadata.registration().download_total();
    registration.downloaded = metadata.registration().downloaded();

    // Total number of requests.
    sub_task_init().initialization_data->num_requests = metadata.num_fetches();

    // Fill BackgroundFetchOptions.
    auto& options = sub_task_init().initialization_data->options;
    options.title = metadata.options().title();
    options.download_total = metadata.options().download_total();
    options.icons.reserve(metadata.options().icons_size());
    for (const auto& icon : metadata.options().icons()) {
      blink::Manifest::ImageResource ir;
      ir.src = GURL(icon.src());
      ir.type = base::ASCIIToUTF16(icon.type());

      ir.sizes.reserve(icon.sizes_size());
      for (const auto& size : icon.sizes())
        ir.sizes.emplace_back(size.width(), size.height());

      ir.purpose.reserve(icon.purpose_size());
      for (auto purpose : icon.purpose()) {
        switch (purpose) {
          case proto::BackgroundFetchOptions_ImageResource_Purpose_ANY:
            ir.purpose.push_back(blink::Manifest::ImageResource::Purpose::ANY);
            break;
          case proto::BackgroundFetchOptions_ImageResource_Purpose_BADGE:
            ir.purpose.push_back(
                blink::Manifest::ImageResource::Purpose::BADGE);
            break;
        }
      }
    }

    if (!metadata.icon().empty()) {
      // Start an icon deserialization SubTask on another thread, then finish.
      AddSubTask(std::make_unique<DeserializeIconTask>(
          this, sub_task_init(),
          base::BindOnce(&FillFromMetadataTask::FinishTask,
                         weak_factory_.GetWeakPtr()),
          metadata.release_icon()));
    } else {
      // Immediately finish.
      FinishTask();
    }
  }

  base::WeakPtrFactory<FillFromMetadataTask> weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(FillFromMetadataTask);
};

// Asynchronously calls the SubTasks required to collect all the information for
// the BackgroundFetchInitializationData.
class FillBackgroundFetchInitializationDataTask : public InitializationSubTask {
 public:
  FillBackgroundFetchInitializationDataTask(DatabaseTaskHost* host,
                                            const SubTaskInit& sub_task_init,
                                            base::OnceClosure done_closure)
      : InitializationSubTask(host, sub_task_init, std::move(done_closure)),
        weak_factory_(this) {}

  ~FillBackgroundFetchInitializationDataTask() override = default;

  void Start() override {
    // We need 3 queries to get the initialization data. These are wrapped
    // in a BarrierClosure to avoid querying them serially.
    // 1. Metadata
    // 2. Active Requests
    // 3. Completed Requests
    // 4. UI Title
    base::RepeatingClosure barrier_closure = base::BarrierClosure(
        4u,
        base::BindOnce(&FillBackgroundFetchInitializationDataTask::FinishTask,
                       weak_factory_.GetWeakPtr()));
    AddSubTask(std::make_unique<FillFromMetadataTask>(this, sub_task_init(),
                                                      barrier_closure));
    AddSubTask(std::make_unique<GetCompletedRequestsTask>(this, sub_task_init(),
                                                          barrier_closure));
    AddSubTask(std::make_unique<GetActiveRequestsTask>(this, sub_task_init(),
                                                       barrier_closure));
    AddSubTask(
        std::make_unique<GetTitleTask>(this, sub_task_init(), barrier_closure));
  }

 private:
  base::WeakPtrFactory<FillBackgroundFetchInitializationDataTask>
      weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(FillBackgroundFetchInitializationDataTask);
};

}  // namespace

BackgroundFetchInitializationData::BackgroundFetchInitializationData() =
    default;

BackgroundFetchInitializationData::BackgroundFetchInitializationData(
    BackgroundFetchInitializationData&&) = default;

BackgroundFetchInitializationData::~BackgroundFetchInitializationData() =
    default;

GetInitializationDataTask::GetInitializationDataTask(
    DatabaseTaskHost* host,
    GetInitializationDataCallback callback)
    : DatabaseTask(host), callback_(std::move(callback)), weak_factory_(this) {}

GetInitializationDataTask::~GetInitializationDataTask() = default;

void GetInitializationDataTask::Start() {
  service_worker_context()->GetUserDataForAllRegistrationsByKeyPrefix(
      kActiveRegistrationUniqueIdKeyPrefix,
      base::BindOnce(&GetInitializationDataTask::DidGetRegistrations,
                     weak_factory_.GetWeakPtr()));
}

void GetInitializationDataTask::DidGetRegistrations(
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kFailed:
      error_ = blink::mojom::BackgroundFetchError::STORAGE_ERROR;
      FinishTask();
      return;
    case DatabaseStatus::kNotFound:
    case DatabaseStatus::kOk:
      break;
  }

  if (user_data.empty()) {
    FinishTask();
    return;
  }

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      user_data.size(), base::BindOnce(&GetInitializationDataTask::FinishTask,
                                       weak_factory_.GetWeakPtr()));
  for (const auto& ud : user_data) {
    auto insertion_result = initialization_data_map_.emplace(
        ud.second, BackgroundFetchInitializationData());
    DCHECK(insertion_result.second);  // Check unique_id is in fact unique.

    AddSubTask(std::make_unique<FillBackgroundFetchInitializationDataTask>(
        this,
        InitializationSubTask::SubTaskInit{
            ud.first, ud.second,
            &insertion_result.first->second /* initialization_data */, &error_},
        barrier_closure));
  }
}
void GetInitializationDataTask::FinishTask() {
  std::vector<BackgroundFetchInitializationData> results;
  results.reserve(initialization_data_map_.size());
  for (auto& id : initialization_data_map_)
    results.emplace_back(std::move(id.second));

  std::move(callback_).Run(error_, std::move(results));
  Finished();  // Destroys |this|.
}

}  // namespace background_fetch

}  // namespace content
