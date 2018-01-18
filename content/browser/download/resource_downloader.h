// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_RESOURCE_DOWNLOADER_
#define CONTENT_BROWSER_DOWNLOAD_RESOURCE_DOWNLOADER_

#include "content/browser/download/download_response_handler.h"
#include "content/browser/download/url_download_handler.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/ssl_status.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/interfaces/url_loader.mojom.h"

namespace storage {
class FileSystemContext;
}

namespace content {

// Class for handing the download of a url.
class ResourceDownloader : public UrlDownloadHandler,
                           public DownloadResponseHandler::Delegate {
 public:
  // Called to start a download, must be called on IO thread.
  static std::unique_ptr<ResourceDownloader> BeginDownload(
      base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
      std::unique_ptr<DownloadUrlParameters> download_url_parameters,
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      const ResourceRequestInfo::WebContentsGetter& web_contents_getter,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_referrer_url,
      uint32_t download_id,
      bool is_parallel_request);

  // Create a ResourceDownloader from a navigation that turns to be a download.
  // No URLLoader is created, but the URLLoaderClient implementation is
  // transferred.
  static std::unique_ptr<ResourceDownloader> InterceptNavigationResponse(
      base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
      std::unique_ptr<network::ResourceRequest> resource_request,
      const ResourceRequestInfo::WebContentsGetter& web_contents_getter,
      std::vector<GURL> url_chain,
      const base::Optional<std::string>& suggested_filename,
      const scoped_refptr<network::ResourceResponse>& response,
      net::CertStatus cert_status,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints);

  ResourceDownloader(
      base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
      std::unique_ptr<network::ResourceRequest> resource_request,
      const ResourceRequestInfo::WebContentsGetter& web_contents_getter,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_referrer_url,
      uint32_t download_id);
  ~ResourceDownloader() override;

  // DownloadResponseHandler::Delegate
  void OnResponseStarted(
      std::unique_ptr<DownloadCreateInfo> download_create_info,
      mojom::DownloadStreamHandlePtr stream_handle) override;
  void OnReceiveRedirect() override;

 private:
  // Helper method to start the network request.
  void Start(scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
             scoped_refptr<storage::FileSystemContext> file_system_context,
             std::unique_ptr<DownloadUrlParameters> download_url_parameters,
             bool is_parallel_request);

  // Intercepts the navigation response.
  void InterceptResponse(
      const scoped_refptr<network::ResourceResponse>& response,
      std::vector<GURL> url_chain,
      const base::Optional<std::string>& suggested_filename,
      net::CertStatus cert_status,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints);

  base::WeakPtr<UrlDownloadHandler::Delegate> delegate_;

  // The ResourceRequest for this object.
  std::unique_ptr<network::ResourceRequest> resource_request_;

  // Object that will handle the response.
  std::unique_ptr<network::mojom::URLLoaderClient> url_loader_client_;

  // URLLoaderClient binding. It sends any requests to the |url_loader_client_|.
  std::unique_ptr<mojo::Binding<network::mojom::URLLoaderClient>>
      url_loader_client_binding_;

  // URLLoader for sending out the request.
  network::mojom::URLLoaderPtr url_loader_;

  // ID of the download, or DownloadItem::kInvalidId if this is a new
  // download.
  uint32_t download_id_;

  // GUID of the download, or empty if this is a new download.
  std::string guid_;

  // Callback to run after download starts.
  DownloadUrlParameters::OnStartedCallback callback_;

  // Used to get WebContents in browser process.
  ResourceRequestInfo::WebContentsGetter web_contents_getter_;

  // Site URL for the site instance that initiated the download.
  GURL site_url_;

  // The URL of the tab that started us.
  GURL tab_url_;

  // The referrer URL of the tab that started us.
  GURL tab_referrer_url_;

  // URLLoader status when intercepting the navigation request.
  base::Optional<network::URLLoaderCompletionStatus> url_loader_status_;

  base::WeakPtrFactory<ResourceDownloader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDownloader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_RESOURCE_DOWNLOADER_
