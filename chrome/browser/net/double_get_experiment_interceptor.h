// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DOUBLE_GET_EXPERIMENT_INTERCEPTOR_H_
#define CHROME_BROWSER_NET_DOUBLE_GET_EXPERIMENT_INTERCEPTOR_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_job_factory.h"

// URL interceptor that will be used to experiment with repeating GET requests.
// The interceptor intercepts HTTP GET method responses that have one of
// predetermined MIME types (MS Office files) and creates new request redirect
// job to the same URL as in the received response. This effectively repeats the
// selected GET requests. The interceptor then listens for responses to the
// redirect request job and records the response code to UMA server.
// Only GET requests that cannot be cached are repeated.
//
// All HTTP request responses with the interesting MIME types are also recorded
// as UMA metrics, based on their method type (POST, CACHABLE GET,
// UNCACHABLE GET).
// TODO(tbarzic): Remove this when enough data is collected.
class DoubleGetExperimentInterceptor
    : public net::URLRequestJobFactory::Interceptor {
 public:
  // Uploads relevant UMA stats.
  class MetricsRecorder {
   public:
    virtual ~MetricsRecorder() {}

    // Reports POST request occurrence.
    virtual void RecordPost() const = 0;
    // Reports GET request occurrence.
    virtual void RecordGet(bool is_cacheable) const = 0;
    // Reports the response code of the repeated GET request.
    virtual void RecordRepeatedGet(int response_code) const = 0;
  };

  // DoubleGetExperimentInterceptor takes the ownership of |metrics_recorder|.
  // Metrics recorder should be NULL in non-testing code. When metrics_recorder
  // is NULL, default implementation of MetricsRecorder interface is used by the
  // interceptor (MetricsRecorderImpl in .cc).
  explicit DoubleGetExperimentInterceptor(MetricsRecorder* metrics_recorder);
  virtual ~DoubleGetExperimentInterceptor();

  // net::URLRequestJobFactory Overrides.

  // NO-OP.
  virtual net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;
  // NO-OP.
  virtual net::URLRequestJob* MaybeInterceptRedirect(
      const GURL& location,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;
  // Processes request responses as described in the class description.
  virtual net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;

 private:
  // The set of mime_types to be redirected.
  std::set<std::string> mime_types_to_intercept_;
  // The set of repeated requests' ids.
  mutable std::set<int64> intercepted_requests_;
  // MetricsRecorder to be used to report metrics.
  scoped_ptr<MetricsRecorder> metrics_recorder_;
};

#endif  // CHROME_BROWSER_NET_DOUBLE_GET_EXPERIMENT_INTERCEPTOR_H_
