// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_JOB_FACTORY_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_JOB_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/download/download_create_info.h"

namespace content {

class DownloadItemImpl;
class DownloadJob;
class DownloadRequestHandleInterface;

// Factory class to create different kinds of DownloadJob.
class DownloadJobFactory {
 public:
  static std::unique_ptr<DownloadJob> CreateJob(
      DownloadItemImpl* download_item,
      std::unique_ptr<DownloadRequestHandleInterface> req_handle,
      const DownloadCreateInfo& create_info);

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadJobFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_JOB_FACTORY_H_
