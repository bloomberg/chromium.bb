// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares GPS providers that run on linux. Currently, just uses
// the libgps (gpsd) API. Public for testing only - for normal usage this
// header should not be required, as location_provider.h declares the needed
// factory function.

#ifndef CONTENT_BROWSER_GEOLOCATION_GPS_LOCATION_PROVIDER_LINUX_H_
#define CONTENT_BROWSER_GEOLOCATION_GPS_LOCATION_PROVIDER_LINUX_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/geolocation/location_provider.h"
#include "content/common/content_export.h"
#include "content/public/common/geoposition.h"

#if defined(USE_LIBGPS)
#include "library_loaders/libgps.h"
#endif

struct gps_data_t;

namespace content {

// Defines a wrapper around the C libgps API (gps.h). Similar to the libgpsmm.h
// API provided by that package.
class CONTENT_EXPORT LibGps {
 public:
  virtual ~LibGps();
  // Attempts to dynamically load the libgps.so library and returns NULL on
  // failure.
  static LibGps* New();

  bool Start();
  void Stop();
  bool Read(content::Geoposition* position);

 protected:
  LibGps();

  // Returns false if there is no fix available.
  virtual bool GetPositionIfFixed(content::Geoposition* position);

#if defined(USE_LIBGPS)
  LibGpsLoader libgps_loader_;

  scoped_ptr<gps_data_t> gps_data_;
  bool is_open_;
#endif  // defined(USE_LIBGPS)

 private:
  DISALLOW_COPY_AND_ASSIGN(LibGps);
};

// Location provider for Linux, that uses libgps/gpsd to obtain position fixes.
// TODO(joth): Currently this runs entirely in the client thread (i.e. Chrome's
// IO thread). As the older libgps API is not designed to support polling,
// there's a chance it could block, so better move this into its own worker
// thread.
class CONTENT_EXPORT GpsLocationProviderLinux : public LocationProviderBase {
 public:
  typedef LibGps* (*LibGpsFactory)();
  // |factory| will be used to create the gpsd client library wrapper. (Note
  // NewSystemLocationProvider() will use the default factory).
  explicit GpsLocationProviderLinux(LibGpsFactory libgps_factory);
  virtual ~GpsLocationProviderLinux();

  void SetGpsdReconnectIntervalMillis(int value) {
    gpsd_reconnect_interval_millis_ = value;
  }
  void SetPollPeriodMovingMillis(int value) {
    poll_period_moving_millis_ = value;
  }
  void SetPollPeriodStationaryMillis(int value) {
    poll_period_stationary_millis_ = value;
  }

  // LocationProvider
  virtual bool StartProvider(bool high_accuracy) OVERRIDE;
  virtual void StopProvider() OVERRIDE;
  virtual void GetPosition(Geoposition* position) OVERRIDE;
  virtual void UpdatePosition() OVERRIDE;

 private:
  // Task which run in the child thread.
  void DoGpsPollTask();

  // Will schedule a poll; i.e. enqueue DoGpsPollTask deferred task.
  void ScheduleNextGpsPoll(int interval);

  int gpsd_reconnect_interval_millis_;
  int poll_period_moving_millis_;
  int poll_period_stationary_millis_;

  const LibGpsFactory libgps_factory_;
  scoped_ptr<LibGps> gps_;
  Geoposition position_;

  // Holder for the tasks which run on the thread; takes care of cleanup.
  base::WeakPtrFactory<GpsLocationProviderLinux> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_GPS_LOCATION_PROVIDER_LINUX_H_
