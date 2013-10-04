// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/itunes_finder_mac.h"

#import "base/mac/foundation_util.h"

namespace itunes {

ITunesFinderMac::ITunesFinderMac(const ITunesFinderCallback& callback)
    : IAppFinderMac(StorageInfo::ITUNES, CFSTR("iTunesRecentDatabasePaths"),
                    callback) {
}

ITunesFinderMac::~ITunesFinderMac() {}

base::FilePath ITunesFinderMac::ExtractPath(NSString* path_ns) const {
  NSString* expanded_path_ns = [path_ns stringByExpandingTildeInPath];
  return base::mac::NSStringToFilePath(expanded_path_ns);
};

}  // namespace itunes
