// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_DRIVE_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_DRIVE_SOURCE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/fileapi/recent_source.h"
#include "components/drive/chromeos/file_system_interface.h"
#include "components/drive/file_errors.h"

class Profile;

namespace chromeos {

// RecentSource implementation for Drive files.
//
// All member functions must be called on the UI thread.
//
// TODO(nya): Write unit tests.
class RecentDriveSource : public RecentSource {
 public:
  explicit RecentDriveSource(Profile* profile);
  ~RecentDriveSource() override;

  // RecentSource overrides:
  void GetRecentFiles(RecentContext context,
                      GetRecentFilesCallback callback) override;

 private:
  static const char kLoadHistogramName[];

  void OnSearchMetadata(
      RecentContext context,
      GetRecentFilesCallback callback,
      const base::TimeTicks& build_start_time,
      drive::FileError error,
      std::unique_ptr<drive::MetadataSearchResultVector> results);

  Profile* const profile_;

  base::WeakPtrFactory<RecentDriveSource> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecentDriveSource);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_DRIVE_SOURCE_H_
