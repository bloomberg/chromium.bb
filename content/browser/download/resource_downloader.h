// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_RESOURCE_DOWNLOADER_
#define CONTENT_BROWSER_DOWNLOAD_RESOURCE_DOWNLOADER_

#include "content/browser/download/download_response_handler.h"
#include "content/browser/download/url_download_handler.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/url_loader.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

class ThrottlingURLLoader;

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
      scoped_refptr<storage::FileSystemContext> file_system_context,
      uint32_t download_id,
      bool is_parallel_request);

  // Called to intercept a navigation response.
  static std::unique_ptr<ResourceDownloader> InterceptNavigationResponse(
      base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
      std::unique_ptr<ResourceRequest> resource_request,
      const scoped_refptr<ResourceResponse>& response,
      mojo::ScopedDataPipeConsumerHandle consumer_handle,
      const SSLStatus& ssl_status,
      std::unique_ptr<ThrottlingURLLoader> url_loader,
      base::Optional<ResourceRequestCompletionStatus> completion_status);

  ResourceDownloader(base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
                     std::unique_ptr<ResourceRequest> resource_request,
                     std::unique_ptr<DownloadSaveInfo> save_info,
                     uint32_t download_id,
                     std::string guid,
                     bool is_parallel_request,
                     bool is_transient,
                     bool fetch_error_body);
  ResourceDownloader(base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
                     std::unique_ptr<ThrottlingURLLoader> url_loader);
  ~ResourceDownloader() override;

  // DownloadResponseHandler::Delegate
  void OnResponseStarted(
      std::unique_ptr<DownloadCreateInfo> download_create_info,
      mojom::DownloadStreamHandlePtr stream_handle) override;
  void OnReceiveRedirect() override;

 private:
  // Helper method to start the network request.
  void Start(mojom::URLLoaderFactoryPtr* factory,
             scoped_refptr<storage::FileSystemContext> file_system_context,
             std::unique_ptr<DownloadUrlParameters> download_url_parameters);

  // Intercepts the navigation response and takes ownership of the |url_loader|.
  void InterceptResponse(
      std::unique_ptr<ThrottlingURLLoader> url_loader,
      const scoped_refptr<ResourceResponse>& response,
      mojo::ScopedDataPipeConsumerHandle consumer_handle,
      const SSLStatus& ssl_status,
      base::Optional<ResourceRequestCompletionStatus> completion_status);

  base::WeakPtr<UrlDownloadHandler::Delegate> delegate_;

  // URLLoader for sending out the request.
  std::unique_ptr<ThrottlingURLLoader> url_loader_;

  // The ResourceRequest for this object.
  std::unique_ptr<ResourceRequest> resource_request_;

  // Object for handing the server response.
  DownloadResponseHandler response_handler_;

  // URLLoaderClient binding for loading a blob.
  mojo::Binding<mojom::URLLoaderClient> blob_client_binding_;

  // ID of the download, or DownloadItem::kInvalidId if this is a new
  // download.
  uint32_t download_id_;

  // GUID of the download, or empty if this is a new download.
  std::string guid_;

  // Callback to run after download starts.
  DownloadUrlParameters::OnStartedCallback callback_;

  base::WeakPtrFactory<ResourceDownloader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDownloader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_RESOURCE_DOWNLOADER_
