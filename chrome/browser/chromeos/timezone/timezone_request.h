// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TIMEZONE_TIMEZONE_REQUEST_H_
#define CHROME_BROWSER_CHROMEOS_TIMEZONE_TIMEZONE_REQUEST_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/geolocation/geoposition.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {

struct TimeZoneResponseData {
  enum Status {
    OK,
    INVALID_REQUEST,
    OVER_QUERY_LIMIT,
    REQUEST_DENIED,
    UNKNOWN_ERROR,
    ZERO_RESULTS,
    REQUEST_ERROR  // local problem
  };

  TimeZoneResponseData();

  std::string ToStringForDebug() const;

  double dstOffset;
  double rawOffset;
  std::string timeZoneId;
  std::string timeZoneName;
  std::string error_message;
  Status status;
};

// Returns default timezone service URL.
GURL DefaultTimezoneProviderURL();

// Takes Geoposition and sends it to a server to get local timezone information.
// It performs formatting of the request and interpretation of the response.
// If error occurs, request is retried until timeout.
// Zero timeout indicates single request.
// Request is owned and destroyed by caller (usually TimeZoneProvider).
// If request is destroyed while callback has not beed called yet, request
// is silently cancelled.
class TimeZoneRequest : private net::URLFetcherDelegate {
 public:
  // Called when a new geo timezone information is available.
  // The second argument indicates whether there was a server error or not.
  // It is true when there was a server or network error - either no response
  // or a 500 error code.
  typedef base::Callback<void(scoped_ptr<TimeZoneResponseData> /* timezone */,
                              bool /* server_error */)>
      TimeZoneResponseCallback;

  // |url| is the server address to which the request wil be sent.
  // |geoposition| is the location to query timezone for.
  // |sensor| if this location was determined using hardware sensor.
  // |retry_timeout| retry request on error until timeout.
  TimeZoneRequest(net::URLRequestContextGetter* url_context_getter,
                  const GURL& service_url,
                  const Geoposition& geoposition,
                  bool sensor,
                  base::TimeDelta retry_timeout);

  virtual ~TimeZoneRequest();

  // Initiates request.
  // Note: if request object is destroyed before callback is called,
  // request will be silently cancelled.
  void MakeRequest(TimeZoneResponseCallback callback);

  void set_retry_sleep_on_server_error_for_testing(
      const base::TimeDelta value) {
    retry_sleep_on_server_error_ = value;
  }

  void set_retry_sleep_on_bad_response_for_testing(
      const base::TimeDelta value) {
    retry_sleep_on_bad_response_ = value;
  }

 private:
  // net::URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Start new request.
  void StartRequest();

  // Schedules retry.
  void Retry(bool server_error);

  scoped_refptr<net::URLRequestContextGetter> url_context_getter_;
  const GURL service_url_;
  Geoposition geoposition_;
  const bool sensor_;

  TimeZoneResponseCallback callback_;

  GURL request_url_;
  scoped_ptr<net::URLFetcher> url_fetcher_;

  // When request was actually started.
  base::Time request_started_at_;

  // Absolute time, when it is passed no more retry requests are allowed.
  base::Time retry_timeout_abs_;

  // Pending retry.
  base::OneShotTimer<TimeZoneRequest> timezone_request_scheduled_;

  base::TimeDelta retry_sleep_on_server_error_;

  base::TimeDelta retry_sleep_on_bad_response_;

  // Number of retry attempts.
  unsigned retries_;

  // Creation and destruction should happen on the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(TimeZoneRequest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_TIMEZONE_TIMEZONE_REQUEST_H_
