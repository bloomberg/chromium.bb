// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_FINDER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_FINDER_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"

#if defined(OS_MACOSX)

class MacPreferences;
#if defined(__OBJC__)
@class NSString;
#else  // __OBJC__
class NSString;
#endif  // __OBJC__

#endif  // OS_MACOSX

namespace picasa {

#if defined(OS_WIN)
extern const wchar_t kPicasaRegistryPath[];
extern const wchar_t kPicasaRegistryAppDataPathKey[];
#endif

#if defined(OS_MACOSX)
extern NSString* const kPicasaAppDataPathMacPreferencesKey;
#endif

typedef base::Callback<void(const std::string&)> DeviceIDCallback;

// Bounces to FILE thread to find Picasa database. If the platform supports
// Picasa and a Picasa database is found, |callback| will be invoked on the
// calling thread with the device ID. Otherwise, |callback| will be invoked
// with an empty string.
void FindPicasaDatabase(const DeviceIDCallback& callback);

// Builds the OS-dependent Picasa database path from the app-data path.
// Used internally and by tests to construct an test environments.
base::FilePath MakePicasaDatabasePath(
    const base::FilePath& picasa_app_data_path);

#if defined(OS_MACOSX)
// Set the mac preferences to use for testing. The caller continues to own
// |preferences| and should call this function again with NULL before freeing
// it.
void SetMacPreferencesForTesting(MacPreferences* preferences);

// Used internally only.
base::FilePath GetCustomPicasaAppDataPathFromMacPreferences();
#endif  // OS_MACOSX

}  // namespace picasa

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_FINDER_H_
