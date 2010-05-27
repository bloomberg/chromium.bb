// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/libgps_wrapper_linux.h"

#include <dlfcn.h>

#include "chrome/common/geoposition.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace {
LibGpsLibraryLoader* TryToOpen(const char* lib,
                               LibGpsLibraryLoader::InitMode mode) {
  void* dl_handle = dlopen(lib, RTLD_LAZY);
  if (dl_handle) {
    LOG(INFO) << "Loaded " << lib;
    scoped_ptr<LibGpsLibraryLoader> wrapper(new LibGpsLibraryLoader(dl_handle));
    if (wrapper->Init(mode))
      return wrapper.release();
  } else {
    DLOG(INFO) << "Could not open " << lib << ": " << dlerror();
  }
  return NULL;
}
} // namespace

LibGps::LibGps(LibGpsLibraryLoader* dl_wrapper)
    : library_(dl_wrapper) {
  DCHECK(dl_wrapper != NULL);
}

LibGps::~LibGps() {
}

LibGps* LibGps::New() {
  LibGpsLibraryLoader* wrapper;
  wrapper = TryToOpen("libgps.so.19", LibGpsLibraryLoader::INITMODE_STREAM);
  if (wrapper)
    return NewV294(wrapper);
  wrapper = TryToOpen("libgps.so.17", LibGpsLibraryLoader::INITMODE_QUERY);
  if (wrapper)
    return NewV238(wrapper);
  wrapper = TryToOpen("libgps.so", LibGpsLibraryLoader::INITMODE_STREAM);
  if (wrapper)
    return NewV294(wrapper);
  return NULL;
}

bool LibGps::Start(std::string* error_out) {
  if (!library().open(NULL, NULL, error_out))
    return false;
  if (!StartStreaming()) {
    library().close();
    return false;
  }
  return true;
}

void LibGps::Stop() {
  library().close();
}

bool LibGps::Poll() {
  bool ret = true;
  while (DataWaiting(&ret)) {
    int error = library().poll();
    if (error) {
      last_error_ = StringPrintf("poll() returned %d", error);
      return false;
    }
  }
  if (!ret)
    last_error_ = "DataWaiting failed";
  return ret;
}

bool LibGps::GetPosition(Geoposition* position) {
  DCHECK(position);
  position->error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  if (!library().is_open()) {
    position->error_message = L"No gpsd connection";
    return false;
  }
  if (!GetPositionIfFixed(position)) {
    position->error_message = ASCIIToWide(last_error_);
    return false;
  }
  position->error_code = Geoposition::ERROR_CODE_NONE;
  if (!position->IsValidFix()) {
    // GetPositionIfFixed returned true, yet we've not got a valid fix.
    // This shouldn't happen; something went wrong in the conversion.
    NOTREACHED() << "Invalid position from GetPositionIfFixed: lat,long "
                 << position->latitude << "," << position->longitude
                 << " accuracy " << position->accuracy << " time "
                 << position->timestamp.ToDoubleT();
    position->error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
    position->error_message = L"Bad fix from gps";
    return false;
  }
  return true;
}

LibGpsLibraryLoader::LibGpsLibraryLoader(void* dl_handle)
    : dl_handle_(dl_handle),
      gps_open_(NULL),
      gps_close_(NULL),
      gps_poll_(NULL),
      gps_query_(NULL),
      gps_stream_(NULL),
      gps_waiting_(NULL),
      gps_data_(NULL) {
  DCHECK(dl_handle_);
}

// |mode| could be turned into a bit mask if required, but for now the
// two options are mutually exclusive.
bool LibGpsLibraryLoader::Init(InitMode mode) {
  DCHECK(dl_handle_);
  DCHECK(!gps_open_) << "Already initialized";
  #define SET_FN_POINTER(function, required)                            \
    function##_ = (function##_fn)dlsym(dl_handle_, #function);          \
    if ((required) && !function##_) {                                   \
      LOG(INFO) << "libgps " << #function << " error: " << dlerror();   \
      close();                                                          \
      return false;                                                     \
    }
  SET_FN_POINTER(gps_open, true);
  SET_FN_POINTER(gps_close, true);
  SET_FN_POINTER(gps_poll, true);
  SET_FN_POINTER(gps_query, mode == INITMODE_QUERY);
  SET_FN_POINTER(gps_stream, mode == INITMODE_STREAM);
  SET_FN_POINTER(gps_waiting, mode == INITMODE_STREAM);
  #undef SET_FN_POINTER
  return true;
}

LibGpsLibraryLoader::~LibGpsLibraryLoader() {
  close();
  if (dlclose(dl_handle_) != 0) {
    NOTREACHED() << "Error closing dl handle";
  }
}

bool LibGpsLibraryLoader::open(const char* host, const char* port,
                               std::string* error_out) {
  DCHECK(!gps_data_) << "libgps handle already opened";
  DCHECK(gps_open_) << "Must call Init() first";
  std::string dummy;
  if (!error_out) error_out = &dummy;

  gps_data_ = gps_open_(host, port);
  if (!gps_data_) {  // GPS session was not created.
    *error_out = "gps_open failed";
    close();
  }
  return gps_data_;
}

void LibGpsLibraryLoader::close() {
  if (gps_data_) {
    DCHECK(gps_close_);
    gps_close_(gps_data_);
    gps_data_ = NULL;
  }
}

int LibGpsLibraryLoader::poll() {
  DCHECK(is_open());
  DCHECK(gps_poll_);
  return gps_poll_(gps_data_);
}

// This is intentionally ignoring the va_arg extension to query(): the caller
// can use base::StringPrintf to achieve the same effect.
int LibGpsLibraryLoader::query(const char* fmt) {
  DCHECK(is_open());
  DCHECK(gps_query_);
  return gps_query_(gps_data_, fmt);
}

int LibGpsLibraryLoader::stream(int flags) {
  DCHECK(is_open());
  DCHECK(gps_stream_);
  return gps_stream_(gps_data_, flags, NULL);
}

bool LibGpsLibraryLoader::waiting() {
  DCHECK(is_open());
  DCHECK(gps_waiting_);
  return gps_waiting_(gps_data_);
}

const gps_data_t& LibGpsLibraryLoader::data() const {
  DCHECK(is_open());
  return *gps_data_;
}

bool LibGpsLibraryLoader::is_open() const {
  return gps_data_ != NULL;
}
