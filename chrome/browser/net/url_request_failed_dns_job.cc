// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_request_failed_dns_job.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"

const char URLRequestFailedDnsJob::kTestUrl[] =
    "http://url.handled.by.fake.dns/";

URLRequestFailedDnsJob::URLRequestFailedDnsJob(net::URLRequest* request)
    : net::URLRequestJob(request),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {}

URLRequestFailedDnsJob::~URLRequestFailedDnsJob() {}

void URLRequestFailedDnsJob::Start() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &URLRequestFailedDnsJob::StartAsync));
}

// static
void URLRequestFailedDnsJob::AddUrlHandler() {
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddUrlHandler(GURL(kTestUrl),
                        &URLRequestFailedDnsJob::Factory);
}

/*static */
net::URLRequestJob* URLRequestFailedDnsJob::Factory(net::URLRequest* request,
    const std::string& scheme) {
  return new URLRequestFailedDnsJob(request);
}

void URLRequestFailedDnsJob::StartAsync() {
  NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                    net::ERR_NAME_NOT_RESOLVED));
}
