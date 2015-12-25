// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOFENCING_GEOFENCING_PROVIDER_H_
#define CONTENT_BROWSER_GEOFENCING_GEOFENCING_PROVIDER_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "content/common/geofencing_types.h"

namespace blink {
struct WebCircularGeofencingRegion;
};

namespace content {

class GeofencingProvider {
 public:
  typedef base::Callback<void(GeofencingStatus)> StatusCallback;

  virtual ~GeofencingProvider() {}

  // Called by |GeofencingService| to register a new fence. GeofencingService is
  // responsible for handling things like duplicate regions, so platform
  // specific implementations shouldn't have to worry about things like that.
  // Also GeofencingService should be making sure the total number of geofences
  // that is registered with the platform specific provider does not exceed the
  // number of regions supported by the platform, although that isn't
  // implemented yet.
  // Implementations of RegisterRegion must asynchronously call the |callback|
  // to indicate success or failure.
  virtual void RegisterRegion(int64_t geofencing_registration_id,
                              const blink::WebCircularGeofencingRegion& region,
                              const StatusCallback& callback) = 0;

  // Called by |GeofencingService| to unregister an existing registration. Will
  // only be called once for each registration.
  virtual void UnregisterRegion(int64_t geofencing_registration_id) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOFENCING_GEOFENCING_PROVIDER_H_
