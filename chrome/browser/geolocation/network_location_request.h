// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_NETWORK_LOCATION_REQUEST_H_
#define CHROME_BROWSER_GEOLOCATION_NETWORK_LOCATION_REQUEST_H_

#include <string>
#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/geolocation/device_data_provider.h"
#include "chrome/browser/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

struct Position;
class URLRequestContextGetter;
class URLFetcher;

// Takes a set of device data and sends it to a server to get a position fix.
// It performs formatting of the request and interpretation of the response.
class NetworkLocationRequest : private URLFetcher::Delegate {
 public:
  // Interface for receiving callbacks from a NetworkLocationRequest object.
  class ListenerInterface {
   public:
    // Updates the listener with a new position. server_error indicates whether
    // was a server or network error - either no response or a 500 error code.
    virtual void LocationResponseAvailable(
        const Position& position,
        bool server_error,
        const string16& access_token) = 0;

   protected:
    virtual ~ListenerInterface() {}
  };

  // |url| is the server address to which the request wil be sent, |host_name|
  // is the host of the webpage that caused this request.
  // TODO(joth): is host needed? What to do when we reuse cached locations?
  NetworkLocationRequest(URLRequestContextGetter* context,
                         const GURL& url,
                         const string16& host_name,
                         ListenerInterface* listener);
  virtual ~NetworkLocationRequest();

  // Makes a new request. Returns true if the new request was successfully
  // started. In all cases, any currently pending request will be canceled.
  bool MakeRequest(const string16& access_token,
                   const RadioData& radio_data,
                   const WifiData& wifi_data,
                   int64 timestamp);

  bool is_request_pending() const { return url_fetcher_ != NULL; }
  const GURL& url() const { return url_; }

 private:
  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  scoped_refptr<URLRequestContextGetter> url_context_;
  int64 timestamp_;  // The timestamp of the data used to make the request.
  ListenerInterface* listener_;
  const GURL url_;
  string16 host_name_;
  scoped_ptr<URLFetcher> url_fetcher_;

  DISALLOW_EVIL_CONSTRUCTORS(NetworkLocationRequest);
};

#endif  // CHROME_BROWSER_GEOLOCATION_NETWORK_LOCATION_REQUEST_H_
