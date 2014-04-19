// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/timezone/timezone_provider.h"

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/geolocation/geoposition.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace chromeos {

TimeZoneProvider::TimeZoneProvider(
    net::URLRequestContextGetter* url_context_getter,
    const GURL& url)
    : url_context_getter_(url_context_getter), url_(url) {
}

TimeZoneProvider::~TimeZoneProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TimeZoneProvider::RequestTimezone(
    const Geoposition& position,
    bool sensor,
    base::TimeDelta timeout,
    TimeZoneRequest::TimeZoneResponseCallback callback) {
  TimeZoneRequest* request(new TimeZoneRequest(
      url_context_getter_, url_, position, sensor, timeout));
  requests_.push_back(request);

  // TimeZoneProvider owns all requests. It is safe to pass unretained "this"
  // because destruction of TimeZoneProvider cancels all requests.
  TimeZoneRequest::TimeZoneResponseCallback callback_tmp(
      base::Bind(&TimeZoneProvider::OnTimezoneResponse,
                 base::Unretained(this),
                 request,
                 callback));
  request->MakeRequest(callback_tmp);
}

void TimeZoneProvider::OnTimezoneResponse(
    TimeZoneRequest* request,
    TimeZoneRequest::TimeZoneResponseCallback callback,
    scoped_ptr<TimeZoneResponseData> timezone,
    bool server_error) {
  ScopedVector<TimeZoneRequest>::iterator new_end =
      std::remove(requests_.begin(), requests_.end(), request);
  DCHECK_EQ(std::distance(new_end, requests_.end()), 1);
  requests_.erase(new_end, requests_.end());

  callback.Run(timezone.Pass(), server_error);
}

}  // namespace chromeos
