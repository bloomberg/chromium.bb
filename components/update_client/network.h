// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_NETWORK_H_
#define COMPONENTS_UPDATE_CLIENT_NETWORK_H_

#include <stdint.h>

#include <limits>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "services/network/public/cpp/simple_url_loader.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace update_client {

class NetworkFetcher {
 public:
  NetworkFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory,
      std::unique_ptr<network::ResourceRequest> resource_request);
  ~NetworkFetcher();

  // These functions are delegating to the corresponding members of
  // network::SimpleURLLoader.
  void DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      network::SimpleURLLoader::BodyAsStringCallback body_as_string_callback);
  void DownloadToFile(
      network::SimpleURLLoader::DownloadToFileCompleteCallback
          download_to_file_complete_callback,
      const base::FilePath& file_path,
      int64_t max_body_size = std::numeric_limits<int64_t>::max());
  void AttachStringForUpload(const std::string& upload_data,
                             const std::string& upload_content_type);
  void SetOnResponseStartedCallback(
      network::SimpleURLLoader::OnResponseStartedCallback
          on_response_started_callback);
  void SetAllowPartialResults(bool allow_partial_results);
  void SetRetryOptions(int max_retries, int retry_mode);
  int NetError() const;
  const network::ResourceResponseHead* ResponseInfo() const;
  const GURL& GetFinalURL() const;
  int64_t GetContentSize() const;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  DISALLOW_COPY_AND_ASSIGN(NetworkFetcher);
};

class NetworkFetcherFactory : public base::RefCounted<NetworkFetcherFactory> {
 public:
  explicit NetworkFetcherFactory(scoped_refptr<network::SharedURLLoaderFactory>
                                     shared_url_network_factory);

  std::unique_ptr<NetworkFetcher> Create(
      std::unique_ptr<network::ResourceRequest> resource_request) const;

 protected:
  friend class base::RefCounted<NetworkFetcherFactory>;
  virtual ~NetworkFetcherFactory();

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkFetcherFactory);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_NETWORK_H_
