// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_CAST_MEDIA_SHLIB_H_
#define CHROMECAST_PUBLIC_CAST_MEDIA_SHLIB_H_

#include <string>
#include <vector>

#include "chromecast_export.h"

namespace chromecast {
namespace media {

class CHROMECAST_EXPORT CastMediaShlib {
 public:
  // Performs platform-specific one-time initialization for media systems and
  // hardware resources. Called at startup in browser process before main
  // message loop begins.
  static void Initialize(const std::vector<std::string>& argv);

  // Performs platform-specific one-time teardown of media systems and hardware
  // resources. Called at browser process exit.
  static void Finalize();
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_CAST_MEDIA_SHLIB_H_
