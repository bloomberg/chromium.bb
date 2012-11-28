// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/double_get_experiment_interceptor.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "chrome/common/url_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_redirect_job.h"

using net::HttpUtil;

namespace {

const char* kMimeTypesToIntercept[] = {
  "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
  "application/vnd.ms-word",
  "application/msword"
};

enum MethodType {
  METHOD_TYPE_POST,
  METHOD_TYPE_GET_CACHABLE,
  METHOD_TYPE_GET_NON_CACHABLE,
  METHOD_TYPE_COUNT,
};

class MetricsRecorderImpl
    : public DoubleGetExperimentInterceptor::MetricsRecorder {
 public:
  MetricsRecorderImpl() {}
  virtual ~MetricsRecorderImpl() {}

  virtual void RecordPost() const OVERRIDE {
    UMA_HISTOGRAM_ENUMERATION("Net.DoubleGetExperiment_InitialResponseMethod",
                              METHOD_TYPE_POST,
                              METHOD_TYPE_COUNT);
  }

  virtual void RecordGet(bool is_cacheable) const OVERRIDE {
    MethodType method = is_cacheable ? METHOD_TYPE_GET_CACHABLE :
                                       METHOD_TYPE_GET_NON_CACHABLE;
    UMA_HISTOGRAM_ENUMERATION("Net.DoubleGetExperiment_InitialResponseMethod",
                              method,
                              METHOD_TYPE_COUNT);
  }

  virtual void RecordRepeatedGet(int response_code) const OVERRIDE {
    UMA_HISTOGRAM_CUSTOM_ENUMERATION("Net.DoubleGetExperiment_ResponseCode",
        HttpUtil::MapStatusCodeForHistogram(response_code),
        HttpUtil::GetStatusCodesForHistogram());
  }
};

// Determines if the request is cacheable.
bool RequestIsCacheable(net::URLRequest* request) {
  DCHECK(request->response_headers());
  base::TimeDelta freshness = request->response_headers()->GetFreshnessLifetime(
      request->response_time());
  return freshness > base::TimeDelta();
}

}  // namespace

DoubleGetExperimentInterceptor::DoubleGetExperimentInterceptor(
    MetricsRecorder* metrics_recorder)
    : metrics_recorder_(metrics_recorder) {
  if (!metrics_recorder_)
    metrics_recorder_.reset(new MetricsRecorderImpl());

  for (size_t i = 0; i < arraysize(kMimeTypesToIntercept); ++i)
    mime_types_to_intercept_.insert(kMimeTypesToIntercept[i]);
}

DoubleGetExperimentInterceptor::~DoubleGetExperimentInterceptor() {}

net::URLRequestJob* DoubleGetExperimentInterceptor::MaybeIntercept(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return NULL;
}

net::URLRequestJob* DoubleGetExperimentInterceptor::MaybeInterceptRedirect(
    const GURL& location,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return NULL;
}

net::URLRequestJob* DoubleGetExperimentInterceptor::MaybeInterceptResponse(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  // Ignore non http schemes.
  if (!request->url().SchemeIs(chrome::kHttpScheme) &&
      !request->url().SchemeIs(chrome::kHttpsScheme)) {
    return NULL;
  }

  // Check if the request has already been intercepted, and if so record the
  // request's response code.
  // This is done before the MIME type is tested because the failed requests
  // might not have the MIME type set.
  if (intercepted_requests_.count(request->identifier())) {
    metrics_recorder_->RecordRepeatedGet(request->GetResponseCode());
    // There is no need to keep watching for the request anymore.
    intercepted_requests_.erase(request->identifier());
    return NULL;
  }

  if (!request->status().is_success())
    return NULL;

  // Ignore the request if it doesn't have a MIME type which is used in the
  // experiment.
  std::string mime_type;
  request->GetMimeType(&mime_type);
  if (mime_types_to_intercept_.count(mime_type) == 0)
    return NULL;

  // Record POST method occurrence.
  if (request->method() == "POST")
    metrics_recorder_->RecordPost();

  // Non GET requests are ignored.
  if (request->method() != "GET")
    return NULL;

  // Cacheable GET requests are recorded in the metrics, but not repeated.
  bool is_cacheable = RequestIsCacheable(request);
  metrics_recorder_->RecordGet(is_cacheable);
  if (is_cacheable)
    return NULL;

  // Repeat the GET requests as it has non cacheable response with the right
  // MIME type.
  intercepted_requests_.insert(request->identifier());
  return new net::URLRequestRedirectJob(request, network_delegate,
                                        request->url());
}

