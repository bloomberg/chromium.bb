// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_RESOURCE_DOWNLOADER_
#define CONTENT_BROWSER_DOWNLOAD_RESOURCE_DOWNLOADER_

#include "content/browser/download/download_response_handler.h"
#include "content/browser/download/url_download_handler.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/url_loader.mojom.h"

namespace content {

class ThrottlingURLLoader;
class URLLoaderFactoryGetter;

// Class for handing the download of a url.
class ResourceDownloader : public UrlDownloadHandler,
                           public DownloadResponseHandler::Delegate {
 public:
  // Called to start a download, must be called on IO thread.
  static std::unique_ptr<ResourceDownloader> BeginDownload(
      base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
      std::unique_ptr<DownloadUrlParameters> download_url_parameters,
      std::unique_ptr<ResourceRequest> request,
      scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
      uint32_t download_id,
      bool is_parallel_request);

  ResourceDownloader(
      base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
      std::unique_ptr<DownloadUrlParameters> download_url_parameters,
      scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
      uint32_t download_id,
      bool is_parallel_request);
  ~ResourceDownloader() override;

  // DownloadResponseHandler::Delegate
  void OnResponseStarted(
      std::unique_ptr<DownloadCreateInfo> download_create_info,
      mojo::ScopedDataPipeConsumerHandle body) override;

 private:
  // Helper method to start the network request.
  void Start(std::unique_ptr<ResourceRequest> request);

  base::WeakPtr<UrlDownloadHandler::Delegate> delegate_;

  // URLLoader for sending out the request.
  std::unique_ptr<ThrottlingURLLoader> url_loader_;

  // Parameters for constructing the ResourceRequest.
  std::unique_ptr<DownloadUrlParameters> download_url_parameters_;

  // Object for retrieving the URLLoaderFactory.
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;

  // Object for handing the server response.
  DownloadResponseHandler response_handler_;

  // ID of the download, or DownloadItem::kInvalidId if this is a new
  // download.
  uint32_t download_id_;

  base::WeakPtrFactory<ResourceDownloader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDownloader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_RESOURCE_DOWNLOADER_
