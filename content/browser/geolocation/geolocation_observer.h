// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_OBSERVER_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_OBSERVER_H_
#pragma once

#include "base/basictypes.h"

struct Geoposition;

// This interface is implemented by observers of GeolocationProvider as
// well as GeolocationProvider itself as an observer of GeolocationArbitrator.
class GeolocationObserver {
 public:
  // This will be called whenever the 'best available' location is updated,
  // or when an error is encountered meaning no location data will be
  // available in the forseeable future.
  virtual void OnLocationUpdate(const Geoposition& position) = 0;

 protected:
  GeolocationObserver() {}
  virtual ~GeolocationObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GeolocationObserver);
};

struct GeolocationObserverOptions {
  GeolocationObserverOptions() : use_high_accuracy(false) {}
  explicit GeolocationObserverOptions(bool high_accuracy)
      : use_high_accuracy(high_accuracy) {}

  // Given a map<ANYTHING, GeolocationObserverOptions> this function will
  // iterate the map and collapse all the options found to a single instance
  // that satisfies them all.
  template <typename MAP>
  static GeolocationObserverOptions Collapse(const MAP& options_map) {
    for (typename MAP::const_iterator it = options_map.begin();
         it != options_map.end(); ++it) {
      if (it->second.use_high_accuracy)
        return GeolocationObserverOptions(true);
    }
    return GeolocationObserverOptions(false);
  }

  bool use_high_accuracy;
};

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_OBSERVER_H_
