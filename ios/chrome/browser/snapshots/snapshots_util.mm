// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/snapshots/snapshots_util.h"

#import <UIKit/UIKit.h>

#include "base/files/file_util.h"
#include "base/ios/ios_util.h"
#include "base/location.h"
#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "ios/web/public/web_thread.h"

namespace {
const char* kOrientationDescriptions[] = {
    "LandscapeLeft",
    "LandscapeRight",
    "Portrait",
    "PortraitUpsideDown",
};
}  // namespace

void ClearIOSSnapshots() {
  // Generates a list containing all the possible snapshot paths because the
  // list of snapshots stored on the device can't be obtained programmatically.
  std::vector<base::FilePath> snapshotsPaths;
  GetSnapshotsPaths(&snapshotsPaths);
  for (base::FilePath snapshotPath : snapshotsPaths) {
    web::WebThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&base::DeleteFile), snapshotPath, false));
  }
}

void GetSnapshotsPaths(std::vector<base::FilePath>* snapshotsPaths) {
  DCHECK(snapshotsPaths);
  base::FilePath snapshotsDir;
  PathService::Get(base::DIR_CACHE, &snapshotsDir);
  // Snapshots are located in a path with the bundle ID used twice.
  snapshotsDir = snapshotsDir.Append("Snapshots")
                     .Append(base::mac::BaseBundleID())
                     .Append(base::mac::BaseBundleID());
  const char* retinaSuffix = "";
  CGFloat scale = [UIScreen mainScreen].scale;
  if (scale == 2) {
    retinaSuffix = "@2x";
  } else if (scale == 3) {
    retinaSuffix = "@3x";
  }
  for (unsigned int i = 0; i < arraysize(kOrientationDescriptions); i++) {
    std::string snapshotFilename =
        base::StringPrintf("UIApplicationAutomaticSnapshotDefault-%s%s.png",
                           kOrientationDescriptions[i], retinaSuffix);
    base::FilePath snapshotPath = snapshotsDir.Append(snapshotFilename);
    snapshotsPaths->push_back(snapshotPath);
  }
}
