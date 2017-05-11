// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/cache_url_loader.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/url_constants.h"
#include "mojo/common/data_pipe_utils.h"
#include "net/url_request/view_cache_helper.h"

namespace content {

namespace {

class CacheURLLoader {
 public:
  CacheURLLoader(const ResourceRequest& request,
                 net::URLRequestContext* request_context,
                 mojom::URLLoaderClientPtr client)
      : client_(std::move(client)) {
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    ResourceResponseHead resource_response;
    resource_response.headers = headers;
    resource_response.mime_type = "text/html";
    client_->OnReceiveResponse(resource_response, base::nullopt, nullptr);

    std::string cache_key =
        request.url.spec().substr(strlen(kChromeUINetworkViewCacheURL));

    int rv;
    if (cache_key.empty()) {
      rv = cache_helper_.GetContentsHTML(
          request_context, kChromeUINetworkViewCacheURL, &data_,
          base::Bind(&CacheURLLoader::DataAvailable, base::Unretained(this)));
    } else {
      rv = cache_helper_.GetEntryInfoHTML(
          cache_key, request_context, &data_,
          base::Bind(&CacheURLLoader::DataAvailable, base::Unretained(this)));
    }

    if (rv != net::ERR_IO_PENDING)
      DataAvailable(rv);
  }

  ~CacheURLLoader() {}

 private:
  void DataAvailable(int result) {
    DCHECK_EQ(net::OK, result);
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = data_.size();
    mojo::DataPipe data_pipe(options);

    DCHECK(data_pipe.producer_handle.is_valid());
    DCHECK(data_pipe.consumer_handle.is_valid());

    CHECK(
        mojo::common::BlockingCopyFromString(data_, data_pipe.producer_handle));

    client_->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

    ResourceRequestCompletionStatus request_complete_data;
    request_complete_data.error_code = net::OK;
    request_complete_data.exists_in_cache = false;
    request_complete_data.completion_time = base::TimeTicks::Now();
    request_complete_data.encoded_data_length = data_.size();
    request_complete_data.encoded_body_length = data_.size();
    client_->OnComplete(request_complete_data);

    // So we don't delete |this| in the constructor.
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

  std::string data_;
  mojom::URLLoaderClientPtr client_;
  net::ViewCacheHelper cache_helper_;

  DISALLOW_COPY_AND_ASSIGN(CacheURLLoader);
};
}

void StartCacheURLLoader(const ResourceRequest& request,
                         net::URLRequestContext* request_context,
                         mojom::URLLoaderClientPtr client) {
  new CacheURLLoader(request, request_context, std::move(client));
}

}  // namespace content
