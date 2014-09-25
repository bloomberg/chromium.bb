// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_FRAUDULENT_CERTIFICATE_REPORTER_H_
#define CHROME_BROWSER_NET_CHROME_FRAUDULENT_CERTIFICATE_REPORTER_H_

#include <set>
#include <string>

#include "net/url_request/fraudulent_certificate_reporter.h"
#include "net/url_request/url_request.h"

namespace net {
class URLRequestContext;
}

namespace chrome_browser_net {

class ChromeFraudulentCertificateReporter
    : public net::FraudulentCertificateReporter,
      public net::URLRequest::Delegate {
 public:
  explicit ChromeFraudulentCertificateReporter(
      net::URLRequestContext* request_context);

  virtual ~ChromeFraudulentCertificateReporter();

  // Allows users of this class to override this and set their own URLRequest
  // type. Used by SendReport.
  virtual scoped_ptr<net::URLRequest> CreateURLRequest(
      net::URLRequestContext* context);

  // net::FraudulentCertificateReporter
  virtual void SendReport(const std::string& hostname,
                          const net::SSLInfo& ssl_info) OVERRIDE;

  // net::URLRequest::Delegate
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE;

 protected:
  net::URLRequestContext* const request_context_;

 private:
  // Performs post-report cleanup.
  void RequestComplete(net::URLRequest* request);

  const GURL upload_url_;
  // Owns the contained requests.
  std::set<net::URLRequest*> inflight_requests_;

  DISALLOW_COPY_AND_ASSIGN(ChromeFraudulentCertificateReporter);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_CHROME_FRAUDULENT_CERTIFICATE_REPORTER_H_

