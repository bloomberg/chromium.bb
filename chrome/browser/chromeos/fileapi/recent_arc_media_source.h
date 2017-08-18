// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_ARC_MEDIA_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_ARC_MEDIA_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/fileapi/recent_context.h"
#include "chrome/browser/chromeos/fileapi/recent_source.h"

class Profile;

namespace chromeos {

// RecentSource implementation for ARC media view.
//
// All member functions must be called on the UI thread.
class RecentArcMediaSource : public RecentSource {
 public:
  explicit RecentArcMediaSource(Profile* profile);
  ~RecentArcMediaSource() override;

  // RecentSource overrides:
  void GetRecentFiles(RecentContext context,
                      GetRecentFilesCallback callback) override;

 private:
  class MediaRoot;

  void OnGetRecentFilesForRoot(RecentFileList files);
  void OnComplete();

  Profile* const profile_;
  std::vector<std::unique_ptr<MediaRoot>> roots_;

  RecentContext context_;
  GetRecentFilesCallback callback_;

  int num_inflight_roots_ = 0;
  RecentFileList files_;

  base::WeakPtrFactory<RecentArcMediaSource> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecentArcMediaSource);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_ARC_MEDIA_SOURCE_H_
