// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPPS_FINDER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPPS_FINDER_H_

#include <string>

#include "base/callback.h"

namespace iapps {

typedef base::Callback<void(const std::string&)> IAppsFinderCallback;

// These methods look for the iTunes/iPhoto library in in an asynchronous
// manner and call |callback| on the UI thread as soon as the result is known.
// If a library exists, |callback| gets the device id for the library. If a
// library does not exist, or the OS does not support iTunes/iPhoto, then
// |callback| gets an empty string.

void FindIPhotoLibrary(const IAppsFinderCallback& callback);

void FindITunesLibrary(const IAppsFinderCallback& callback);

// These methods returns true if |path| should be interpreted as the device
// indicated by |device_id|.

bool PathIndicatesIPhotoLibrary(const std::string& device_id,
                                const base::FilePath& path);

bool PathIndicatesITunesLibrary(const std::string& device_id,
                                const base::FilePath& path);

}  // namespace iapps

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPPS_FINDER_H_
