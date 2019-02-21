// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_NET_NETWORK_WINHTTP_H_
#define CHROME_UPDATER_WIN_NET_NETWORK_WINHTTP_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/network.h"

namespace updater {

class NetworkFetcherWinHTTP : public update_client::NetworkFetcher {
 public:
  using ResponseStartedCallback =
      update_client::NetworkFetcher::ResponseStartedCallback;
  using PostRequestCompleteCallback =
      update_client::NetworkFetcher::PostRequestCompleteCallback;
  using DownloadToFileCompleteCallback =
      update_client::NetworkFetcher::DownloadToFileCompleteCallback;

  NetworkFetcherWinHTTP();
  ~NetworkFetcherWinHTTP() override;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;
  void DownloadToFile(const GURL& url,
                      const base::FilePath& file_path,
                      ResponseStartedCallback response_started_callback,
                      DownloadToFileCompleteCallback
                          download_to_file_complete_callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkFetcherWinHTTP);
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_NET_NETWORK_WINHTTP_H_
