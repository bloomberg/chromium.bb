// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/navigation_body_loader.h"

#include "base/bind.h"
#include "base/macros.h"
#include "content/renderer/loader/code_cache_loader_impl.h"
#include "content/renderer/loader/resource_load_stats.h"
#include "content/renderer/loader/url_response_body_consumer.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace content {

NavigationBodyLoader::NavigationBodyLoader(
    mojom::ResourceLoadInfoPtr resource_load_info,
    const network::ResourceResponseHead& head,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    int render_frame_id)
    : resource_load_info_(std::move(resource_load_info)),
      head_(head),
      endpoints_(std::move(endpoints)),
      task_runner_(task_runner),
      render_frame_id_(render_frame_id),
      url_loader_client_binding_(this),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      task_runner),
      weak_factory_(this) {}

NavigationBodyLoader::~NavigationBodyLoader() {
  if (!has_received_completion_ || !has_seen_end_of_data_) {
    NotifyResourceLoadCanceled(
        render_frame_id_, resource_load_info_->request_id,
        resource_load_info_->url, resource_load_info_->resource_type,
        net::ERR_ABORTED);
  }
}

void NavigationBodyLoader::OnReceiveResponse(
    const network::ResourceResponseHead& head) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void NavigationBodyLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void NavigationBodyLoader::OnUploadProgress(int64_t current_position,
                                            int64_t total_size,
                                            OnUploadProgressCallback callback) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void NavigationBodyLoader::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  client_->BodyCodeCacheReceived(data);
}

void NavigationBodyLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  NotifyResourceTransferSizeUpdated(
      render_frame_id_, resource_load_info_->request_id, transfer_size_diff);
}

void NavigationBodyLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle handle) {
  DCHECK(!has_received_body_handle_);
  has_received_body_handle_ = true;
  has_seen_end_of_data_ = false;
  handle_ = std::move(handle);
  DCHECK(handle_.is_valid());
  handle_watcher_.Watch(handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                        base::BindRepeating(&NavigationBodyLoader::OnReadable,
                                            base::Unretained(this)));
  OnReadable(MOJO_RESULT_OK);
}

void NavigationBodyLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // Except for errors, there must always be a response's body.
  DCHECK(has_received_body_handle_ || status.error_code != net::OK);
  has_received_completion_ = true;
  status_ = status;
  NotifyCompletionIfAppropriate();
}

void NavigationBodyLoader::SetDefersLoading(bool defers) {
  if (is_deferred_ == defers)
    return;
  is_deferred_ = defers;
  OnReadable(MOJO_RESULT_OK);
}

void NavigationBodyLoader::StartLoadingBody(
    WebNavigationBodyLoader::Client* client,
    bool use_isolated_code_cache) {
  client_ = client;

  NotifyResourceLoadStarted(render_frame_id_, resource_load_info_->request_id,
                            resource_load_info_->url, head_,
                            resource_load_info_->resource_type);

  if (use_isolated_code_cache) {
    code_cache_loader_ = std::make_unique<CodeCacheLoaderImpl>();
    code_cache_loader_->FetchFromCodeCache(
        blink::mojom::CodeCacheType::kJavascript, resource_load_info_->url,
        base::BindOnce(&NavigationBodyLoader::CodeCacheReceived,
                       weak_factory_.GetWeakPtr()));
  } else {
    BindURLLoaderAndContinue();
  }
}

void NavigationBodyLoader::CodeCacheReceived(const base::Time& response_time,
                                             const std::vector<uint8_t>& data) {
  if (head_.response_time == response_time && client_) {
    base::WeakPtr<NavigationBodyLoader> weak_self = weak_factory_.GetWeakPtr();
    client_->BodyCodeCacheReceived(data);
    if (!weak_self)
      return;
  }
  code_cache_loader_.reset();
  // TODO(dgozman): we should explore retrieveing code cache in parallel with
  // receiving response or reading the first data chunk.
  BindURLLoaderAndContinue();
}

