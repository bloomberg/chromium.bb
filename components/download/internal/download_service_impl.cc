// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/download_service_impl.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "components/download/internal/client_set.h"
#include "components/download/internal/config.h"
#include "components/download/internal/controller_impl.h"
#include "components/download/internal/download_driver.h"
#include "components/download/internal/download_store.h"
#include "components/download/internal/model_impl.h"
#include "components/download/internal/proto/entry.pb.h"
#include "components/download/internal/stats.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace download {
namespace {
const char kEntryDBStorageDir[] = "EntryDB";
}  // namespace

// static
DownloadService* DownloadService::Create(
    std::unique_ptr<DownloadClientMap> clients,
    const base::FilePath& storage_dir,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner) {
  auto entry_db_storage_dir = storage_dir.AppendASCII(kEntryDBStorageDir);
  auto entry_db =
      base::MakeUnique<leveldb_proto::ProtoDatabaseImpl<protodb::Entry>>(
          background_task_runner);

  auto store = base::MakeUnique<DownloadStore>(entry_db_storage_dir,
                                               std::move(entry_db));
  auto client_set = base::MakeUnique<ClientSet>(std::move(clients));
  auto config = Configuration::CreateFromFinch();
  auto driver = base::WrapUnique<DownloadDriver>(nullptr);
  auto model = base::MakeUnique<ModelImpl>(std::move(store));

  std::unique_ptr<Controller> controller =
      base::MakeUnique<ControllerImpl>(std::move(client_set), std::move(config),
                                       std::move(driver), std::move(model));

  return new DownloadServiceImpl(std::move(controller));
}

DownloadServiceImpl::DownloadServiceImpl(std::unique_ptr<Controller> controller)
    : controller_(std::move(controller)) {
  controller_->Initialize();
}

DownloadServiceImpl::~DownloadServiceImpl() = default;

DownloadService::ServiceStatus DownloadServiceImpl::GetStatus() {
  if (!controller_->GetStartupStatus().Complete())
    return DownloadService::ServiceStatus::STARTING_UP;

  if (!controller_->GetStartupStatus().Ok())
    return DownloadService::ServiceStatus::UNAVAILABLE;

  return DownloadService::ServiceStatus::READY;
}

void DownloadServiceImpl::StartDownload(const DownloadParams& download_params) {
  stats::LogServiceApiAction(download_params.client,
                             stats::ServiceApiAction::START_DOWNLOAD);
  controller_->StartDownload(download_params);
}

void DownloadServiceImpl::PauseDownload(const std::string& guid) {
  stats::LogServiceApiAction(controller_->GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::PAUSE_DOWNLOAD);
  controller_->PauseDownload(guid);
}

void DownloadServiceImpl::ResumeDownload(const std::string& guid) {
  stats::LogServiceApiAction(controller_->GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::RESUME_DOWNLOAD);
  controller_->ResumeDownload(guid);
}

void DownloadServiceImpl::CancelDownload(const std::string& guid) {
  stats::LogServiceApiAction(controller_->GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::CANCEL_DOWNLOAD);
  controller_->CancelDownload(guid);
}

void DownloadServiceImpl::ChangeDownloadCriteria(
    const std::string& guid,
    const SchedulingParams& params) {
  stats::LogServiceApiAction(controller_->GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::CHANGE_CRITERIA);
  controller_->ChangeDownloadCriteria(guid, params);
}

}  // namespace download
