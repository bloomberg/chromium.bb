// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GEOLOCATION_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_GEOLOCATION_PROVIDER_H_

#include "base/callback_forward.h"
#include "content/common/content_export.h"

namespace content {
struct Geoposition;

class CONTENT_EXPORT GeolocationProvider {
 public:
  // This method, and all below, can only be called on the IO thread unless
  // otherwise specified.
  static GeolocationProvider* GetInstance();

  typedef base::Callback<void(const Geoposition&)> LocationUpdateCallback;

  // |use_high_accuracy| is used as a 'hint' for the provider preferences for
  // this particular observer, however the observer could receive updates for
  // best available locations from any active provider whilst it is registered.
  // If an existing observer is added a second time, its options are updated
  // but only a single call to RemoveLocationUpdateCallback() is required to
  // remove it.
  virtual void AddLocationUpdateCallback(const LocationUpdateCallback& callback,
                                         bool use_high_accuracy) = 0;

  // Remove a previously registered observer. No-op if not previously registered
  // via AddLocationUpdateCallback(). Returns true if the observer was removed.
  virtual bool RemoveLocationUpdateCallback(
      const LocationUpdateCallback& callback) = 0;

  // Calling this method indicates the user has opted into using location
  // services, including sending network requests to [Google servers to] resolve
  // the user's location. Use this method carefully, in line with the rules in
  // go/chrome-privacy-doc.
  virtual void UserDidOptIntoLocationServices() = 0;

  // Overrides the current location for testing. This function may be called on
  // any thread. The completion callback will be invoked asynchronously on the
  // calling thread when the override operation is completed.
  //
  // This function allows the current location to be faked without having to
  // manually instantiate a GeolocationProvider backed by a MockLocationProvider
  // that serves a fake location.
  //
  // Do not use this function in unit tests. The function instantiates the
  // singleton geolocation stack in the background and manipulates it to report
  // a fake location. Neither step can be undone, breaking unit test isolation
  // (crbug.com/125931).
  static void OverrideLocationForTesting(
      const Geoposition& position,
      const base::Closure& completion_callback);

 protected:
  virtual~GeolocationProvider() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GEOLOCATION_PROVIDER_H_
