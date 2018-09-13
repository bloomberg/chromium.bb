// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_single_script_update_checker.h"
#include "content/browser/service_worker/service_worker_update_checker.h"

namespace content {

ServiceWorkerSingleScriptUpdateChecker::ServiceWorkerSingleScriptUpdateChecker(
    const GURL script_url,
    int64_t resource_id,
    ServiceWorkerUpdateChecker* owner)
    : owner_(owner) {
  NOTIMPLEMENTED();
}

ServiceWorkerSingleScriptUpdateChecker::
    ~ServiceWorkerSingleScriptUpdateChecker() = default;

void ServiceWorkerSingleScriptUpdateChecker::Start() {
  CommitCompleted(true /* is_script_changed */);
}

// URLLoaderClient override ----------------------------------------------------
void ServiceWorkerSingleScriptUpdateChecker::OnReceiveResponse(
    const network::ResourceResponseHead& response_head) {
  NOTIMPLEMENTED();
}

void ServiceWorkerSingleScriptUpdateChecker::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  NOTIMPLEMENTED();
}

void ServiceWorkerSingleScriptUpdateChecker::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  NOTIMPLEMENTED();
}

void ServiceWorkerSingleScriptUpdateChecker::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  NOTIMPLEMENTED();
}

void ServiceWorkerSingleScriptUpdateChecker::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  NOTIMPLEMENTED();
}

void ServiceWorkerSingleScriptUpdateChecker::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle consumer) {
  NOTIMPLEMENTED();
}

void ServiceWorkerSingleScriptUpdateChecker::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  NOTIMPLEMENTED();
}
//------------------------------------------------------------------------------

void ServiceWorkerSingleScriptUpdateChecker::CommitCompleted(
    bool is_script_changed) {
  owner_->OnOneUpdateCheckFinished(is_script_changed);
}

}  // namespace content
