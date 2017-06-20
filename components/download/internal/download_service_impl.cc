// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/download_service_impl.h"

#include "base/strings/string_util.h"
#include "components/download/internal/controller.h"
#include "components/download/internal/startup_status.h"
#include "components/download/internal/stats.h"

namespace download {

DownloadServiceImpl::DownloadServiceImpl(std::unique_ptr<Configuration> config,
                                         std::unique_ptr<Controller> controller)
    : config_(std::move(config)),
      controller_(std::move(controller)),
      service_config_(config_.get()) {
  controller_->Initialize();
}

DownloadServiceImpl::~DownloadServiceImpl() = default;

const ServiceConfig& DownloadServiceImpl::GetConfig() {
  return service_config_;
}

void DownloadServiceImpl::OnStartScheduledTask(
    DownloadTaskType task_type,
    const TaskFinishedCallback& callback) {
  controller_->OnStartScheduledTask(task_type, callback);
}

bool DownloadServiceImpl::OnStopScheduledTask(DownloadTaskType task_type) {
  return controller_->OnStopScheduledTask(task_type);
}

DownloadService::ServiceStatus DownloadServiceImpl::GetStatus() {
  if (!controller_->GetStartupStatus()->Complete())
    return DownloadService::ServiceStatus::STARTING_UP;

  if (!controller_->GetStartupStatus()->Ok())
    return DownloadService::ServiceStatus::UNAVAILABLE;

  return DownloadService::ServiceStatus::READY;
}

void DownloadServiceImpl::StartDownload(const DownloadParams& download_params) {
  stats::LogServiceApiAction(download_params.client,
                             stats::ServiceApiAction::START_DOWNLOAD);
  DCHECK_EQ(download_params.guid, base::ToUpperASCII(download_params.guid));
  controller_->StartDownload(download_params);
}

void DownloadServiceImpl::PauseDownload(const std::string& guid) {
  stats::LogServiceApiAction(controller_->GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::PAUSE_DOWNLOAD);
  DCHECK_EQ(guid, base::ToUpperASCII(guid));
  controller_->PauseDownload(guid);
}

void DownloadServiceImpl::ResumeDownload(const std::string& guid) {
  stats::LogServiceApiAction(controller_->GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::RESUME_DOWNLOAD);
  DCHECK_EQ(guid, base::ToUpperASCII(guid));
  controller_->ResumeDownload(guid);
}

void DownloadServiceImpl::CancelDownload(const std::string& guid) {
  stats::LogServiceApiAction(controller_->GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::CANCEL_DOWNLOAD);
  DCHECK_EQ(guid, base::ToUpperASCII(guid));
  controller_->CancelDownload(guid);
}

void DownloadServiceImpl::ChangeDownloadCriteria(
    const std::string& guid,
    const SchedulingParams& params) {
  stats::LogServiceApiAction(controller_->GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::CHANGE_CRITERIA);
  DCHECK_EQ(guid, base::ToUpperASCII(guid));
  controller_->ChangeDownloadCriteria(guid, params);
}

}  // namespace download
