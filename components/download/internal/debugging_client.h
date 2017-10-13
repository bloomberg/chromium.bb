// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "components/download/public/client.h"

namespace download {

// An empty Client implementation that is meant to be used by debugging layers
// like the WebUI.
class DebuggingClient : public Client {
 public:
  DebuggingClient() = default;
  ~DebuggingClient() override = default;

 private:
  // Client implementation.
  void OnServiceInitialized(
      bool state_lost,
      const std::vector<DownloadMetaData>& downloads) override;
  void OnServiceUnavailable() override;
  ShouldDownload OnDownloadStarted(
      const std::string& guid,
      const std::vector<GURL>& url_chain,
      const scoped_refptr<const net::HttpResponseHeaders>& headers) override;
  void OnDownloadUpdated(const std::string& guid,
                         uint64_t bytes_downloaded) override;
  void OnDownloadFailed(const std::string& guid, FailureReason reason) override;
  void OnDownloadSucceeded(const std::string& guid,
                           const CompletionInfo& completion_info) override;
  bool CanServiceRemoveDownloadedFile(const std::string& guid,
                                      bool force_delete) override;

  DISALLOW_COPY_AND_ASSIGN(DebuggingClient);
};

}  // namespace download
