// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a wrapper around the C libgps API (gps.h). Similar to the libgpsmm.h
// API provided by that package, but adds:
// - shared object dynamic loading
// - support for (and abstraction from) different libgps.so versions
// - configurable for testing
// - more convenient error handling.

#ifndef CHROME_BROWSER_GEOLOCATION_LIBGPS_WRAPPER_LINUX_H_
#define CHROME_BROWSER_GEOLOCATION_LIBGPS_WRAPPER_LINUX_H_

#include <string>

#include "base/scoped_ptr.h"
#include "base/time.h"

struct Geoposition;
class LibGpsLibraryLoader;

class LibGps {
 public:
  virtual ~LibGps();
  // Attempts to dynamically load the libgps.so library, and creates and
  // appropriate LibGps instance for the version loaded. Returns NULL on
  // failure.
  static LibGps* New();

  bool Start();
  void Stop();
  bool Poll();
  bool GetPosition(Geoposition* position);

 protected:
  // Takes ownership of |dl_wrapper|.
  explicit LibGps(LibGpsLibraryLoader* dl_wrapper);

  LibGpsLibraryLoader& library() {
    return *library_;
  }
  // Called be start Start after successful |gps_open| to setup streaming.
  virtual bool StartStreaming() = 0;
  virtual bool DataWaiting(bool* ok) = 0;
  // Returns false if there is not fix available.
  virtual bool GetPositionIfFixed(Geoposition* position) = 0;

 private:
  // Factory functions to create instances of LibGps using the corresponding
  // libgps API versions (v2.38 => libgps.so.17, v2.94 => libgps.so.19).
  // See LibGps::New() for the public API to this.
  // Takes ownership of |dl_wrapper|.
  static LibGps* NewV238(LibGpsLibraryLoader* dl_wrapper);
  static LibGps* NewV294(LibGpsLibraryLoader* dl_wrapper);

  scoped_ptr<LibGpsLibraryLoader> library_;
  std::string last_error_;

  DISALLOW_COPY_AND_ASSIGN(LibGps);
};

struct gps_data_t;

// Wraps the low-level shared object, binding C++ member functions onto the
// underlying C functions obtained from the library.
class LibGpsLibraryLoader {
 public:
  // Pass to Init() to indicate which functions should be wired up.
  enum InitMode {
    INITMODE_QUERY,
    INITMODE_STREAM,
  };

  // Takes ownership of the handle passed in (will be released via dlclose).
  explicit LibGpsLibraryLoader(void* dl_handle);
  ~LibGpsLibraryLoader();
  // Must be successfully called before using any of the API methods below.
  // Returns false on error.
  bool Init(InitMode requirement);

  // Analogs of gps_xxx methods in gps.h
  bool open(const char* host, const char* port);
  void close();
  int poll();
  int query(const char* fmt);
  int stream(int flags);
  bool waiting();
  const gps_data_t& data() const;
  bool is_open() const;

 private:
  typedef gps_data_t* (*gps_open_fn)(const char*, const char*);
  typedef int (*gps_close_fn)(gps_data_t*);
  typedef int (*gps_poll_fn)(gps_data_t*);
  // v2.34 only
  typedef int (*gps_query_fn)(struct gps_data_t *gpsdata, const char* fmt, ...);
  // v2.90+
  typedef int (*gps_stream_fn)(gps_data_t*, unsigned int, void*);
  typedef bool (*gps_waiting_fn)(gps_data_t*);

  void* dl_handle_;
  gps_open_fn gps_open_;
  gps_close_fn gps_close_;
  gps_poll_fn gps_poll_;
  gps_query_fn gps_query_;
  gps_stream_fn gps_stream_;
  gps_waiting_fn gps_waiting_;

  gps_data_t* gps_data_;

  DISALLOW_COPY_AND_ASSIGN(LibGpsLibraryLoader);
};

#endif  // CHROME_BROWSER_GEOLOCATION_LIBGPS_WRAPPER_LINUX_H_
