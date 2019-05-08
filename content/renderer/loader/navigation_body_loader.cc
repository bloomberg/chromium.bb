// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/navigation_body_loader.h"

#include "base/bind.h"
#include "base/macros.h"
#include "content/renderer/loader/code_cache_loader_impl.h"
#include "content/renderer/loader/resource_load_stats.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "third_party/blink/public/web/web_navigation_params.h"

namespace content {

// static
constexpr uint32_t NavigationBodyLoader::kMaxNumConsumedBytesInTask;

// static
void NavigationBodyLoader::FillNavigationParamsResponseAndBodyLoader(
    const CommonNavigationParams& common_params,
    const CommitNavigationParams& commit_params,
    int request_id,
    const network::ResourceResponseHead& head,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    int render_frame_id,
    bool is_main_frame,
    blink::WebNavigationParams* navigation_params) {
  // Use the original navigation url to start with. We'll replay the redirects
  // afterwards and will eventually arrive to the final url.
  GURL url = !commit_params.original_url.is_empty() ? commit_params.original_url
                                                    : common_params.url;
  auto resource_load_info = NotifyResourceLoadInitiated(
      render_frame_id, request_id, url,
      !commit_params.original_method.empty() ? commit_params.original_method
                                             : common_params.method,
      common_params.referrer.url,
      is_main_frame ? ResourceType::kMainFrame : ResourceType::kSubFrame);

  size_t redirect_count = commit_params.redirect_response.size();
  navigation_params->redirects.reserve(redirect_count);
  navigation_params->redirects.resize(redirect_count);
  for (size_t i = 0; i < redirect_count; ++i) {
    blink::WebNavigationParams::RedirectInfo& redirect =
        navigation_params->redirects[i];
    auto& redirect_info = commit_params.redirect_infos[i];
    auto& redirect_response = commit_params.redirect_response[i];
    NotifyResourceRedirectReceived(render_frame_id, resource_load_info.get(),
                                   redirect_info, redirect_response);
    WebURLLoaderImpl::PopulateURLResponse(
        url, redirect_response, &redirect.redirect_response,
        false /* report_security_info */, request_id);
    if (url.SchemeIs(url::kDataScheme))
      redirect.redirect_response.SetHttpStatusCode(200);
    redirect.new_url = redirect_info.new_url;
    redirect.new_referrer =
        blink::WebString::FromUTF8(redirect_info.new_referrer);
    redirect.new_referrer_policy =
        Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
            redirect_info.new_referrer_policy);
    redirect.new_http_method =
        blink::WebString::FromLatin1(redirect_info.new_method);
    url = redirect_info.new_url;
  }

  WebURLLoaderImpl::PopulateURLResponse(url, head, &navigation_params->response,
                                        false /* report_security_info */,
                                        request_id);
  if (url.SchemeIs(url::kDataScheme))
    navigation_params->response.SetHttpStatusCode(200);

  if (url_loader_client_endpoints) {
    navigation_params->body_loader.reset(new NavigationBodyLoader(
        head, std::move(url_loader_client_endpoints), task_runner,
        render_frame_id, std::move(resource_load_info)));
  }
}

NavigationBodyLoader::NavigationBodyLoader(
    const network::ResourceResponseHead& head,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    int render_frame_id,
    mojom::ResourceLoadInfoPtr resource_load_info)
    : render_frame_id_(render_frame_id),
      head_(head),
      endpoints_(std::move(endpoints)),
      task_runner_(std::move(task_runner)),
      resource_load_info_(std::move(resource_load_info)),
      url_loader_client_binding_(this),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      task_runner_),
      weak_factory_(this) {}

NavigationBodyLoader::~NavigationBodyLoader() {
  if (!has_received_completion_ || !has_seen_end_of_data_) {
    NotifyResourceLoadCanceled(render_frame_id_, std::move(resource_load_info_),
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

void NavigationBodyLoader::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  // Even if IsolatedCodeCaching is landed, this code is still used by
  // ServiceWorker.
  // TODO(horo, kinuko): Make a test to cover this function.
  // TODO(https://crbug.com/930000): Add support for inline script code caching
  // with the service worker service.
  client_->BodyCodeCacheReceived(std::move(data));
}

void NavigationBodyLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  NotifyResourceTransferSizeUpdated(render_frame_id_, resource_load_info_.get(),
                                    transfer_size_diff);
}

void NavigationBodyLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle handle) {
  TRACE_EVENT1("loading", "NavigationBodyLoader::OnStartLoadingResponseBody",
               "url", resource_load_info_->url.possibly_invalid_spec());
  DCHECK(!has_received_body_handle_);
  DCHECK(!has_received_completion_);
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
  if (handle_.is_valid())
    OnReadable(MOJO_RESULT_OK);
}

void NavigationBodyLoader::StartLoadingBody(
    WebNavigationBodyLoader::Client* client,
    bool use_isolated_code_cache) {
  TRACE_EVENT1("loading", "NavigationBodyLoader::StartLoadingBody", "url",
               resource_load_info_->url.possibly_invalid_spec());
  client_ = client;

  NotifyResourceResponseReceived(render_frame_id_, resource_load_info_.get(),
                                 head_, content::PREVIEWS_OFF);

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

void NavigationBodyLoader::CodeCacheReceived(base::Time response_time,
                                             base::span<const uint8_t> data) {
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
  TRACE_EVENT1("loading", "NavigationBodyLoader::OnReadable", "url",
               resource_load_info_->url.possibly_invalid_spec());
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
  TRACE_EVENT1("loading", "NavigationBodyLoader::ReadFromDataPipe", "url",
               resource_load_info_->url.possibly_invalid_spec());
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
    DCHECK_LE(num_bytes_consumed, kMaxNumConsumedBytesInTask);
    available =
        std::min(available, kMaxNumConsumedBytesInTask - num_bytes_consumed);
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

  base::Optional<blink::WebURLError> error;
  if (status_.error_code != net::OK) {
    error =
        WebURLLoaderImpl::PopulateURLError(status_, resource_load_info_->url);
  }

  NotifyResourceLoadCompleted(render_frame_id_, std::move(resource_load_info_),
                              status_);

  if (!client_)
    return;

  // |this| may be deleted after calling into client_, so clear it in advance.
  WebNavigationBodyLoader::Client* client = client_;
  client_ = nullptr;
  client->BodyLoadingFinished(
      status_.completion_time, status_.encoded_data_length,
      status_.encoded_body_length, status_.decoded_body_length,
      status_.should_report_corb_blocking, error);
}

}  // namespace content
