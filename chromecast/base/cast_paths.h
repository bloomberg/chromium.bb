// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CAST_PATHS_H_
#define CHROMECAST_BASE_CAST_PATHS_H_

#include "build/build_config.h"

// This file declares path keys for the chromecast module.  These can be used
// with the PathService to access various special directories and files.

namespace chromecast {

enum {
  PATH_START = 8000,

  DIR_CAST_HOME,    // Return a modified $HOME which works for both
                    // development use and the actual device.

#if defined(OS_ANDROID)
  FILE_CAST_ANDROID_LOG, // Log file location for Android.
#endif  // defined(OS_ANDROID)
  FILE_CAST_CONFIG, // Config/preferences file path.
  FILE_CAST_CRL,    // CRL persistent cache file path.
  FILE_CAST_PAK,    // cast_shell.pak file path.
  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace chromecast

#endif  // CHROMECAST_BASE_CAST_PATHS_H_
