// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a wrapper around the C libgps API (gps.h). Similar to the libgpsmm.h
// API provided by that package.

#ifndef CONTENT_BROWSER_GEOLOCATION_LIBGPS_WRAPPER_LINUX_H_
#define CONTENT_BROWSER_GEOLOCATION_LIBGPS_WRAPPER_LINUX_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

struct gps_data_t;

namespace content {
struct Geoposition;
}

class CONTENT_EXPORT LibGps {
 public:
  virtual ~LibGps();
  // Attempts to dynamically load the libgps.so library and returns NULL on
  // failure.
  static LibGps* New();

  bool Start();
  void Stop();
  bool Read(content::Geoposition* position);

 protected:
  typedef int (*gps_open_fn)(const char*, const char*, struct gps_data_t*);
  typedef int (*gps_close_fn)(struct gps_data_t*);
  typedef int (*gps_read_fn)(struct gps_data_t*);

  explicit LibGps(void* dl_handle,
                  gps_open_fn gps_open,
                  gps_close_fn gps_close,
                  gps_read_fn gps_read);

  // Returns false if there is not fix available.
  virtual bool GetPositionIfFixed(content::Geoposition* position);

 private:
  void* dl_handle_;
  gps_open_fn gps_open_;
  gps_close_fn gps_close_;
  gps_read_fn gps_read_;

  scoped_ptr<gps_data_t> gps_data_;
  bool is_open_;

  DISALLOW_COPY_AND_ASSIGN(LibGps);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_LIBGPS_WRAPPER_LINUX_H_
