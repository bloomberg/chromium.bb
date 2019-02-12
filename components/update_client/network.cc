// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/network.h"

#include <utility>

#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

const net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("component_updater_utils", R"(
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

NetworkFetcher::NetworkFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory,
    std::unique_ptr<network::ResourceRequest> resource_request)
    : shared_url_network_factory_(shared_url_network_factory),
      simple_url_loader_(
          network::SimpleURLLoader::Create(std::move(resource_request),
                                           traffic_annotation)) {}
NetworkFetcher::~NetworkFetcher() = default;

void NetworkFetcher::DownloadToStringOfUnboundedSizeUntilCrashAndDie(
    network::SimpleURLLoader::BodyAsStringCallback body_as_string_callback) {
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      shared_url_network_factory_.get(), std::move(body_as_string_callback));
}

void NetworkFetcher::DownloadToFile(
    network::SimpleURLLoader::DownloadToFileCompleteCallback
        download_to_file_complete_callback,
    const base::FilePath& file_path,
    int64_t max_body_size) {
  return simple_url_loader_->DownloadToFile(
      shared_url_network_factory_.get(),
      std::move(download_to_file_complete_callback), file_path, max_body_size);
}

void NetworkFetcher::AttachStringForUpload(
    const std::string& upload_data,
    const std::string& upload_content_type) {
  return simple_url_loader_->AttachStringForUpload(upload_data,
                                                   upload_content_type);
}

void NetworkFetcher::SetOnResponseStartedCallback(
    network::SimpleURLLoader::OnResponseStartedCallback
        on_response_started_callback) {
  simple_url_loader_->SetOnResponseStartedCallback(
      std::move(on_response_started_callback));
}

void NetworkFetcher::SetAllowPartialResults(bool allow_partial_results) {
  simple_url_loader_->SetAllowPartialResults(allow_partial_results);
}

void NetworkFetcher::SetRetryOptions(int max_retries, int retry_mode) {
  return simple_url_loader_->SetRetryOptions(max_retries, retry_mode);
}

int NetworkFetcher::NetError() const {
  return simple_url_loader_->NetError();
}

const network::ResourceResponseHead* NetworkFetcher::ResponseInfo() const {
  return simple_url_loader_->ResponseInfo();
}

const GURL& NetworkFetcher::GetFinalURL() const {
  return simple_url_loader_->GetFinalURL();
}

int64_t NetworkFetcher::GetContentSize() const {
  return simple_url_loader_->GetContentSize();
}

NetworkFetcherFactory::NetworkFetcherFactory(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory)
    : shared_url_network_factory_(shared_url_network_factory) {}

NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<NetworkFetcher> NetworkFetcherFactory::Create(
    std::unique_ptr<network::ResourceRequest> resource_request) const {
  return std::make_unique<NetworkFetcher>(shared_url_network_factory_,
                                          std::move(resource_request));
}

}  // namespace update_client
