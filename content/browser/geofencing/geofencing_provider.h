// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOFENCING_GEOFENCING_PROVIDER_H_
#define CONTENT_BROWSER_GEOFENCING_GEOFENCING_PROVIDER_H_

#include "base/callback_forward.h"
#include "content/common/geofencing_status.h"

namespace blink {
struct WebCircularGeofencingRegion;
};

namespace content {

class GeofencingProvider {
 public:
  // Callback that gets called on completion of registering a new region. The
  // status indicates success or failure, and in case of success, an id to use
  // to later unregister the region is passed as |registration_id|. If
  // registration failed, providers should set |registration_id| to -1.
  typedef base::Callback<void(GeofencingStatus, int registration_id)>
      RegisterCallback;

  virtual ~GeofencingProvider() {}

  // Called by |GeofencingManager| to register a new fence. GeofencingManager is
  // responsible for handling things like duplicate regions, so platform
  // specific implementations shouldn't have to worry about things like that.
  // Also GeofencingManager should be making sure the total number of geofences
  // that is registered with the platform specific provider does not exceed the
  // number of regions supported by the platform, although that isn't
  // implemented yet.
  virtual void RegisterRegion(const blink::WebCircularGeofencingRegion& region,
                              const RegisterCallback& callback) = 0;

  // Called by |GeofencingManager| to unregister an existing registration. Will
  // only be called once for each registration.
  virtual void UnregisterRegion(int registration_id) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOFENCING_GEOFENCING_PROVIDER_H_
