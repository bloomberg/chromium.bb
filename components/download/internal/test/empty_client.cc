// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/test/empty_client.h"

namespace download {
namespace test {

void EmptyClient::OnServiceInitialized(
    const std::vector<std::string>& outstanding_download_guids) {}

Client::ShouldDownload EmptyClient::OnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {
  return Client::ShouldDownload::CONTINUE;
}

void EmptyClient::OnDownloadUpdated(const std::string& guid,
                                    uint64_t bytes_downloaded) {}

void EmptyClient::OnDownloadFailed(const std::string& guid,
                                   FailureReason reason) {}

void EmptyClient::OnDownloadSucceeded(const std::string& guid,
                                      const base::FilePath& path,
                                      uint64_t size) {}

}  // namespace test
}  // namespace download
