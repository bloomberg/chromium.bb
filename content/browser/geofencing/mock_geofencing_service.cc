// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "content/browser/geofencing/mock_geofencing_service.h"

#include <cmath>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/geofencing/geofencing_registration_delegate.h"
#include "third_party/WebKit/public/platform/WebCircularGeofencingRegion.h"

namespace content {

namespace {

void RegisterRegionResult(GeofencingRegistrationDelegate* delegate,
                          int64_t geofencing_registration_id,
                          GeofencingStatus status) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&GeofencingRegistrationDelegate::RegistrationFinished,
                 base::Unretained(delegate), geofencing_registration_id,
                 status));
}

double DegreesToRadians(float degrees) {
  return (M_PI * degrees) / 180.f;
}

double Haversin(double theta) {
  double temp = sin(theta / 2);
  return temp * temp;
}

// Calculates the distance in meters between two points with coordinates in
// degrees.
double Distance(double lat1, double long1, double lat2, double long2) {
  double R = 6371000;  // radius of earth in meters
  double phi1 = DegreesToRadians(lat1);
  double phi2 = DegreesToRadians(lat2);
  double dphi = DegreesToRadians(lat2 - lat1);
  double dlambda = DegreesToRadians(long2 - long1);
  double haversine = Haversin(dphi) + cos(phi1) * cos(phi2) * Haversin(dlambda);
  return 2 * R * asin(sqrt(haversine));
}

// Returns true iff the provided coordinate is inside the region.
bool PositionInRegion(double latitude,
                      double longitude,
                      const blink::WebCircularGeofencingRegion& region) {
  return Distance(latitude, longitude, region.latitude, region.longitude) <=
         region.radius;
}
}

struct MockGeofencingService::Registration {
  blink::WebCircularGeofencingRegion region;
  GeofencingRegistrationDelegate* delegate;
  // True iff the last event emitted for this region was a RegionEntered event.
  bool is_inside;
};

MockGeofencingService::MockGeofencingService(bool service_available)
    : available_(service_available),
      next_id_(0),
      has_position_(false),
      last_latitude_(0),
      last_longitude_(0) {
}

MockGeofencingService::~MockGeofencingService() {
}

void MockGeofencingService::SetMockPosition(double latitude, double longitude) {
  has_position_ = true;
  last_latitude_ = latitude;
  last_longitude_ = longitude;
  for (auto& registration : registrations_) {
    bool is_inside =
        PositionInRegion(latitude, longitude, registration.second.region);
    if (is_inside != registration.second.is_inside) {
      if (is_inside)
        registration.second.delegate->RegionEntered(registration.first);
      else
        registration.second.delegate->RegionExited(registration.first);
    }
    registration.second.is_inside = is_inside;
  }
}

bool MockGeofencingService::IsServiceAvailable() {
  return available_;
}

int64_t MockGeofencingService::RegisterRegion(
    const blink::WebCircularGeofencingRegion& region,
    GeofencingRegistrationDelegate* delegate) {
  int64_t id = next_id_++;
  Registration& registration = registrations_[id];
  registration.region = region;
  registration.delegate = delegate;
  registration.is_inside =
      has_position_ &&
      PositionInRegion(last_latitude_, last_longitude_, region);
  RegisterRegionResult(delegate, id, GEOFENCING_STATUS_OK);
  if (registration.is_inside) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&GeofencingRegistrationDelegate::RegionEntered,
                              base::Unretained(delegate), id));
  }
  return id;
}

void MockGeofencingService::UnregisterRegion(
    int64_t geofencing_registration_id) {
  registrations_.erase(geofencing_registration_id);
}

}  // namespace content
