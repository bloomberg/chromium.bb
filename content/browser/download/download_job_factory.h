// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_JOB_FACTORY_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_JOB_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/download/public/common/download_create_info.h"

namespace download {
class DownloadItem;
class DownloadJob;
class DownloadRequestHandleInterface;
}

namespace net {
class URLRequestContextGetter;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

// Factory class to create different kinds of DownloadJob.
class DownloadJobFactory {
 public:
  static std::unique_ptr<download::DownloadJob> CreateJob(
      download::DownloadItem* download_item,
      std::unique_ptr<download::DownloadRequestHandleInterface> req_handle,
      const download::DownloadCreateInfo& create_info,
      bool is_save_package_download,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      net::URLRequestContextGetter* url_request_context_getter);

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadJobFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_JOB_FACTORY_H_
