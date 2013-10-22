// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/iapps_finder_impl.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/time/time.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "content/public/browser/browser_thread.h"

using base::mac::CFCast;
using base::mac::CFToNSCast;
using base::mac::NSToCFCast;

namespace iapps {

namespace {

typedef base::Callback<base::FilePath(NSString*)> PListPathExtractor;

void FindMostRecentDatabase(
    base::scoped_nsobject<NSString> recent_databases_key,
    const PListPathExtractor& path_extractor,
    const IAppsFinderCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  CFStringRef iapp_id = CFSTR("com.apple.iApps");
  base::scoped_nsobject<NSArray> plist(CFToNSCast(CFCast<CFArrayRef>(
      CFPreferencesCopyAppValue(NSToCFCast(recent_databases_key.get()),
                                iapp_id))));
  if (!plist) {
    callback.Run(std::string());
    return;
  }

  // Find the most recently used database from the list of database paths. Most
  // of the time |plist| has a size of 1.
  base::Time most_recent_db_time;
  base::FilePath most_recent_db_path;
  for (NSString* path_ns in plist.get()) {
    base::FilePath db_path = path_extractor.Run(path_ns);
    if (db_path.empty())
      continue;

    base::PlatformFileInfo file_info;
    if (!file_util::GetFileInfo(db_path, &file_info))
      continue;

    // In case of two databases with the same modified time, tie breaker goes
    // to the first one on the list.
    if (file_info.last_modified <= most_recent_db_time)
      continue;

    most_recent_db_time = file_info.last_modified;
    most_recent_db_path = db_path;
  }
  callback.Run(most_recent_db_path.value());
}

base::FilePath ExtractIPhotoPath(NSString* path_ns) {
  NSURL* url = [NSURL URLWithString:path_ns];
  if (![url isFileURL])
    return base::FilePath();

  NSString* expanded_path_ns = [url path];
  return base::mac::NSStringToFilePath(expanded_path_ns);
}

base::FilePath ExtractITunesPath(NSString* path_ns) {
  NSString* expanded_path_ns = [path_ns stringByExpandingTildeInPath];
  return base::mac::NSStringToFilePath(expanded_path_ns);
};

}  // namespace

NSString* const kIPhotoRecentDatabasesKey = @"iPhotoRecentDatabases";
NSString* const kITunesRecentDatabasePathsKey = @"iTunesRecentDatabasePaths";

void TestFunc(
    const PListPathExtractor& path_extractor,
    const IAppsFinderCallback& callback) {
}
void TestFunc2(
    const IAppsFinderCallback& callback) {
}

void FindIPhotoLibrary(const IAppsFinderCallback& callback) {
  FindIAppsOnFileThread(
      StorageInfo::IPHOTO,
      base::Bind(&FindMostRecentDatabase,
                 base::scoped_nsobject<NSString>(kIPhotoRecentDatabasesKey),
                 base::Bind(&ExtractIPhotoPath)),
      callback);
}

void FindITunesLibrary(const IAppsFinderCallback& callback) {
  FindIAppsOnFileThread(
      StorageInfo::ITUNES,
      base::Bind(&FindMostRecentDatabase,
                 base::scoped_nsobject<NSString>(kITunesRecentDatabasePathsKey),
                 base::Bind(&ExtractITunesPath)),
      callback);
}

}  // namespace iapps
