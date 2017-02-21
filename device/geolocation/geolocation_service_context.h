// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_GEOLOCATION_SERVICE_CONTEXT_H_
#define DEVICE_GEOLOCATION_GEOLOCATION_SERVICE_CONTEXT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "device/geolocation/geolocation_export.h"
#include "device/geolocation/public/interfaces/geolocation.mojom.h"

namespace device {

class GeolocationServiceImpl;
struct Geoposition;

// Provides information to a set of GeolocationServiceImpl instances that are
// associated with a given context. Notably, allows pausing and resuming
// geolocation on these instances.
class DEVICE_GEOLOCATION_EXPORT GeolocationServiceContext {
 public:
  GeolocationServiceContext();
  virtual ~GeolocationServiceContext();

  // Creates a GeolocationServiceImpl that is weakly bound to |request|.
  void CreateService(mojo::InterfaceRequest<mojom::GeolocationService> request);

  // Called when a service has a connection error. After this call, it is no
  // longer safe to access |service|.
  void ServiceHadConnectionError(GeolocationServiceImpl* service);

  // Enables geolocation override. This method can be used to trigger possible
  // location-specific behavior in a particular context.
  void SetOverride(std::unique_ptr<Geoposition> geoposition);

  // Disables geolocation override.
  void ClearOverride();

 private:
  std::vector<std::unique_ptr<GeolocationServiceImpl>> services_;

  std::unique_ptr<Geoposition> geoposition_override_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationServiceContext);
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_GEOLOCATION_SERVICE_CONTEXT_H_
