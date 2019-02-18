// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_NET_NETWORK_IMPL_H_
#define COMPONENTS_UPDATE_CLIENT_NET_NETWORK_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/network.h"

namespace network {
struct ResourceResponseHead;
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace update_client {

class NetworkFetcherImpl : public NetworkFetcher {
 public:
  explicit NetworkFetcherImpl(scoped_refptr<network::SharedURLLoaderFactory>
                                  shared_url_network_factory);
  ~NetworkFetcherImpl() override;

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
  int NetError() const override;
  std::string GetStringHeaderValue(const char* header_name) const override;
  int64_t GetInt64HeaderValue(const char* header_name) const override;
  int64_t GetContentSize() const override;

 private:
  void OnResponseStartedCallback(
      ResponseStartedCallback response_started_callback,
      const GURL& final_url,
      const network::ResourceResponseHead& response_head);

  static constexpr int kMaxRetriesOnNetworkChange = 3;

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  DISALLOW_COPY_AND_ASSIGN(NetworkFetcherImpl);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_NET_NETWORK_IMPL_H_
