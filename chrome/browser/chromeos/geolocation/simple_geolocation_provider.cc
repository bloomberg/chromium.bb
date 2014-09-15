// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/geolocation/simple_geolocation_provider.h"

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/geolocation/geoposition.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace chromeos {

namespace {
const char kDefaultGeolocationProviderUrl[] =
    "https://www.googleapis.com/geolocation/v1/geolocate?";
}  // namespace

SimpleGeolocationProvider::SimpleGeolocationProvider(
    net::URLRequestContextGetter* url_context_getter,
    const GURL& url)
    : url_context_getter_(url_context_getter), url_(url) {
}

SimpleGeolocationProvider::~SimpleGeolocationProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void SimpleGeolocationProvider::RequestGeolocation(
    base::TimeDelta timeout,
    SimpleGeolocationRequest::ResponseCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SimpleGeolocationRequest* request(
      new SimpleGeolocationRequest(url_context_getter_.get(), url_, timeout));
  requests_.push_back(request);

  // SimpleGeolocationProvider owns all requests. It is safe to pass unretained
  // "this" because destruction of SimpleGeolocationProvider cancels all
  // requests.
  SimpleGeolocationRequest::ResponseCallback callback_tmp(
      base::Bind(&SimpleGeolocationProvider::OnGeolocationResponse,
                 base::Unretained(this),
                 request,
                 callback));
  request->MakeRequest(callback_tmp);
}

// static
GURL SimpleGeolocationProvider::DefaultGeolocationProviderURL() {
  return GURL(kDefaultGeolocationProviderUrl);
}

void SimpleGeolocationProvider::OnGeolocationResponse(
    SimpleGeolocationRequest* request,
    SimpleGeolocationRequest::ResponseCallback callback,
    const Geoposition& geoposition,
    bool server_error,
    const base::TimeDelta elapsed) {
  DCHECK(thread_checker_.CalledOnValidThread());

  callback.Run(geoposition, server_error, elapsed);

  ScopedVector<SimpleGeolocationRequest>::iterator new_end =
      std::remove(requests_.begin(), requests_.end(), request);
  DCHECK_EQ(std::distance(new_end, requests_.end()), 1);
  requests_.erase(new_end, requests_.end());
}

}  // namespace chromeos
