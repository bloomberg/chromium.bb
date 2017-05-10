// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/histogram_internals_url_loader.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "content/browser/histogram_internals_request_job.h"
#include "mojo/common/data_pipe_utils.h"

namespace content {

void StartHistogramInternalsURLLoader(const ResourceRequest& request,
                                      mojom::URLLoaderClientPtr client) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  ResourceResponseHead resource_response;
  resource_response.headers = headers;
  resource_response.mime_type = "text/html";
  client->OnReceiveResponse(resource_response, base::nullopt, nullptr);

  base::StatisticsRecorder::ImportProvidedHistograms();
  std::string data = HistogramInternalsRequestJob::GenerateHTML(request.url);

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = data.size();
  mojo::DataPipe data_pipe(options);

  DCHECK(data_pipe.producer_handle.is_valid());
  DCHECK(data_pipe.consumer_handle.is_valid());

  CHECK(mojo::common::BlockingCopyFromString(data, data_pipe.producer_handle));

  client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

  ResourceRequestCompletionStatus request_complete_data;
  request_complete_data.error_code = net::OK;
  request_complete_data.exists_in_cache = false;
  request_complete_data.completion_time = base::TimeTicks::Now();
  request_complete_data.encoded_data_length = data.size();
  request_complete_data.encoded_body_length = data.size();
  client->OnComplete(request_complete_data);
}

}  // namespace content
