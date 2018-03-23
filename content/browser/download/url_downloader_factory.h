// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_FACTORY_H_
#define CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_FACTORY_H_

#include "components/download/public/common/url_download_handler_factory.h"

namespace content {

// Class for creating UrlDownloader object.
// TODO(qinmin): remove this once network service is fully enabled.
class UrlDownloaderFactory : public download::UrlDownloadHandlerFactory {
 public:
  UrlDownloaderFactory();
  ~UrlDownloaderFactory() override;

  // download::UrlDownloadHandlerFactory
  download::UrlDownloadHandler::UniqueUrlDownloadHandlerPtr
  CreateUrlDownloadHandler(
      std::unique_ptr<download::DownloadUrlParameters> params,
      base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_FACTORY_H_
