// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/libgps_wrapper_linux.h"

#include <dlfcn.h>
#include <errno.h>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "content/common/geoposition.h"

namespace {
// Attempts to load dynamic library named |lib| and initialize the required
// function pointers according to |mode|. Returns ownership a new instance
// of the library loader class, or NULL on failure.
// TODO(joth): This is a hang-over from when we dynamically two different
// versions of libgps and chose between them. Now we could remove at least one
// layer of wrapper class and directly #include gps.h in this cc file.
// See http://crbug.com/98132 and http://crbug.com/99177
LibGpsLibraryWrapper* TryToOpen(const char* lib) {
  void* dl_handle = dlopen(lib, RTLD_LAZY);
  if (!dl_handle) {
    VLOG(1) << "Could not open " << lib << ": " << dlerror();
    return NULL;
  }
  VLOG(1) << "Loaded " << lib;

  #define DECLARE_FN_POINTER(function)                                   \
    LibGpsLibraryWrapper::function##_fn function;                        \
    function = reinterpret_cast<LibGpsLibraryWrapper::function##_fn>(    \
        dlsym(dl_handle, #function));                                    \
    if (!function) {                                                     \
      LOG(WARNING) << "libgps " << #function << " error: " << dlerror(); \
      dlclose(dl_handle);                                                \
      return NULL;                                                       \
    }
  DECLARE_FN_POINTER(gps_open);
  DECLARE_FN_POINTER(gps_close);
  DECLARE_FN_POINTER(gps_poll);
  DECLARE_FN_POINTER(gps_stream);
  DECLARE_FN_POINTER(gps_waiting);
  #undef DECLARE_FN_POINTER

  return new LibGpsLibraryWrapper(dl_handle,
                                  gps_open,
                                  gps_close,
                                  gps_poll,
                                  gps_stream,
                                  gps_waiting);
}
} // namespace

LibGps::LibGps(LibGpsLibraryWrapper* dl_wrapper)
    : library_(dl_wrapper) {
  DCHECK(dl_wrapper != NULL);
}

LibGps::~LibGps() {
}

LibGps* LibGps::New() {
  LibGpsLibraryWrapper* wrapper;
  wrapper = TryToOpen("libgps.so.19");
  if (wrapper)
    return NewV294(wrapper);
  wrapper = TryToOpen("libgps.so");
  if (wrapper)
    return NewV294(wrapper);
  return NULL;
}

bool LibGps::Start() {
  if (library().is_open())
    return true;
  errno = 0;
  static int fail_count = 0;
  if (!library().open(NULL, NULL)) {
    // See gps.h NL_NOxxx for definition of gps_open() error numbers.
    LOG_IF(WARNING, 0 == fail_count++) << "gps_open() failed: " << errno;
    return false;
  }
  fail_count = 0;
  if (!StartStreaming()) {
    VLOG(1) << "StartStreaming failed";
    library().close();
    return false;
  }
  return true;
}

void LibGps::Stop() {
  library().close();
}

bool LibGps::Poll() {
  last_error_ = "no data received from gpsd";
  while (DataWaiting()) {
    int error = library().poll();
    if (error) {
      last_error_ = base::StringPrintf("poll() returned %d", error);
      Stop();
      return false;
    }
    last_error_.clear();
  }
  return last_error_.empty();
}

bool LibGps::GetPosition(Geoposition* position) {
  DCHECK(position);
  position->error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  if (!library().is_open()) {
    position->error_message = "No gpsd connection";
    return false;
  }
  if (!GetPositionIfFixed(position)) {
    position->error_message = last_error_;
    return false;
  }
  position->error_code = Geoposition::ERROR_CODE_NONE;
  position->timestamp = base::Time::Now();
  if (!position->IsValidFix()) {
    // GetPositionIfFixed returned true, yet we've not got a valid fix.
    // This shouldn't happen; something went wrong in the conversion.
    NOTREACHED() << "Invalid position from GetPositionIfFixed: lat,long "
                 << position->latitude << "," << position->longitude
                 << " accuracy " << position->accuracy << " time "
                 << position->timestamp.ToDoubleT();
    position->error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
    position->error_message = "Bad fix from gps";
    return false;
  }
  return true;
}

LibGpsLibraryWrapper::LibGpsLibraryWrapper(void* dl_handle,
                                           gps_open_fn gps_open,
                                           gps_close_fn gps_close,
                                           gps_poll_fn gps_poll,
                                           gps_stream_fn gps_stream,
                                           gps_waiting_fn gps_waiting)
    : dl_handle_(dl_handle),
      gps_open_(gps_open),
      gps_close_(gps_close),
      gps_poll_(gps_poll),
      gps_stream_(gps_stream),
      gps_waiting_(gps_waiting),
      gps_data_(NULL) {
}

LibGpsLibraryWrapper::~LibGpsLibraryWrapper() {
  close();
  if (dl_handle_) {
    const int err = dlclose(dl_handle_);
    CHECK_EQ(0, err) << "Error closing dl handle: " << err;
  }
}

bool LibGpsLibraryWrapper::open(const char* host, const char* port) {
  DCHECK(!gps_data_) << "libgps already opened";
  DCHECK(gps_open_);
  gps_data_ = gps_open_(host, port);
  return is_open();
}

void LibGpsLibraryWrapper::close() {
  if (is_open()) {
    DCHECK(gps_close_);
    gps_close_(gps_data_);
    gps_data_ = NULL;
  }
}

int LibGpsLibraryWrapper::poll() {
  DCHECK(is_open());
  DCHECK(gps_poll_);
  return gps_poll_(gps_data_);
}

int LibGpsLibraryWrapper::stream(int flags) {
  DCHECK(is_open());
  DCHECK(gps_stream_);
  return gps_stream_(gps_data_, flags, NULL);
}

bool LibGpsLibraryWrapper::waiting() {
  DCHECK(is_open());
  DCHECK(gps_waiting_);
  return gps_waiting_(gps_data_);
}

const gps_data_t& LibGpsLibraryWrapper::data() const {
  DCHECK(is_open());
  return *gps_data_;
}

bool LibGpsLibraryWrapper::is_open() const {
  return gps_data_ != NULL;
}
