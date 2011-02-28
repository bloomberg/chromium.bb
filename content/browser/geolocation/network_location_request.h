// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_NETWORK_LOCATION_REQUEST_H_
#define CONTENT_BROWSER_GEOLOCATION_NETWORK_LOCATION_REQUEST_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/net/url_fetcher.h"
#include "content/browser/geolocation/device_data_provider.h"
#include "googleurl/src/gurl.h"

class URLRequestContextGetter;
class URLFetcher;
struct Geoposition;
struct Position;

// Takes a set of device data and sends it to a server to get a position fix.
// It performs formatting of the request and interpretation of the response.
class NetworkLocationRequest : private URLFetcher::Delegate {
 public:
  // ID passed to URLFetcher::Create(). Used for testing.
  static int url_fetcher_id_for_tests;
  // Interface for receiving callbacks from a NetworkLocationRequest object.
  class ListenerInterface {
   public:
    // Updates the listener with a new position. server_error indicates whether
    // was a server or network error - either no response or a 500 error code.
    virtual void LocationResponseAvailable(
        const Geoposition& position,
        bool server_error,
        const string16& access_token,
        const GatewayData& gateway_data,
        const RadioData& radio_data,
        const WifiData& wifi_data) = 0;

   protected:
    virtual ~ListenerInterface() {}
  };

  // |url| is the server address to which the request wil be sent.
  NetworkLocationRequest(URLRequestContextGetter* context,
                         const GURL& url,
                         ListenerInterface* listener);
  virtual ~NetworkLocationRequest();

  // Makes a new request. Returns true if the new request was successfully
  // started. In all cases, any currently pending request will be canceled.
  bool MakeRequest(const std::string& host,
                   const string16& access_token,
                   const GatewayData& gateway_data,
                   const RadioData& radio_data,
                   const WifiData& wifi_data,
                   const base::Time& timestamp);

  bool is_request_pending() const { return url_fetcher_ != NULL; }
  const GURL& url() const { return url_; }

 private:
  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  scoped_refptr<URLRequestContextGetter> url_context_;
  ListenerInterface* listener_;
  const GURL url_;
  scoped_ptr<URLFetcher> url_fetcher_;

  // Keep a copy of the data sent in the request, so we can refer back to it
  // when the response arrives.
  GatewayData gateway_data_;
  RadioData radio_data_;
  WifiData wifi_data_;
  base::Time timestamp_;  // Timestamp of the above data, not of the request.

  DISALLOW_COPY_AND_ASSIGN(NetworkLocationRequest);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_NETWORK_LOCATION_REQUEST_H_
