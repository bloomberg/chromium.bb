// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_OVERWRITE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_OVERWRITE_INFOBAR_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/android/download/download_overwrite_infobar_delegate.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "components/infobars/core/infobar_delegate.h"

using base::android::ScopedJavaGlobalRef;

class InfoBarService;

namespace chrome {
namespace android {

// An infobar delegate that starts from the given file path.
class ChromeDownloadManagerOverwriteInfoBarDelegate
    : public DownloadOverwriteInfoBarDelegate {
 public:
  ~ChromeDownloadManagerOverwriteInfoBarDelegate() override;

  static void Create(
      InfoBarService* infobar_service,
      const base::FilePath& suggested_download_path,
      const DownloadTargetDeterminerDelegate::FileSelectedCallback&
          file_selected_callback);

 private:
  ChromeDownloadManagerOverwriteInfoBarDelegate(
      const base::FilePath& suggested_path,
      const DownloadTargetDeterminerDelegate::FileSelectedCallback& callback);

  // DownloadOverwriteInfoBarDelegate:
  bool OverwriteExistingFile() override;
  bool CreateNewFile() override;
  std::string GetFileName() const override;
  std::string GetDirName() const override;
  std::string GetDirFullPath() const override;
  void InfoBarDismissed() override;

  // Called on the FILE thread to create a new file.  Calls |callback| on the UI
  // thread when finished.
  static void CreateNewFileInternal(
      const base::FilePath& suggested_download_path,
      const DownloadTargetDeterminerDelegate::FileSelectedCallback& callback);

  // The suggested download path from download target determiner. This is used
  // to show users the file name and the directory that will be used.
  base::FilePath suggested_download_path_;

  // A callback to download target determiner to notify that file selection
  // is made (or cancelled).
  DownloadTargetDeterminerDelegate::FileSelectedCallback
      file_selected_callback_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDownloadManagerOverwriteInfoBarDelegate);
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_OVERWRITE_INFOBAR_DELEGATE_H_
