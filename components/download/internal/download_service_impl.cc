// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/download_service_impl.h"

namespace download {

// static
DownloadService* DownloadService::Create(
    const base::FilePath& storage_dir,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner) {
  return new DownloadServiceImpl(Configuration::CreateFromFinch());
}

DownloadServiceImpl::DownloadServiceImpl(std::unique_ptr<Configuration> config)
    : config_(std::move(config)) {}

DownloadServiceImpl::~DownloadServiceImpl() = default;

void DownloadServiceImpl::StartDownload(const DownloadParams& download_params) {
}
void DownloadServiceImpl::PauseDownload(const std::string& guid) {}
void DownloadServiceImpl::ResumeDownload(const std::string& guid) {}
void DownloadServiceImpl::CancelDownload(const std::string& guid) {}
void DownloadServiceImpl::ChangeDownloadCriteria(
    const std::string& guid,
    const SchedulingParams& params) {}

}  // namespace download
