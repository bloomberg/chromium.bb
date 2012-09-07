// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#if defined(OS_WIN)
  DIR_ALT_USER_DATA,            // Directory of the desktop or metro user data
                                // (the one that isn't in use).
#endif
  DIR_CRASH_DUMPS,              // Directory where crash dumps are written.
  DIR_USER_DESKTOP,             // Directory that correspond to the desktop.
  DIR_RESOURCES,                // Directory containing separate file resources
                                // used by Chrome at runtime.
  DIR_INSPECTOR,                // Directory where web inspector is located.
  DIR_APP_DICTIONARIES,         // Directory where the global dictionaries are.
  DIR_USER_DOCUMENTS,           // Directory for a user's "My Documents".
  DIR_USER_PICTURES,            // Directory for a user's pictures.
  DIR_DEFAULT_DOWNLOADS_SAFE,   // Directory for a user's
                                // "My Documents/Downloads", (Windows) or
                                // "Downloads". (Linux)
  DIR_DEFAULT_DOWNLOADS,        // Directory for a user's downloads.
  DIR_USER_DATA_TEMP,           // A temp directory within DIR_USER_DATA.  Use
                                // this when a temporary file or directory will
                                // be moved into the profile, to avoid issues
                                // moving across volumes.  See crbug.com/13044 .
                                // Getting this path does not create it.  Users
                                // should check that the path exists before
                                // using it.
  DIR_INTERNAL_PLUGINS,         // Directory where internal plugins reside.
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  DIR_POLICY_FILES,             // Directory for system-wide read-only
                                // policy files that allow sys-admins
                                // to set policies for chrome. This directory
                                // contains subdirectories.
#endif
#if defined(OS_MACOSX)
  DIR_MANAGED_PREFS,            // Directory that stores the managed prefs plist
                                // files for the current user.
#endif
#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
  DIR_USER_EXTERNAL_EXTENSIONS,  // Directory for per-user external extensions
                                 // on Chrome Mac.  On Chrome OS, this path is
                                 // used for OEM customization.
                                 // Getting this path does not create it.
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  DIR_STANDALONE_EXTERNAL_EXTENSIONS,  // Directory for 'per-extension'
                                       // definition manifest files that
                                       // describe extensions which are to be
                                       // installed when chrome is run.
#endif
  DIR_EXTERNAL_EXTENSIONS,      // Directory where installer places .crx files.

  DIR_DEFAULT_APPS,             // Directory where installer places .crx files
                                // to be installed when chrome is first run.
  DIR_PEPPER_FLASH_PLUGIN,      // Directory to the Pepper Flash plugin,
                                // containing the plugin and the manifest.
  FILE_RESOURCE_MODULE,         // Full path and filename of the module that
                                // contains embedded resources (version,
                                // strings, images, etc.).
  FILE_LOCAL_STATE,             // Path and filename to the file in which
                                // machine/installation-specific state is saved.
  FILE_RECORDED_SCRIPT,         // Full path to the script.log file that
                                // contains recorded browser events for
                                // playback.
  FILE_FLASH_PLUGIN,            // Full path to the internal Flash plugin file.
                                // Querying this path will succeed no matter the
                                // file exists or not.
  FILE_FLASH_PLUGIN_EXISTING,   // Full path to the internal Flash plugin file.
                                // Querying this path will fail if the file
                                // doesn't exist.
  FILE_PEPPER_FLASH_PLUGIN,     // Full path to the Pepper Flash plugin file.
  FILE_PDF_PLUGIN,              // Full path to the internal PDF plugin file.

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  FILE_NACL_HELPER,             // Full path to Linux nacl_helper executable.
  FILE_NACL_HELPER_BOOTSTRAP,   // ... and nacl_helper_bootstrap executable.
#endif
  FILE_NACL_PLUGIN,             // Full path to the internal NaCl plugin file.
  DIR_PNACL_BASE,               // Full path to the base dir for PNaCl.
  DIR_PNACL_COMPONENT,          // Full path to the latest PNaCl version
                                // (subdir of DIR_PNACL_BASE).
  FILE_O3D_PLUGIN,              // Full path to the O3D Pepper plugin file.
  FILE_GTALK_PLUGIN,            // Full path to the GTalk Pepper plugin file.
  FILE_LIBAVCODEC,              // Full path to libavcodec media decoding
                                // library.
  FILE_LIBAVFORMAT,             // Full path to libavformat media parsing
                                // library.
  FILE_LIBAVUTIL,               // Full path to libavutil media utility library.
  FILE_RESOURCES_PACK,          // Full path to the .pak file containing
                                // binary data (e.g., html files and images
                                // used by interal pages).
  DIR_RESOURCES_EXTENSION,      // Full path to extension resources.
#if defined(OS_CHROMEOS)
  DIR_CHROMEOS_WALLPAPERS,      // Directory where downloaded chromeos
                                // wallpapers reside.
#endif

  // Valid only in development environment; TODO(darin): move these
  DIR_GEN_TEST_DATA,            // Directory where generated test data resides.
  DIR_TEST_DATA,                // Directory where unit test data resides.
  DIR_TEST_TOOLS,               // Directory where unit test tools reside.

  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_PATHS_H__
