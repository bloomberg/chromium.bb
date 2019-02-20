// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_NETWORK_H_
#define COMPONENTS_UPDATE_CLIENT_NETWORK_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace update_client {

class NetworkFetcher {
 public:
  using PostRequestCompleteCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body)>;
  using DownloadToFileCompleteCallback =
      base::OnceCallback<void(base::FilePath path)>;
  using ResponseStartedCallback = base::OnceCallback<
      void(const GURL& final_url, int response_code, int64_t content_length)>;

  virtual ~NetworkFetcher() = default;

  virtual void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      PostRequestCompleteCallback post_request_complete_callback) = 0;
  virtual void DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback) = 0;
  virtual int NetError() const = 0;

  // Returns the string value of a header of the server response or an empty
  // string if the header is not available. Only the first header is returned
  // if multiple instances of the same header are present.
  virtual std::string GetStringHeaderValue(const char* header_name) const = 0;

  // Returns the integral value of a header of the server response or -1 if
  // if the header is not available or a conversion error has occured.
  virtual int64_t GetInt64HeaderValue(const char* header_name) const = 0;
  virtual int64_t GetContentSize() const = 0;

 protected:
  NetworkFetcher() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkFetcher);
};

class NetworkFetcherFactory : public base::RefCounted<NetworkFetcherFactory> {
 public:
  virtual std::unique_ptr<NetworkFetcher> Create() const = 0;

 protected:
  friend class base::RefCounted<NetworkFetcherFactory>;
  NetworkFetcherFactory() = default;
  virtual ~NetworkFetcherFactory() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkFetcherFactory);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_NETWORK_H_
