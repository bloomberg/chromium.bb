// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPPS_FINDER_IMPL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPPS_FINDER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/media_galleries/fileapi/iapps_finder.h"
#include "components/storage_monitor/storage_info.h"

#if defined(OS_MACOSX)

class MacPreferences;
#if defined(__OBJC__)
@class NSArray;
@class NSString;
#else  // __OBJC__
class NSArray;
class NSString;
#endif  // __OBJC__

#endif  // OS_MACOSX

namespace iapps {

#if defined(OS_MACOSX)

extern NSString* const kIPhotoRecentDatabasesKey;
extern NSString* const kITunesRecentDatabasePathsKey;

// Set the mac preferences to use for testing. The caller continues to own
// |preferences| and should call this function again with NULL before freeing
// it.
void SetMacPreferencesForTesting(MacPreferences* preferences);

// Returns an NSArray with a single string entry which is a path value.
NSArray* NSArrayFromFilePath(const base::FilePath& path);

#endif  // OS_MACOSX

typedef base::Callback<void(const IAppsFinderCallback&)> IAppsFinderTask;

// FindIAppsOnFileThread helps iApps finders by taking care of the details of
// bouncing to the FILE thread to run |task| and then turning the result into a
// device id and posting it to |callback| on the UI thread.  If |task| does not
// find the iApps's library, |callback| gets an empty string.
void FindIAppsOnFileThread(storage_monitor::StorageInfo::Type type,
                           const IAppsFinderTask& task,
                           const IAppsFinderCallback& callback);

}  // namespace iapps

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPPS_FINDER_IMPL_H_
