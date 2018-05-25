// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_background_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/search/background/ntp_background.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

// The MIME type of the POST data sent to the server.
constexpr char kProtoMimeType[] = "application/x-protobuf";

// The url to download the proto of the complete list of wallpaper collections.
constexpr char kBackdropCollectionsUrl[] =
    "https://clients3.google.com/cast/chromecast/home/wallpaper/"
    "collections?rt=b";

}  // namespace

NtpBackgroundService::NtpBackgroundService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const base::Optional<GURL>& api_url_override)
    : url_loader_factory_(url_loader_factory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  api_url_ = api_url_override.value_or(GURL(kBackdropCollectionsUrl));
}

NtpBackgroundService::~NtpBackgroundService() = default;

void NtpBackgroundService::FetchCollectionInfo() {
  if (simple_loader_ != nullptr)
    return;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("backdrop_collection_names_download",
                                          R"(
        semantics {
          sender: "Desktop NTP Background Selector"
          description:
            "The Chrome Desktop New Tab Page background selector displays a "
            "rich set of wallpapers for users to choose from. Each wallpaper "
            "belongs to a collection (e.g. Arts, Landscape etc.). The list of "
            "all available collections is obtained from the Backdrop wallpaper "
            "service."
          trigger:
            "Clicking the the settings (gear) icon on the New Tab page."
          data:
            "The Backdrop protocol buffer messages. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature by selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");

  ntp::background::GetCollectionsRequest request;
  // The language field may include the country code (e.g. "en-US").
  request.set_language(g_browser_process->GetApplicationLocale());
  std::string serialized_proto;
  request.SerializeToString(&serialized_proto);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = api_url_;
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DO_NOT_SEND_AUTH_DATA;

  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  simple_loader_->AttachStringForUpload(serialized_proto, kProtoMimeType);
  simple_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&NtpBackgroundService::OnCollectionInfoFetchComplete,
                     base::Unretained(this)),
      1024 * 1024);
}

void NtpBackgroundService::OnCollectionInfoFetchComplete(
    std::unique_ptr<std::string> response_body) {
  collection_info_.clear();
  // The loader will be deleted when the request is handled.
  std::unique_ptr<network::SimpleURLLoader> loader_deleter(
      std::move(simple_loader_));

  if (!response_body) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    DLOG(WARNING) << "Request failed with error: "
                  << loader_deleter->NetError();
    NotifyObservers();
    return;
  }

  ntp::background::GetCollectionsResponse collections_response;
  if (!collections_response.ParseFromString(*response_body)) {
    DLOG(WARNING)
        << "Deserializing Backdrop wallpaper proto for collection info "
           "failed.";
    NotifyObservers();
    return;
  }

  for (int i = 0; i < collections_response.collections_size(); ++i) {
    collection_info_.push_back(
        CollectionInfo::CreateFromProto(collections_response.collections(i)));
  }

  NotifyObservers();
}

void NtpBackgroundService::AddObserver(NtpBackgroundServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void NtpBackgroundService::RemoveObserver(
    NtpBackgroundServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NtpBackgroundService::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnCollectionInfoAvailable();
  }
}

GURL NtpBackgroundService::GetLoadURLForTesting() const {
  return api_url_;
}
