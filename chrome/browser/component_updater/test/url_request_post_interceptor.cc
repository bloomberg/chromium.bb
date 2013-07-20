// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/test/url_request_post_interceptor.h"

#include "content/public/test/test_browser_thread.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_util.h"

using content::BrowserThread;

URLRequestPostInterceptor::URLRequestPostInterceptor(
    RequestCounter* counter) : delegate_(new Delegate(counter)) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Delegate::Register,
                                     base::Unretained(delegate_)));
}

URLRequestPostInterceptor::~URLRequestPostInterceptor() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Delegate::Unregister,
                                     base::Unretained(delegate_)));
}

URLRequestPostInterceptor::Delegate::Delegate(
    RequestCounter* counter) : counter_(counter) {
}

void URLRequestPostInterceptor::Delegate::Register() {
  net::URLRequestFilter::GetInstance()->AddHostnameProtocolHandler(
      "http", "localhost2",
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>(this));
}

void URLRequestPostInterceptor::Delegate::Unregister() {
  net::URLRequestFilter::GetInstance()->
      RemoveHostnameHandler("http", "localhost2");
}

net::URLRequestJob* URLRequestPostInterceptor::Delegate::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (request->has_upload()) {
    counter_->Trial(request);
    return new URLRequestPingMockJob(request, network_delegate);
  }
  return NULL;
}

URLRequestPingMockJob::URLRequestPingMockJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate)
    : net::URLRequestSimpleJob(request, network_delegate) {
}

int URLRequestPingMockJob::GetData(
    std::string* mime_type,
    std::string* charset,
    std::string* data,
    const net::CompletionCallback& callback) const {
  mime_type->assign("text/plain");
  charset->assign("US-ASCII");
  data->assign("");  // There is no reason to have a response body.
  return net::OK;
}
