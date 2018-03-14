// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_DOWNLOAD_JOB_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_DOWNLOAD_JOB_H_

#include "base/macros.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_job.h"
#include "components/download/public/common/download_request_handle_interface.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT SavePackageDownloadJob : public download::DownloadJob {
 public:
  SavePackageDownloadJob(
      download::DownloadItem* download_item,
      std::unique_ptr<download::DownloadRequestHandleInterface> request_handle);
  ~SavePackageDownloadJob() override;

  // DownloadJob implementation.
  bool IsSavePackageDownload() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SavePackageDownloadJob);
};

}  //  namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_DOWNLOAD_JOB_H_
