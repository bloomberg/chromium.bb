// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOFENCING_MOCK_GEOFENCING_SERVICE_H_
#define CONTENT_BROWSER_GEOFENCING_MOCK_GEOFENCING_SERVICE_H_

#include <stdint.h>

#include <map>

#include "content/browser/geofencing/geofencing_service.h"

namespace content {

// This class implements a geofencing service that doesn't rely on any
// underlying platform implementation. Instead whenever SetMockPosition is
// called this class will compare the provided position with all currently
// registered regions, and emit corresponding geofencing events.
//
// If an instance is created with |service_available| set to false, the mock
// will behave as if the platform does not support geofencing.
class MockGeofencingService : public GeofencingService {
 public:
  explicit MockGeofencingService(bool service_available);
  ~MockGeofencingService() override;

  void SetMockPosition(double latitude, double longitude);

  // GeofencingService implementation.
  bool IsServiceAvailable() override;
  int64_t RegisterRegion(const blink::WebCircularGeofencingRegion& region,
                         GeofencingRegistrationDelegate* delegate) override;
  void UnregisterRegion(int64_t geofencing_registration_id) override;

 private:
  struct Registration;

  bool available_;
  std::map<int64_t, Registration> registrations_;
  int64_t next_id_;
  bool has_position_;
  double last_latitude_;
  double last_longitude_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOFENCING_MOCK_GEOFENCING_SERVICE_H_
