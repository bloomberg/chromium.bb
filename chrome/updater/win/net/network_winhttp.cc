// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/net/network_winhttp.h"

#include "base/callback.h"
#include "chrome/updater/win/net/network.h"
#include "chrome/updater/win/net/scoped_hinternet.h"

namespace updater {

NetworkFetcherWinHTTP::NetworkFetcherWinHTTP() = default;
NetworkFetcherWinHTTP::~NetworkFetcherWinHTTP() = default;

void NetworkFetcherWinHTTP::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    PostRequestCompleteCallback post_request_complete_callback) {}

void NetworkFetcherWinHTTP::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {}

NetworkFetcherWinHTTPFactory::NetworkFetcherWinHTTPFactory()
    : session_handle_(WinHttpOpen(L"Chrome Updater",
                                  WINHTTP_ACCESS_TYPE_NO_PROXY,
                                  WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS,
                                  WINHTTP_FLAG_ASYNC)) {}
NetworkFetcherWinHTTPFactory::~NetworkFetcherWinHTTPFactory() = default;

std::unique_ptr<update_client::NetworkFetcher>
NetworkFetcherWinHTTPFactory::Create() const {
  return session_handle_.get() ? std::make_unique<NetworkFetcherWinHTTP>()
                               : nullptr;
}

}  // namespace updater
