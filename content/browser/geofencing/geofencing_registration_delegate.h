// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOFENCING_GEOFENCING_REGISTRATION_DELEGATE_H_
#define CONTENT_BROWSER_GEOFENCING_GEOFENCING_REGISTRATION_DELEGATE_H_

#include <stdint.h>

#include "content/common/geofencing_types.h"

namespace content {

// |GeofencingService| has an instance of this class associated with each
// geofence registration, and uses it to inform about events related to the
// registration, such as the geofence finishing being registered.
// These methods will always be called on the IO thread.
// TODO(mek): Add methods for geofence enter/leave events.
class GeofencingRegistrationDelegate {
 public:
  virtual void RegistrationFinished(int64_t geofencing_registration_id,
                                    GeofencingStatus status) = 0;
  virtual void RegionEntered(int64_t geofencing_registration_id) = 0;
  virtual void RegionExited(int64_t geofencing_registration_id) = 0;

 protected:
  virtual ~GeofencingRegistrationDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOFENCING_GEOFENCING_REGISTRATION_DELEGATE_H_
