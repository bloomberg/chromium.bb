// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mock_resource_context.h"

#include "net/url_request/url_request_test_util.h"

namespace content {

MockResourceContext::MockResourceContext()
    : test_request_context_(new TestURLRequestContext),
      media_observer_(NULL),
      media_stream_manager_(NULL),
      audio_manager_(NULL) {
}

MockResourceContext::MockResourceContext(net::URLRequestContext* context)
    : test_request_context_(context),
      media_observer_(NULL),
      media_stream_manager_(NULL),
      audio_manager_(NULL) {
}

MockResourceContext::~MockResourceContext() {}

net::HostResolver* MockResourceContext::GetHostResolver()  {
  return NULL;
}

net::URLRequestContext* MockResourceContext::GetRequestContext()  {
  return test_request_context_;
}

MediaObserver* MockResourceContext::GetMediaObserver()  {
  return media_observer_;
}

media_stream::MediaStreamManager*
    MockResourceContext::GetMediaStreamManager() {
  return media_stream_manager_;
}

AudioManager* MockResourceContext::GetAudioManager() {
  return audio_manager_;
}

}  // namespace content
