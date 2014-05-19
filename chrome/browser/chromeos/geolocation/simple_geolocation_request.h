// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GEOLOCATION_SIMPLE_GEOLOCATION_REQUEST_H_
#define CHROME_BROWSER_CHROMEOS_GEOLOCATION_SIMPLE_GEOLOCATION_REQUEST_H_

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

// Sends request to a server to get local geolocation information.
// It performs formatting of the request and interpretation of the response.
// Request is owned and destroyed by caller (usually SimpleGeolocationProvider).
// - If error occurs, request is retried until timeout.
// - On successul response, callback is called.
// - On timeout, callback with last (failed) position is called.
// (position.status is set to STATUS_TIMEOUT.)
// - If request is destroyed while callback has not beed called yet, request
// is silently cancelled.
class SimpleGeolocationRequest : private net::URLFetcherDelegate {
 public:
  // Called when a new geo geolocation information is available.
  // The second argument indicates whether there was a server error or not.
  // It is true when there was a server or network error - either no response
  // or a 500 error code.
  typedef base::Callback<void(const Geoposition& /* position*/,
                              bool /* server_error */,
                              const base::TimeDelta elapsed)> ResponseCallback;

  // |url| is the server address to which the request wil be sent.
  // |timeout| retry request on error until timeout.
  SimpleGeolocationRequest(net::URLRequestContextGetter* url_context_getter,
                           const GURL& service_url,
                           base::TimeDelta timeout);

  virtual ~SimpleGeolocationRequest();

  // Initiates request.
  // Note: if request object is destroyed before callback is called,
  // request will be silently cancelled.
  void MakeRequest(const ResponseCallback& callback);

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

  // Run callback and destroy "this".
  void ReplyAndDestroySelf(const base::TimeDelta elapsed, bool server_error);

  // Called by timeout_timer_ .
  void OnTimeout();

  scoped_refptr<net::URLRequestContextGetter> url_context_getter_;

  // Service URL from constructor arguments.
  const GURL service_url_;

  ResponseCallback callback_;

  // Actual URL with parameters.
  GURL request_url_;

  scoped_ptr<net::URLFetcher> url_fetcher_;

  // When request was actually started.
  base::Time request_started_at_;

  base::TimeDelta retry_sleep_on_server_error_;

  base::TimeDelta retry_sleep_on_bad_response_;

  const base::TimeDelta timeout_;

  // Pending retry.
  base::OneShotTimer<SimpleGeolocationRequest> request_scheduled_;

  // Stop request on timeout.
  base::OneShotTimer<SimpleGeolocationRequest> timeout_timer_;

  // Number of retry attempts.
  unsigned retries_;

  // This is updated on each retry.
  Geoposition position_;

  // Creation and destruction should happen on the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SimpleGeolocationRequest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_GEOLOCATION_SIMPLE_GEOLOCATION_REQUEST_H_
