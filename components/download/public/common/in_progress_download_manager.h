// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_IN_PROGRESS_DOWNLOAD_MANAGER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_IN_PROGRESS_DOWNLOAD_MANAGER_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_item_impl_delegate.h"
#include "components/download/public/common/url_download_handler.h"
#include "url/gurl.h"

namespace network {
struct ResourceResponse;
}

namespace download {

class DownloadURLLoaderFactoryGetter;
class DownloadUrlParameters;

// Manager for handling all active downloads.
class COMPONENTS_DOWNLOAD_EXPORT InProgressDownloadManager
    : public UrlDownloadHandler::Delegate,
      public DownloadItemImplDelegate {
 public:
  InProgressDownloadManager(UrlDownloadHandler::Delegate* delegate);
  ~InProgressDownloadManager() override;

  // Called to start a download.
  void StartDownload(
      std::unique_ptr<DownloadUrlParameters> params,
      scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
      uint32_t download_id,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_referrer_url);

  // Intercepts a download from navigation.
  void InterceptDownloadFromNavigation(
      std::unique_ptr<network::ResourceRequest> resource_request,
      int render_process_id,
      int render_frame_id,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_referrer_url,
      std::vector<GURL> url_chain,
      const base::Optional<std::string>& suggested_filename,
      scoped_refptr<network::ResourceResponse> response,
      net::CertStatus cert_status,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter);

  // Shutting down the manager and stop all downloads.
  void ShutDown();

 private:
  // UrlDownloadHandler::Delegate implementations.
  void OnUrlDownloadStarted(
      std::unique_ptr<DownloadCreateInfo> download_create_info,
      std::unique_ptr<InputStream> input_stream,
      scoped_refptr<DownloadURLLoaderFactoryGetter> shared_url_loader_factory,
      const DownloadUrlParameters::OnStartedCallback& callback) override;
  void OnUrlDownloadStopped(UrlDownloadHandler* downloader) override;
  void OnUrlDownloadHandlerCreated(
      UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader) override;

  // Overridden from DownloadItemImplDelegate.
  void ResumeInterruptedDownload(std::unique_ptr<DownloadUrlParameters> params,
                                 uint32_t id,
                                 const GURL& site_url) override;

  // Active download handlers.
  std::vector<UrlDownloadHandler::UniqueUrlDownloadHandlerPtr>
      url_download_handlers_;

  // Delegate to call when download starts. Only OnUrlDownloadStarted() is being
  // passed through for now as we need content layer to provide default download
  // dir.
  // TODO(qinmin): remove this once this class drives all active download.
  UrlDownloadHandler::Delegate* delegate_;

  base::WeakPtrFactory<InProgressDownloadManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InProgressDownloadManager);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_IN_PROGRESS_DOWNLOAD_MANAGER_H_
