// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/geolocation.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "content/browser/geolocation/geolocation_provider.h"
#include "content/common/geoposition.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

void OverrideLocationForTestingOnIOThread(
    double latitude,
    double longitude,
    double altitude,
    const base::Closure& completion_callback,
    scoped_refptr<base::MessageLoopProxy> callback_loop) {
  Geoposition position;
  position.latitude = latitude;
  position.longitude = longitude;
  position.altitude = altitude;
  position.accuracy = 0;
  position.timestamp = base::Time::Now();
  GeolocationProvider::GetInstance()->OverrideLocationForTesting(position);
  callback_loop->PostTask(FROM_HERE, completion_callback);
}

}  // namespace

void OverrideLocationForTesting(
    double latitude,
    double longitude,
    double altitude,
    const base::Closure& completion_callback) {
  base::Closure closure = base::Bind(
      &OverrideLocationForTestingOnIOThread,
      latitude, longitude, altitude, completion_callback,
      base::MessageLoopProxy::current());
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, closure);
  } else {
    closure.Run();
  }
}

}  // namespace content
