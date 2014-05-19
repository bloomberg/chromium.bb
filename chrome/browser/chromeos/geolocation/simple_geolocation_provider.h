// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GEOLOCATION_SIMPLE_GEOLOCATION_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_GEOLOCATION_SIMPLE_GEOLOCATION_PROVIDER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/geolocation/simple_geolocation_request.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {

// This class implements Google Maps Geolocation API.
//
// SimpleGeolocationProvider must be created and used on the same thread.
//
// Note: this should probably be a singleton to monitor requests rate.
// But as it is used only diring ChromeOS Out-of-Box, it can be owned by
// WizardController for now.
class SimpleGeolocationProvider {
 public:
  SimpleGeolocationProvider(net::URLRequestContextGetter* url_context_getter,
                            const GURL& url);
  virtual ~SimpleGeolocationProvider();

  // Initiates new request (See SimpleGeolocationRequest for parameters
  // description.)
  void RequestGeolocation(base::TimeDelta timeout,
                          SimpleGeolocationRequest::ResponseCallback callback);

  // Returns default geolocation service URL.
  static GURL DefaultGeolocationProviderURL();

 private:
  friend class TestGeolocationAPIURLFetcherCallback;

  // Geolocation response callback. Deletes request from requests_.
  void OnGeolocationResponse(
      SimpleGeolocationRequest* request,
      SimpleGeolocationRequest::ResponseCallback callback,
      const Geoposition& geoposition,
      bool server_error,
      const base::TimeDelta elapsed);

  scoped_refptr<net::URLRequestContextGetter> url_context_getter_;

  // URL of the Google Maps Geolocation API.
  const GURL url_;

  // Requests in progress.
  // SimpleGeolocationProvider owns all requests, so this vector is deleted on
  // destroy.
  ScopedVector<SimpleGeolocationRequest> requests_;

  // Creation and destruction should happen on the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SimpleGeolocationProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_GEOLOCATION_SIMPLE_GEOLOCATION_PROVIDER_H_
