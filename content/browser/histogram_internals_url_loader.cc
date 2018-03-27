// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/histogram_internals_url_loader.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "content/browser/histogram_internals_request_job.h"
#include "mojo/common/data_pipe_utils.h"

namespace content {

void StartHistogramInternalsURLLoader(
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  network::ResourceResponseHead resource_response;
  resource_response.headers = headers;
  resource_response.mime_type = "text/html";
  client->OnReceiveResponse(resource_response, nullptr);

  base::StatisticsRecorder::ImportProvidedHistograms();
  std::string data = HistogramInternalsRequestJob::GenerateHTML(request.url);
  mojo::DataPipe data_pipe(data.size());
  CHECK(mojo::common::BlockingCopyFromString(data, data_pipe.producer_handle));

  client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));
  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = data.size();
  status.encoded_body_length = data.size();
  client->OnComplete(status);
}

}  // namespace content
