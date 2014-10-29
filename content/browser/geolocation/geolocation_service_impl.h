// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "content/browser/geolocation/geolocation_provider_impl.h"
#include "content/common/geolocation_service.mojom.h"

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_SERVICE_IMPL_H_

namespace content {

class GeolocationProvider;
class GeolocationServiceContext;

// Implements the GeolocationService Mojo interface.
class GeolocationServiceImpl : public mojo::InterfaceImpl<GeolocationService> {
 public:
  // |context| must outlive this object. |update_callback| will be
  // called when location updates are sent.
  GeolocationServiceImpl(GeolocationServiceContext* context,
                         const base::Closure& update_callback);
  ~GeolocationServiceImpl() override;

  // Starts listening for updates.
  void StartListeningForUpdates();

  // Pauses and resumes sending updates to the client of this instance.
  void PauseUpdates();
  void ResumeUpdates();

  // Enables and disables geolocation override.
  void SetOverride(const Geoposition& position);
  void ClearOverride();

 private:
  // GeolocationService:
  void SetHighAccuracy(bool high_accuracy) override;

  // mojo::InterfaceImpl:
  void OnConnectionError() override;

  void OnLocationUpdate(const Geoposition& position);

  // Owns this object.
  GeolocationServiceContext* context_;
  scoped_ptr<GeolocationProvider::Subscription> geolocation_subscription_;
  base::Closure update_callback_;

  // Valid iff SetOverride() has been called and ClearOverride() has not
  // subsequently been called.
  Geoposition position_override_;

  // Whether this instance is currently observing location updates with high
  // accuracy.
  bool high_accuracy_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_SERVICE_IMPL_H_
