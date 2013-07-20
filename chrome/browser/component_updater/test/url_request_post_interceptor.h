// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_TEST_URL_REQUEST_POST_INTERCEPTOR_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_TEST_URL_REQUEST_POST_INTERCEPTOR_H_

#include "base/basictypes.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_simple_job.h"

namespace net {
class NetworkDelegate;
class URLRequest;
}

class RequestCounter {
 public:
  virtual void Trial(net::URLRequest* request) = 0;
};

class URLRequestPostInterceptor {
 public:
  explicit URLRequestPostInterceptor(RequestCounter* counter);
  virtual ~URLRequestPostInterceptor();

 private:
  class Delegate;

  // After creation, |delegate_| lives on the IO thread, and a task to delete it
  // is posted from ~URLRequestPostInterceptor().
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestPostInterceptor);
};

class URLRequestPostInterceptor::Delegate
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  explicit Delegate(RequestCounter* counter);
  virtual ~Delegate() {}

  void Register();

  void Unregister();

 private:
  RequestCounter* counter_;

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;
};

class URLRequestPingMockJob : public net::URLRequestSimpleJob {
 public:
  URLRequestPingMockJob(net::URLRequest* request,
                        net::NetworkDelegate* network_delegate);

 protected:
  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* data,
                      const net::CompletionCallback& callback) const OVERRIDE;

 private:
  virtual ~URLRequestPingMockJob() {}

  DISALLOW_COPY_AND_ASSIGN(URLRequestPingMockJob);
};

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_TEST_URL_REQUEST_POST_INTERCEPTOR_H_
