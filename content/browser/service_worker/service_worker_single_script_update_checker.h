// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SINGLE_SCRIPT_UPDATE_CHECKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SINGLE_SCRIPT_UPDATE_CHECKER_H_

#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

class ServiceWorkerUpdateChecker;

class ServiceWorkerSingleScriptUpdateChecker
    : public network::mojom::URLLoaderClient {
 public:
  ServiceWorkerSingleScriptUpdateChecker(const GURL script_url,
                                         int64_t resource_id,
                                         ServiceWorkerUpdateChecker* owner);
  ~ServiceWorkerSingleScriptUpdateChecker() override;

  void Start();

  // network::mojom::URLLoaderClient override:
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle consumer) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

 private:
  void CommitCompleted(bool is_script_changed);

  ServiceWorkerUpdateChecker* owner_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SINGLE_SCRIPT_UPDATE_CHECKER_H_
