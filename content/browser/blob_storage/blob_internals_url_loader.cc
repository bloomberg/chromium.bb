// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/blob_internals_url_loader.h"

#include "content/browser/blob_storage/blob_internals_url_loader.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "storage/browser/blob/view_blob_internals_job.h"

namespace content {

void StartBlobInternalsURLLoader(
    const ResourceRequest& request,
    mojom::URLLoaderClientPtrInfo client_info,
    ChromeBlobStorageContext* blob_storage_context) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  ResourceResponseHead resource_response;
  resource_response.headers = headers;
  resource_response.mime_type = "text/html";

  mojom::URLLoaderClientPtr client;
  client.Bind(std::move(client_info));
  client->OnReceiveResponse(resource_response, base::nullopt, nullptr);

  std::string output = storage::ViewBlobInternalsJob::GenerateHTML(
      blob_storage_context->context());

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = output.size();
  mojo::DataPipe data_pipe(options);

  DCHECK(data_pipe.producer_handle.is_valid());
  DCHECK(data_pipe.consumer_handle.is_valid());

  void* buffer = nullptr;
  uint32_t num_bytes = output.size();
  MojoResult result =
      BeginWriteDataRaw(data_pipe.producer_handle.get(), &buffer, &num_bytes,
                        MOJO_WRITE_DATA_FLAG_NONE);
  CHECK_EQ(result, MOJO_RESULT_OK);
  CHECK_EQ(num_bytes, output.size());

  memcpy(buffer, output.c_str(), output.size());
  result = EndWriteDataRaw(data_pipe.producer_handle.get(), num_bytes);
  CHECK_EQ(result, MOJO_RESULT_OK);

  client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

  ResourceRequestCompletionStatus request_complete_data;
  request_complete_data.error_code = net::OK;
  request_complete_data.exists_in_cache = false;
  request_complete_data.completion_time = base::TimeTicks::Now();
  request_complete_data.encoded_data_length = output.size();
  request_complete_data.encoded_body_length = output.size();
  client->OnComplete(request_complete_data);
}

}  // namespace content
