// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/iphoto_finder_mac.h"

#import "base/mac/foundation_util.h"
#include "chrome/browser/storage_monitor/storage_info.h"

namespace iphoto {

IPhotoFinder::IPhotoFinder(const IPhotoFinderCallback& callback)
    : IAppFinderMac(StorageInfo::IPHOTO, CFSTR("iPhotoRecentDatabases"),
                    callback) {
}

IPhotoFinder::~IPhotoFinder() {}

base::FilePath IPhotoFinder::ExtractPath(NSString* path_ns) const {
  NSURL* url = [NSURL URLWithString:path_ns];
  if (![url isFileURL])
    return base::FilePath();

  NSString* expanded_path_ns = [url path];
  return base::mac::NSStringToFilePath(expanded_path_ns);
}

}  // namespace iphoto
