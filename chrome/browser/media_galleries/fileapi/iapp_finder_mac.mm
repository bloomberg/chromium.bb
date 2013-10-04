// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/iapp_finder_mac.h"

#include "base/file_util.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"

using base::mac::CFCast;
using base::mac::CFToNSCast;

namespace iapps {

IAppFinderMac::~IAppFinderMac() {}

IAppFinderMac::IAppFinderMac(StorageInfo::Type type,
                             CFStringRef recent_databases_key,
                             const IAppFinderCallback& callback)
    : IAppFinder(type, callback),
      recent_databases_key_(recent_databases_key) {
}

void IAppFinderMac::FindIAppOnFileThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  CFStringRef iapp_id = CFSTR("com.apple.iApps");
  base::scoped_nsobject<NSArray> plist(CFToNSCast(
      CFCast<CFArrayRef>(CFPreferencesCopyAppValue(recent_databases_key_,
                                                   iapp_id))));
  if (!plist) {
    PostResultToUIThread(std::string());
    return;
  }

  // Find the most recently used database from the list of database paths. Most
  // of the time |plist| has a size of 1.
  base::Time most_recent_db_time;
  base::FilePath most_recent_db_path;
  for (NSString* path_ns in plist.get()) {
    base::FilePath db_path = ExtractPath(path_ns);
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
  PostResultToUIThread(most_recent_db_path.value());
}

}  // namespace iapps
