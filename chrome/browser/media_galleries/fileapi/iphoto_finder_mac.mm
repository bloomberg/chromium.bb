// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/iphoto_finder_mac.h"

#import "base/mac/foundation_util.h"
#include "chrome/browser/media_galleries/fileapi/iapp_finder_mac.h"
#include "chrome/browser/storage_monitor/storage_info.h"

namespace iphoto {

namespace {

// IPhotoFinder looks for the iPhoto library in an asynchronous manner and
// calls the given IPhotoFinderCallback on the UI thread as soon as it knows
// the result. If an iPhoto library exists, the IPhotoFinderCallback gets the
// device id for the library. If an iPhoto library does not exist, or the OS
// does not support iPhoto, then the callback result is an empty string.
// |callback| runs on the UI thread. This class deletes itself.
class IPhotoFinder : public iapps::IAppFinderMac {
 public:
  explicit IPhotoFinder(const IPhotoFinderCallback& callback)
    : IAppFinderMac(StorageInfo::IPHOTO, CFSTR("iPhotoRecentDatabases"),
                    callback) {
  }

  virtual ~IPhotoFinder() {}

 private:
  virtual base::FilePath ExtractPath(NSString* path_ns) const OVERRIDE {
    NSURL* url = [NSURL URLWithString:path_ns];
    if (![url isFileURL])
      return base::FilePath();

    NSString* expanded_path_ns = [url path];
    return base::mac::NSStringToFilePath(expanded_path_ns);
  }

  DISALLOW_COPY_AND_ASSIGN(IPhotoFinder);
};

}  // namespace

void FindIPhotoLibrary(const IPhotoFinderCallback& callback) {
  (new IPhotoFinder(callback))->Start();
}

}  // namespace iphoto
