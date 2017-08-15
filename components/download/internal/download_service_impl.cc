// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/download_service_impl.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "components/download/internal/controller.h"
#include "components/download/internal/startup_status.h"
#include "components/download/internal/stats.h"

namespace download {

DownloadServiceImpl::DownloadServiceImpl(std::unique_ptr<Configuration> config,
                                         std::unique_ptr<Controller> controller)
    : config_(std::move(config)),
      controller_(std::move(controller)),
      service_config_(config_.get()),
      startup_completed_(false) {
  controller_->Initialize(base::Bind(
      &DownloadServiceImpl::OnControllerInitialized, base::Unretained(this)));
}

DownloadServiceImpl::~DownloadServiceImpl() = default;

const ServiceConfig& DownloadServiceImpl::GetConfig() {
  return service_config_;
}

void DownloadServiceImpl::OnStartScheduledTask(
    DownloadTaskType task_type,
    const TaskFinishedCallback& callback) {
  if (startup_completed_) {
    controller_->OnStartScheduledTask(task_type, callback);
    return;
  }

  pending_tasks_[task_type] =
      base::Bind(&Controller::OnStartScheduledTask,
                 base::Unretained(controller_.get()), task_type, callback);
}

bool DownloadServiceImpl::OnStopScheduledTask(DownloadTaskType task_type) {
  if (startup_completed_) {
    return controller_->OnStopScheduledTask(task_type);
  }

  auto iter = pending_tasks_.find(task_type);
  if (iter != pending_tasks_.end()) {
    // We still need to run the callback in order to properly cleanup and notify
    // the system by running the respective task finished callbacks.
    iter->second.Run();
    pending_tasks_.erase(iter);
  }

  return true;
}

DownloadService::ServiceStatus DownloadServiceImpl::GetStatus() {
  switch (controller_->GetState()) {
    case Controller::State::CREATED:       // Intentional fallthrough.
    case Controller::State::INITIALIZING:  // Intentional fallthrough.
    case Controller::State::RECOVERING:
      return DownloadService::ServiceStatus::STARTING_UP;
    case Controller::State::READY:
      return DownloadService::ServiceStatus::READY;
    case Controller::State::UNAVAILABLE:  // Intentional fallthrough.
    default:
      return DownloadService::ServiceStatus::UNAVAILABLE;
  }
}

void DownloadServiceImpl::StartDownload(const DownloadParams& download_params) {
  stats::LogServiceApiAction(download_params.client,
                             stats::ServiceApiAction::START_DOWNLOAD);
  stats::LogDownloadParams(download_params);
  if (startup_completed_) {
    controller_->StartDownload(download_params);
  } else {
    pending_actions_.push_back(base::Bind(&Controller::StartDownload,
                                          base::Unretained(controller_.get()),
                                          download_params));
  }
}

void DownloadServiceImpl::PauseDownload(const std::string& guid) {
  stats::LogServiceApiAction(controller_->GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::PAUSE_DOWNLOAD);
  if (startup_completed_) {
    controller_->PauseDownload(guid);
  } else {
    pending_actions_.push_back(base::Bind(
        &Controller::PauseDownload, base::Unretained(controller_.get()), guid));
  }
}

void DownloadServiceImpl::ResumeDownload(const std::string& guid) {
  stats::LogServiceApiAction(controller_->GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::RESUME_DOWNLOAD);
  if (startup_completed_) {
    controller_->ResumeDownload(guid);
  } else {
    pending_actions_.push_back(base::Bind(&Controller::ResumeDownload,
                                          base::Unretained(controller_.get()),
                                          guid));
  }
}

void DownloadServiceImpl::CancelDownload(const std::string& guid) {
  stats::LogServiceApiAction(controller_->GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::CANCEL_DOWNLOAD);
  if (startup_completed_) {
    controller_->CancelDownload(guid);
  } else {
    pending_actions_.push_back(base::Bind(&Controller::CancelDownload,
                                          base::Unretained(controller_.get()),
                                          guid));
  }
}

void DownloadServiceImpl::ChangeDownloadCriteria(
    const std::string& guid,
    const SchedulingParams& params) {
  stats::LogServiceApiAction(controller_->GetOwnerOfDownload(guid),
                             stats::ServiceApiAction::CHANGE_CRITERIA);
  if (startup_completed_) {
    controller_->ChangeDownloadCriteria(guid, params);
  } else {
    pending_actions_.push_back(base::Bind(&Controller::ChangeDownloadCriteria,
                                          base::Unretained(controller_.get()),
                                          guid, params));
  }
}

void DownloadServiceImpl::OnControllerInitialized() {
  while (!pending_actions_.empty()) {
    auto callback = pending_actions_.front();
    callback.Run();
    pending_actions_.pop_front();
  }

  while (!pending_tasks_.empty()) {
    auto iter = pending_tasks_.begin();
    iter->second.Run();
    pending_tasks_.erase(iter);
  }

  startup_completed_ = true;
}

}  // namespace download
