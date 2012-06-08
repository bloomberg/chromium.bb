// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/libgps_wrapper_linux.h"

#include <math.h>
#include <dlfcn.h>
#include <errno.h>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "content/public/common/geoposition.h"
#include "third_party/gpsd/release-3.1/gps.h"

COMPILE_ASSERT(GPSD_API_MAJOR_VERSION == 5, GPSD_API_version_is_not_5);

namespace {
const char kLibGpsName[] = "libgps.so.20";
}  // namespace

LibGps::LibGps(void* dl_handle,
               gps_open_fn gps_open,
               gps_close_fn gps_close,
               gps_read_fn gps_read)
    : dl_handle_(dl_handle),
      gps_open_(gps_open),
      gps_close_(gps_close),
      gps_read_(gps_read),
      gps_data_(new gps_data_t),
      is_open_(false) {
  DCHECK(gps_open_);
  DCHECK(gps_close_);
  DCHECK(gps_read_);
}

LibGps::~LibGps() {
  Stop();
  if (dl_handle_) {
    const int err = dlclose(dl_handle_);
    DCHECK_EQ(0, err) << "Error closing dl handle: " << err;
  }
}

LibGps* LibGps::New() {
  void* dl_handle = dlopen(kLibGpsName, RTLD_LAZY);
  if (!dl_handle) {
    DLOG(WARNING) << "Could not open " << kLibGpsName << ": " << dlerror();
    return NULL;
  }

  DLOG(INFO) << "Loaded " << kLibGpsName;

  #define DECLARE_FN_POINTER(function)                                   \
    function##_fn function = reinterpret_cast<function##_fn>(            \
        dlsym(dl_handle, #function));                                    \
    if (!function) {                                                     \
      DLOG(WARNING) << "libgps " << #function << " error: " << dlerror(); \
      dlclose(dl_handle);                                                \
      return NULL;                                                       \
    }
  DECLARE_FN_POINTER(gps_open);
  DECLARE_FN_POINTER(gps_close);
  DECLARE_FN_POINTER(gps_read);
  // We don't use gps_shm_read() directly, just to make sure that libgps has
  // the shared memory support.
  typedef int (*gps_shm_read_fn)(struct gps_data_t*);
  DECLARE_FN_POINTER(gps_shm_read);
  #undef DECLARE_FN_POINTER

  return new LibGps(dl_handle, gps_open, gps_close, gps_read);
}

bool LibGps::Start() {
  if (is_open_)
    return true;

#if defined(OS_CHROMEOS)
  errno = 0;
  if (gps_open_(GPSD_SHARED_MEMORY, 0, gps_data_.get()) != 0) {
    // See gps.h NL_NOxxx for definition of gps_open() error numbers.
    DLOG(WARNING) << "gps_open() failed " << errno;
    return false;
  } else {
    is_open_ = true;
    return true;
  }
#else  // drop the support for desktop linux for now
  DLOG(WARNING) << "LibGps is only supported on ChromeOS";
  return false;
#endif
}
void LibGps::Stop() {
  if (is_open_)
    gps_close_(gps_data_.get());
  is_open_ = false;
}

bool LibGps::Read(content::Geoposition* position) {
  DCHECK(position);
  position->error_code = content::Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  if (!is_open_) {
      DLOG(WARNING) << "No gpsd connection";
      position->error_message = "No gpsd connection";
      return false;
  }

  if (gps_read_(gps_data_.get()) < 0) {
      DLOG(WARNING) << "gps_read() fails";
      position->error_message = "gps_read() fails";
      return false;
  }

  if (!GetPositionIfFixed(position)) {
      DLOG(WARNING) << "No fixed position";
      position->error_message = "No fixed position";
      return false;
  }

  position->error_code = content::Geoposition::ERROR_CODE_NONE;
  position->timestamp = base::Time::Now();
  if (!position->Validate()) {
    // GetPositionIfFixed returned true, yet we've not got a valid fix.
    // This shouldn't happen; something went wrong in the conversion.
    NOTREACHED() << "Invalid position from GetPositionIfFixed: lat,long "
                 << position->latitude << "," << position->longitude
                 << " accuracy " << position->accuracy << " time "
                 << position->timestamp.ToDoubleT();
    position->error_code =
        content::Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
    position->error_message = "Bad fix from gps";
    return false;
  }
  return true;
}

bool LibGps::GetPositionIfFixed(content::Geoposition* position) {
  DCHECK(position);
  if (gps_data_->status == STATUS_NO_FIX) {
    DVLOG(2) << "Status_NO_FIX";
    return false;
  }

  if (isnan(gps_data_->fix.latitude) || isnan(gps_data_->fix.longitude)) {
    DVLOG(2) << "No valid lat/lon value";
    return false;
  }

  position->latitude = gps_data_->fix.latitude;
  position->longitude = gps_data_->fix.longitude;

  if (!isnan(gps_data_->fix.epx) && !isnan(gps_data_->fix.epy)) {
    position->accuracy = std::max(gps_data_->fix.epx, gps_data_->fix.epy);
  } else if (isnan(gps_data_->fix.epx) && !isnan(gps_data_->fix.epy)) {
    position->accuracy = gps_data_->fix.epy;
  } else if (!isnan(gps_data_->fix.epx) && isnan(gps_data_->fix.epy)) {
    position->accuracy = gps_data_->fix.epx;
  } else {
    // TODO(joth): Fixme. This is a workaround for http://crbug.com/99326
    DVLOG(2) << "libgps reported accuracy NaN, forcing to zero";
    position->accuracy = 0;
  }

  if (gps_data_->fix.mode == MODE_3D && !isnan(gps_data_->fix.altitude)) {
    position->altitude = gps_data_->fix.altitude;
    if (!isnan(gps_data_->fix.epv))
      position->altitude_accuracy = gps_data_->fix.epv;
  }

  if (!isnan(gps_data_->fix.track))
    position->heading = gps_data_->fix.track;
  if (!isnan(gps_data_->fix.speed))
    position->speed = gps_data_->fix.speed;
  return true;
}
