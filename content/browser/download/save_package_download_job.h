// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_DOWNLOAD_JOB_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_DOWNLOAD_JOB_H_

#include "base/macros.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_job.h"
#include "content/browser/download/download_request_handle.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT SavePackageDownloadJob : public DownloadJob {
 public:
  SavePackageDownloadJob(
      DownloadItemImpl* download_item,
      std::unique_ptr<DownloadRequestHandleInterface> request_handle);
  ~SavePackageDownloadJob() override;

  // DownloadJob implementation.
  bool IsSavePackageDownload() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SavePackageDownloadJob);
};

}  //  namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_DOWNLOAD_JOB_H_
