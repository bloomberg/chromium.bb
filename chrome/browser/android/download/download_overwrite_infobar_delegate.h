// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_OVERWRITE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_OVERWRITE_INFOBAR_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "components/infobars/core/infobar_delegate.h"

class InfoBarService;

namespace chrome {
namespace android {

// An infobar that asks if it is ok to overwrite an
// existing file. Due to limited disk space on Android, two options are
// presented to the user when downloading a file whose name conflicts with an
// already present file:
//
// 1. Overwrite the file.
// 2. Create a new file.
//
// Also, the user can dismiss the infobar to abort the download.
//
// Note that this infobar does not expire if the user subsequently navigates,
// since such navigations won't automatically cancel the underlying download.
class DownloadOverwriteInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  // This is called when the user chooses to overwrite the existing file.
  virtual void OverwriteExistingFile() = 0;

  // This is called when the user chooses to create a new file.
  virtual void CreateNewFile() = 0;

  // Gets the file name to be downloaded.
  virtual std::string GetFileName() const = 0;
  // Gets the download directory name.
  virtual std::string GetDirName() const = 0;
  // Gets the full directory path.
  virtual std::string GetDirFullPath() const = 0;

  bool ShouldExpire(const NavigationDetails& details) const override;
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_OVERWRITE_INFOBAR_DELEGATE_H_
