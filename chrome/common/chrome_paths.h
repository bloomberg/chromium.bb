// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PATHS_H__
#define CHROME_COMMON_CHROME_PATHS_H__

#include "build/build_config.h"

// This file declares path keys for the chrome module.  These can be used with
// the PathService to access various special directories and files.

namespace chrome {

enum {
  PATH_START = 1000,

  DIR_APP = PATH_START,         // Directory where dlls and data reside.
  DIR_LOGS,                     // Directory where logs should be written.
  DIR_USER_DATA,                // Directory where user data can be written.
  DIR_CRASH_DUMPS,              // Directory where crash dumps are written.
  DIR_USER_DESKTOP,             // Directory that correspond to the desktop.
  DIR_RESOURCES,                // Directory containing separate file resources
                                // used by Chrome at runtime.
  DIR_BOOKMARK_MANAGER,         // Directory containing the bookmark manager.
  DIR_INSPECTOR,                // Directory where web inspector is located.
  DIR_NET_INTERNALS,            // Directory where net internals is located.
  DIR_APP_DICTIONARIES,         // Directory where the global dictionaries are.
  DIR_USER_DOCUMENTS,           // Directory for a user's "My Documents".
  DIR_DEFAULT_DOWNLOADS_SAFE,   // Directory for a user's
                                // "My Documents/Downloads".
  DIR_DEFAULT_DOWNLOADS,        // Directory for a user's downloads.
  FILE_RESOURCE_MODULE,         // Full path and filename of the module that
                                // contains embedded resources (version,
                                // strings, images, etc.).
  FILE_LOCAL_STATE,             // Path and filename to the file in which
                                // machine/installation-specific state is saved.
  FILE_RECORDED_SCRIPT,         // Full path to the script.log file that
                                // contains recorded browser events for
                                // playback.
  FILE_GEARS_PLUGIN,            // Full path to the gears.dll plugin file.
  FILE_FLASH_PLUGIN,            // Full path to the internal Flash plugin file.
  FILE_LIBAVCODEC,              // Full path to libavcodec media decoding
                                // library.
  FILE_LIBAVFORMAT,             // Full path to libavformat media parsing
                                // library.
  FILE_LIBAVUTIL,               // Full path to libavutil media utility library.
#if defined(OS_CHROMEOS)
  FILE_CHROMEOS_API,            // Full path to chrome os api shared object.
#endif


  // Valid only in development environment; TODO(darin): move these
  DIR_TEST_DATA,                // Directory where unit test data resides.
  DIR_TEST_TOOLS,               // Directory where unit test tools reside.

  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_PATHS_H__
