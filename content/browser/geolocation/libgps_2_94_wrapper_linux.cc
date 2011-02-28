// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a variant of libgps wrapper for use with the 2.94 release of the API.

#include "content/browser/geolocation/libgps_wrapper_linux.h"

#include "base/logging.h"
#include "chrome/common/geoposition.h"
#include "third_party/gpsd/release-2.94/gps.h"

class LibGpsV294 : public LibGps {
 public:
  explicit LibGpsV294(LibGpsLibraryWrapper* dl_wrapper) : LibGps(dl_wrapper) {}

  // LibGps
  virtual bool StartStreaming() {
    return library().stream(WATCH_ENABLE) == 0;
  }
  virtual bool DataWaiting() {
    return library().waiting();
  }
  virtual bool GetPositionIfFixed(Geoposition* position) {
    // This function is duplicated between the library versions, however it
    // cannot be shared as each one must be strictly compiled against the
    // corresponding version of gps.h.
    DCHECK(position);
    const gps_data_t& gps_data = library().data();
    if (gps_data.status == STATUS_NO_FIX)
      return false;
    position->latitude = gps_data.fix.latitude;
    position->longitude = gps_data.fix.longitude;
    position->accuracy = std::max(gps_data.fix.epx, gps_data.fix.epy);
    position->altitude = gps_data.fix.altitude;
    position->altitude_accuracy = gps_data.fix.epv;
    position->heading = gps_data.fix.track;
    position->speed = gps_data.fix.speed;
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LibGpsV294);
};

LibGps* LibGps::NewV294(LibGpsLibraryWrapper* dl_wrapper) {
  return new LibGpsV294(dl_wrapper);
}
