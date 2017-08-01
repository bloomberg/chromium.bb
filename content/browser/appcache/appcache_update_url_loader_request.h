// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_URL_LOADER_REQUEST_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_URL_LOADER_REQUEST_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "content/browser/appcache/appcache_update_request_base.h"
#include "content/public/common/url_loader.mojom.h"

namespace content {

// URLLoaderClient subclass for the UpdateRequestBase class. Provides
// functionality to update the AppCache using functionality provided by the
// network URL loader
class AppCacheUpdateJob::UpdateURLLoaderRequest
    : public AppCacheUpdateJob::UpdateRequestBase,
      public mojom::URLLoaderClient {
 public:
  ~UpdateURLLoaderRequest() override;

  // UpdateRequestBase overrides.
  void Start() override;
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;
  GURL GetURL() const override;
  GURL GetOriginalURL() const override;
  void SetLoadFlags(int flags) override;
  int GetLoadFlags() const override;
  std::string GetMimeType() const override;
  void SetFirstPartyForCookies(const GURL& first_party_for_cookies) override;
  void SetInitiator(const base::Optional<url::Origin>& initiator) override;
  net::HttpResponseHeaders* GetResponseHeaders() const override;
  int GetResponseCode() const override;
  net::HttpResponseInfo GetResponseInfo() const override;
  const net::URLRequestContext* GetRequestContext() const override;
  int Read(net::IOBuffer* buf, int max_bytes) override;
  int Cancel() override;

  // mojom::URLLoaderClient implementation.
  // These methods are called by the network loader.
  void OnReceiveResponse(const ResourceResponseHead& response_head,
                         const base::Optional<net::SSLInfo>& ssl_info,
                         mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override;
  void OnDataDownloaded(int64_t data_len, int64_t encoded_data_len) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const ResourceRequestCompletionStatus& status) override;

 private:
  UpdateURLLoaderRequest(net::URLRequestContext* request_context,
                         const GURL& url,
                         URLFetcher* fetcher);

  friend class AppCacheUpdateJob::UpdateRequestBase;

  URLFetcher* fetcher_;

  DISALLOW_COPY_AND_ASSIGN(UpdateURLLoaderRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_URL_LOADER_REQUEST_H_