void NavigationBodyLoader::BindURLLoaderAndContinue() {
  url_loader_.Bind(std::move(endpoints_->url_loader), task_runner_);
  url_loader_client_binding_.Bind(std::move(endpoints_->url_loader_client),
                                  task_runner_);
  url_loader_client_binding_.set_connection_error_handler(base::BindOnce(
      &NavigationBodyLoader::OnConnectionClosed, base::Unretained(this)));
}

void NavigationBodyLoader::OnConnectionClosed() {
  // If the connection aborts before the load completes, mark it as failed.
  if (!has_received_completion_)
    OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
}

void NavigationBodyLoader::OnReadable(MojoResult unused) {
  if (has_seen_end_of_data_ || is_deferred_ || is_in_on_readable_)
    return;
  // Protect against reentrancy:
  // - when the client calls SetDefersLoading;
  // - when nested message loop starts from BodyDataReceived
  //   and we get notified by the watcher.
  // Note: we cannot use AutoReset here since |this| may be deleted
  // before reset.
  is_in_on_readable_ = true;
  base::WeakPtr<NavigationBodyLoader> weak_self = weak_factory_.GetWeakPtr();
  ReadFromDataPipe();
  if (!weak_self)
    return;
  is_in_on_readable_ = false;
}

void NavigationBodyLoader::ReadFromDataPipe() {
  uint32_t num_bytes_consumed = 0;
  while (!is_deferred_) {
    const void* buffer = nullptr;
    uint32_t available = 0;
    MojoResult result =
        handle_->BeginReadData(&buffer, &available, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher_.ArmOrNotify();
      return;
    }
    if (result == MOJO_RESULT_FAILED_PRECONDITION) {
      has_seen_end_of_data_ = true;
      NotifyCompletionIfAppropriate();
      return;
    }
    if (result != MOJO_RESULT_OK) {
      status_.error_code = net::ERR_FAILED;
      has_seen_end_of_data_ = true;
      has_received_completion_ = true;
      NotifyCompletionIfAppropriate();
      return;
    }
    DCHECK_LE(num_bytes_consumed,
              URLResponseBodyConsumer::kMaxNumConsumedBytesInTask);
    available = std::min(available,
                         URLResponseBodyConsumer::kMaxNumConsumedBytesInTask -
                             num_bytes_consumed);
    if (available == 0) {
      // We've already consumed many bytes in this task. Defer the remaining
      // to the next task.
      result = handle_->EndReadData(0);
      DCHECK_EQ(result, MOJO_RESULT_OK);
      handle_watcher_.ArmOrNotify();
      return;
    }
    num_bytes_consumed += available;
    base::WeakPtr<NavigationBodyLoader> weak_self = weak_factory_.GetWeakPtr();
    client_->BodyDataReceived(
        base::make_span(static_cast<const char*>(buffer), available));
    if (!weak_self)
      return;
    result = handle_->EndReadData(available);
    DCHECK_EQ(MOJO_RESULT_OK, result);
  }
}

void NavigationBodyLoader::NotifyCompletionIfAppropriate() {
  if (!has_received_completion_ || !has_seen_end_of_data_)
    return;

  handle_watcher_.Cancel();

  GURL url = resource_load_info_->url;
  resource_load_info_->was_cached = status_.exists_in_cache;
  resource_load_info_->net_error = status_.error_code;
  resource_load_info_->total_received_bytes = status_.encoded_data_length;
  resource_load_info_->raw_body_bytes = status_.encoded_body_length;
  NotifyResourceLoadCompleted(render_frame_id_, std::move(resource_load_info_),
                              status_);

  if (!client_)
    return;

  // |this| may be deleted after calling into client_, so clear it in advance.
  WebNavigationBodyLoader::Client* client = client_;
  client_ = nullptr;
  base::Optional<blink::WebURLError> error;
  if (status_.error_code != net::OK)
    error = WebURLLoaderImpl::PopulateURLError(status_, url);
  client->BodyLoadingFinished(
      status_.completion_time, status_.encoded_data_length,
      status_.encoded_body_length, status_.decoded_body_length,
      status_.should_report_corb_blocking, error);
}

}  // namespace content
