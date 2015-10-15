// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/url_request_cloneable.h"

#include "base/logging.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/common/url_type_converters.h"

namespace web_view {

// TODO(erg): In the long run, we might not want to have a stack of
// URLRequestPtrs, but another type that captures most of the data. When I saw
// NavigationController the first time, I didn't understand why they made their
// own datastructure which kept track of everything in a request. The reason is
// that they have to build requests from multiple different datatypes.

URLRequestCloneable::URLRequestCloneable(mojo::URLRequestPtr original_request)
    : url_(original_request->url),
      method_(original_request->method),
      headers_(original_request->headers.Pass()),
      response_body_buffer_size_(original_request->response_body_buffer_size),
      auto_follow_redirects_(original_request->auto_follow_redirects),
      bypass_cache_(original_request->bypass_cache),
      original_body_null_(original_request->body.is_null()),
      body_(original_request->body.size()),
      originating_time_(base::TimeTicks::FromInternalValue(
          original_request->originating_time_ticks)) {
  // TODO(erg): Maybe we can do some sort of async copy here?
  for (size_t i = 0; i < original_request->body.size(); ++i) {
    mojo::common::BlockingCopyToString(original_request->body[i].Pass(),
                                       &body_[i]);
  }
}

URLRequestCloneable::URLRequestCloneable(const GURL& raw_url)
    : url_(mojo::String::From(raw_url)),
      method_("GET"),
      headers_(),
      response_body_buffer_size_(0),
      auto_follow_redirects_(false),
      bypass_cache_(false),
      original_body_null_(true) {
}

URLRequestCloneable::~URLRequestCloneable() {}

mojo::URLRequestPtr URLRequestCloneable::Clone() const {
  mojo::URLRequestPtr request = mojo::URLRequest::New();
  request->url = url_;
  request->method = method_;
  request->headers = headers_.Clone();
  request->response_body_buffer_size = response_body_buffer_size_;
  request->auto_follow_redirects = auto_follow_redirects_;
  request->bypass_cache = bypass_cache_;

  if (!original_body_null_) {
    request->body =
        mojo::Array<mojo::ScopedDataPipeConsumerHandle>(body_.size());
    for (size_t i = 0; i < body_.size(); ++i) {
      uint32_t num_bytes = static_cast<uint32_t>(body_[i].size());
      MojoCreateDataPipeOptions options;
      options.struct_size = sizeof(MojoCreateDataPipeOptions);
      options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
      options.element_num_bytes = 1;
      options.capacity_num_bytes = num_bytes;
      mojo::DataPipe data_pipe(options);
      request->body[i] = data_pipe.consumer_handle.Pass();
      WriteDataRaw(data_pipe.producer_handle.get(), body_[i].data(), &num_bytes,
                   MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
      DCHECK_EQ(num_bytes, body_[i].size());
    }
  }

  request->originating_time_ticks = originating_time_.ToInternalValue();

  return request.Pass();
}

}  // namespace web_view
