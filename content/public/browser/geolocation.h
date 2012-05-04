// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GEOLOCATION_H_
#define CONTENT_PUBLIC_BROWSER_GEOLOCATION_H_
#pragma once

#include "base/callback_forward.h"
#include "content/common/content_export.h"

namespace content {

struct Geoposition;

typedef base::Callback<void(const Geoposition&)> GeolocationUpdateCallback;

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
void CONTENT_EXPORT OverrideLocationForTesting(
    const Geoposition& position,
    const base::Closure& completion_callback);

// Requests a one-time callback when the next location update becomes available.
// This function may be called on any thread. The callback will be invoked on
// the calling thread.
void CONTENT_EXPORT RequestLocationUpdate(
    const GeolocationUpdateCallback& callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GEOLOCATION_H_
