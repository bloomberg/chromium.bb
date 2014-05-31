// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/net_metrics_log_uploader.h"

#include "base/metrics/histogram.h"
#include "components/metrics/net/compression_utils.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

namespace metrics {

NetMetricsLogUploader::NetMetricsLogUploader(
    net::URLRequestContextGetter* request_context_getter,
    const std::string& server_url,
    const std::string& mime_type,
    const base::Callback<void(int)>& on_upload_complete)
    : MetricsLogUploader(server_url, mime_type, on_upload_complete),
      request_context_getter_(request_context_getter) {
}

NetMetricsLogUploader::~NetMetricsLogUploader() {
}

bool NetMetricsLogUploader::UploadLog(const std::string& log_data,
                                      const std::string& log_hash) {
  std::string compressed_log_data;
  if (!GzipCompress(log_data, &compressed_log_data)) {
    NOTREACHED();
    return false;
  }

  UMA_HISTOGRAM_PERCENTAGE(
      "UMA.ProtoCompressionRatio",
      static_cast<int>(100 * compressed_log_data.size() / log_data.size()));
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "UMA.ProtoGzippedKBSaved",
      static_cast<int>((log_data.size() - compressed_log_data.size()) / 1024),
      1, 2000, 50);

  current_fetch_.reset(
      net::URLFetcher::Create(GURL(server_url_), net::URLFetcher::POST, this));
  current_fetch_->SetRequestContext(request_context_getter_);
  current_fetch_->SetUploadData(mime_type_, compressed_log_data);

  // Tell the server that we're uploading gzipped protobufs.
  current_fetch_->SetExtraRequestHeaders("content-encoding: gzip");

  DCHECK(!log_hash.empty());
  current_fetch_->AddExtraRequestHeader("X-Chrome-UMA-Log-SHA1: " + log_hash);

  // We already drop cookies server-side, but we might as well strip them out
  // client-side as well.
  current_fetch_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                               net::LOAD_DO_NOT_SEND_COOKIES);
  return true;
}

void NetMetricsLogUploader::OnURLFetchComplete(const net::URLFetcher* source) {
  // We're not allowed to re-use the existing |URLFetcher|s, so free them here.
  // Note however that |source| is aliased to the fetcher, so we should be
  // careful not to delete it too early.
  DCHECK_EQ(current_fetch_.get(), source);

  int response_code = source->GetResponseCode();
  if (response_code == net::URLFetcher::RESPONSE_CODE_INVALID)
    response_code = -1;
  on_upload_complete_.Run(response_code);
  current_fetch_.reset();
}

}  // namespace metrics
