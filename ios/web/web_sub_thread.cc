// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_sub_thread.h"

#include "base/threading/thread_restrictions.h"
#include "ios/web/public/web_thread.h"
#include "ios/web/web_thread_impl.h"
#include "net/url_request/url_fetcher.h"

namespace web {

WebSubThread::WebSubThread(WebThread::ID identifier)
    : WebThreadImpl(identifier) {}

WebSubThread::WebSubThread(WebThread::ID identifier,
                           base::MessageLoop* message_loop)
    : WebThreadImpl(identifier, message_loop) {}

WebSubThread::~WebSubThread() {
  Stop();
}

void WebSubThread::Init() {
  WebThreadImpl::Init();

  if (WebThread::CurrentlyOn(WebThread::IO)) {
    // Though this thread is called the "IO" thread, it actually just routes
    // messages around; it shouldn't be allowed to perform any blocking disk
    // I/O.
    base::DisallowUnresponsiveTasks();
  }
}

void WebSubThread::CleanUp() {
  if (WebThread::CurrentlyOn(WebThread::IO))
    IOThreadPreCleanUp();

  WebThreadImpl::CleanUp();
}

void WebSubThread::IOThreadPreCleanUp() {
  // Kill all things that might be holding onto
  // net::URLRequest/net::URLRequestContexts.

  // Destroy all URLRequests started by URLFetchers.
  net::URLFetcher::CancelAll();
}

}  // namespace web
