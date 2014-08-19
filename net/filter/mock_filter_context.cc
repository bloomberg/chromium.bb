// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/mock_filter_context.h"

#include "net/url_request/url_request_context.h"

namespace net {

MockFilterContext::MockFilterContext()
    : is_cached_content_(false),
      is_download_(false),
      is_sdch_response_(false),
      ok_to_call_get_url_(true),
      response_code_(-1),
      context_(new URLRequestContext()) {
}

MockFilterContext::~MockFilterContext() {}

void MockFilterContext::NukeUnstableInterfaces() {
  context_.reset();
  ok_to_call_get_url_ = false;
  request_time_ = base::Time();
}

bool MockFilterContext::GetMimeType(std::string* mime_type) const {
  *mime_type = mime_type_;
  return true;
}

// What URL was used to access this data?
// Return false if gurl is not present.
bool MockFilterContext::GetURL(GURL* gurl) const {
  DCHECK(ok_to_call_get_url_);
  *gurl = gurl_;
  return true;
}

bool MockFilterContext::GetContentDisposition(std::string* disposition) const {
  if (content_disposition_.empty())
    return false;
  *disposition = content_disposition_;
  return true;
}

// What was this data requested from a server?
base::Time MockFilterContext::GetRequestTime() const {
  return request_time_;
}

bool MockFilterContext::IsCachedContent() const { return is_cached_content_; }

bool MockFilterContext::IsDownload() const { return is_download_; }

bool MockFilterContext::IsSdchResponse() const { return is_sdch_response_; }

int64 MockFilterContext::GetByteReadCount() const { return 0; }

int MockFilterContext::GetResponseCode() const { return response_code_; }

const URLRequestContext* MockFilterContext::GetURLRequestContext() const {
  return context_.get();
}

}  // namespace net
