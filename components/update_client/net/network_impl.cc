// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/net/network_impl.h"

#include <utility>

#include "base/bind.h"
#include "components/update_client/net/network_chromium.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

const net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("update_client", R"(
        semantics {
          sender: "Component Updater"
          description:
            "The component updater in Chrome is responsible for updating code "
            "and data modules such as Flash, CrlSet, Origin Trials, etc. These "
            "modules are updated on cycles independent of the Chrome release "
            "tracks. It runs in the browser process and communicates with a "
            "set of servers using the Omaha protocol to find the latest "
            "versions of components, download them, and register them with the "
            "rest of Chrome."
          trigger: "Manual or automatic software updates."
          data:
            "Various OS and Chrome parameters such as version, bitness, "
            "release tracks, etc."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled."
          chrome_policy {
            ComponentUpdatesEnabled {
              policy_options {mode: MANDATORY}
              ComponentUpdatesEnabled: false
            }
          }
        })");

}  // namespace

namespace update_client {

NetworkFetcherImpl::NetworkFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory)
    : shared_url_network_factory_(shared_url_network_factory) {}
NetworkFetcherImpl::~NetworkFetcherImpl() = default;

void NetworkFetcherImpl::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  DCHECK(!simple_url_loader_);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DO_NOT_SEND_COOKIES |
                                 net::LOAD_DO_NOT_SAVE_COOKIES |
                                 net::LOAD_DISABLE_CACHE;
  for (const auto& header : post_additional_headers)
    resource_request->headers.SetHeader(header.first, header.second);
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->SetRetryOptions(
      kMaxRetriesOnNetworkChange,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->AttachStringForUpload(post_data, "application/xml");
  simple_url_loader_->SetOnResponseStartedCallback(base::BindOnce(
      &NetworkFetcherImpl::OnResponseStartedCallback, base::Unretained(this),
      std::move(response_started_callback)));
  constexpr size_t kMaxResponseSize = 1024 * 1024;
  simple_url_loader_->DownloadToString(
      shared_url_network_factory_.get(),
      std::move(post_request_complete_callback), kMaxResponseSize);
}

void NetworkFetcherImpl::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  DCHECK(!simple_url_loader_);
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_DO_NOT_SEND_COOKIES |
                                 net::LOAD_DO_NOT_SAVE_COOKIES |
                                 net::LOAD_DISABLE_CACHE;
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->SetRetryOptions(
      kMaxRetriesOnNetworkChange,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->SetAllowPartialResults(true);
  simple_url_loader_->SetOnResponseStartedCallback(base::BindOnce(
      &NetworkFetcherImpl::OnResponseStartedCallback, base::Unretained(this),
      std::move(response_started_callback)));
  simple_url_loader_->DownloadToFile(
      shared_url_network_factory_.get(),
      std::move(download_to_file_complete_callback), file_path);
}

int64_t NetworkFetcherImpl::GetContentSize() const {
  DCHECK(simple_url_loader_);
  return simple_url_loader_->GetContentSize();
}

int NetworkFetcherImpl::NetError() const {
  DCHECK(simple_url_loader_);
  return simple_url_loader_->NetError();
}

std::string NetworkFetcherImpl::GetStringHeaderValue(
    const char* header_name) const {
  DCHECK(simple_url_loader_);

  const auto* response_info = simple_url_loader_->ResponseInfo();
  if (!response_info || !response_info->headers)
    return {};

  std::string header_value;
  return response_info->headers->EnumerateHeader(nullptr, header_name,
                                                 &header_value)
             ? header_value
             : std::string{};
}

int64_t NetworkFetcherImpl::GetInt64HeaderValue(const char* header_name) const {
  DCHECK(simple_url_loader_);

  const auto* response_info = simple_url_loader_->ResponseInfo();
  if (!response_info || !response_info->headers)
    return -1;

  return response_info->headers->GetInt64HeaderValue(header_name);
}

void NetworkFetcherImpl::OnResponseStartedCallback(
    ResponseStartedCallback response_started_callback,
    const GURL& final_url,
    const network::ResourceResponseHead& response_head) {
  std::move(response_started_callback)
      .Run(final_url,
           response_head.headers ? response_head.headers->response_code() : -1,
           response_head.content_length);
}

NetworkFetcherChromiumFactory::NetworkFetcherChromiumFactory(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory)
    : shared_url_network_factory_(shared_url_network_factory) {}

NetworkFetcherChromiumFactory::~NetworkFetcherChromiumFactory() = default;

std::unique_ptr<NetworkFetcher> NetworkFetcherChromiumFactory::Create() const {
  return std::make_unique<NetworkFetcherImpl>(shared_url_network_factory_);
}

}  // namespace update_client
