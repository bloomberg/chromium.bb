// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a variant of libgps wrapper for use with the 2.38 release of the API.

#include "chrome/browser/geolocation/libgps_wrapper_linux.h"

// This lot needed for Poll()
#include <sys/socket.h>
#include <sys/time.h>

#include "base/logging.h"
#include "chrome/common/geoposition.h"
#include "third_party/gpsd/release-2.38/gps.h"

class LibGpsV238 : public LibGps {
 public:
  explicit LibGpsV238(LibGpsLibraryLoader* dl_wrapper) : LibGps(dl_wrapper) {}

  // LibGps
  virtual bool StartStreaming() {
    return library().query("w+x\n") == 0;
  }
  virtual bool DataWaiting(bool* ok) {
    // Unfortunately the 2.38 API has no public method for non-blocking test
    // for new data arrived, so we must contrive our own by doing a select() on
    // the underlying struct's socket fd.
    DCHECK(ok);
    *ok = true;
    fd_set fds;
    struct timeval timeout = {0};
    int fd = library().data().gps_fd;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    int ret = select(fd + 1, &fds, NULL, NULL, &timeout);
    if (ret == -1) {
      LOG(INFO) << "libgps socket select failed: " << ret;
      *ok = false;
      return false;
    }
    DCHECK_EQ(!!ret, !!FD_ISSET(fd, &fds));
    return ret;
  }
  virtual bool GetPositionIfFixed(Geoposition* position) {
    // This function is duplicated between the library versions, however it
    // cannot be shared as each one must be strictly compiled against the
    // corresponding version of gps.h.
    DCHECK(position);
    const gps_data_t& gps_data = library().data();
    if (gps_data.status == STATUS_NO_FIX)
      return false;
    position->timestamp = base::Time::FromDoubleT(gps_data.fix.time);
    position->latitude = gps_data.fix.latitude;
    position->longitude = gps_data.fix.longitude;
    position->accuracy = library().data().fix.eph;
    position->altitude = gps_data.fix.altitude;
    position->altitude_accuracy = gps_data.fix.epv;
    position->heading = gps_data.fix.track;
    position->speed = gps_data.fix.speed;
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LibGpsV238);
};

LibGps* LibGps::NewV238(LibGpsLibraryLoader* dl_wrapper) {
  return new LibGpsV238(dl_wrapper);
}
