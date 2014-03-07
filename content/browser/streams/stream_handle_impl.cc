// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/streams/stream_handle_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/browser/streams/stream.h"

namespace content {

StreamHandleImpl::StreamHandleImpl(const base::WeakPtr<Stream>& stream,
                                   const GURL& original_url,
                                   const std::string& mime_type,
                                   const std::string& response_headers)
    : stream_(stream),
      url_(stream->url()),
      original_url_(original_url),
      mime_type_(mime_type),
      response_headers_(response_headers),
      stream_message_loop_(base::MessageLoopProxy::current().get()) {}

StreamHandleImpl::~StreamHandleImpl() {
  stream_message_loop_->PostTask(FROM_HERE,
                                 base::Bind(&Stream::CloseHandle, stream_));
}

const GURL& StreamHandleImpl::GetURL() {
  return url_;
}

const GURL& StreamHandleImpl::GetOriginalURL() {
  return original_url_;
}

const std::string& StreamHandleImpl::GetMimeType() {
  return mime_type_;
}

const std::string& StreamHandleImpl::GetResponseHeaders() {
  return response_headers_;
}

}  // namespace content
