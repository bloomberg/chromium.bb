// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_DOWNLOAD_SERVICE_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_DOWNLOAD_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/download/internal/config.h"
#include "components/download/public/download_service.h"

namespace download {

struct DownloadParams;
struct SchedulingParams;

// The internal implementation of the DownloadService.
class DownloadServiceImpl : public DownloadService {
 public:
  DownloadServiceImpl(std::unique_ptr<Configuration> config);
  ~DownloadServiceImpl() override;

  // DownloadService implementation.
  void StartDownload(const DownloadParams& download_params) override;
  void PauseDownload(const std::string& guid) override;
  void ResumeDownload(const std::string& guid) override;
  void CancelDownload(const std::string& guid) override;
  void ChangeDownloadCriteria(const std::string& guid,
                              const SchedulingParams& params) override;

 private:
  std::unique_ptr<Configuration> config_;

  DISALLOW_COPY_AND_ASSIGN(DownloadServiceImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_DOWNLOAD_SERVICE_IMPL_H_
