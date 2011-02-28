// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares GPS providers that run on linux. Currently, just uses
// the libgps (gpsd) API. Public for testing only - for normal usage this
// header should not be required, as location_provider.h declares the needed
// factory function.

#ifndef CONTENT_BROWSER_GEOLOCATION_GPS_LOCATION_PROVIDER_LINUX_H_
#define CONTENT_BROWSER_GEOLOCATION_GPS_LOCATION_PROVIDER_LINUX_H_
#pragma once

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/common/geoposition.h"
#include "content/browser/geolocation/location_provider.h"

class LibGps;

// Location provider for Linux, that uses libgps/gpsd to obtain position fixes.
// TODO(joth): Currently this runs entirely in the client thread (i.e. Chrome's
// IO thread). As the older libgps API is not designed to support polling,
// there's a chance it could block, so better move this into its own worker
// thread.
class GpsLocationProviderLinux : public LocationProviderBase {
 public:
  typedef LibGps* (*LibGpsFactory)();
  // |factory| will be used to create the gpsd client library wrapper. (Note
  // NewSystemLocationProvider() will use the default factory).
  explicit GpsLocationProviderLinux(LibGpsFactory libgps_factory);
  virtual ~GpsLocationProviderLinux();

  // LocationProvider
  virtual bool StartProvider(bool high_accuracy);
  virtual void StopProvider();
  virtual void GetPosition(Geoposition* position);
  virtual void UpdatePosition();
  virtual void OnPermissionGranted(const GURL& requesting_frame);

 private:
  // Task which run in the child thread.
  void DoGpsPollTask();

  // Will schedule a poll; i.e. enqueue DoGpsPollTask deferred task.
  void ScheduleNextGpsPoll(int interval);

  const LibGpsFactory libgps_factory_;
  scoped_ptr<LibGps> gps_;
  Geoposition position_;

  // Holder for the tasks which run on the thread; takes care of cleanup.
  ScopedRunnableMethodFactory<GpsLocationProviderLinux> task_factory_;
};

#endif  // CONTENT_BROWSER_GEOLOCATION_GPS_LOCATION_PROVIDER_LINUX_H_
