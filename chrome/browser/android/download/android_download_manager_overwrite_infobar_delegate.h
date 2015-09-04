// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_OVERWRITE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_OVERWRITE_INFOBAR_DELEGATE_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/android/download/download_overwrite_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"

class InfoBarService;

namespace chrome {
namespace android {

// An infobar delegate that is initiated from Java side.
class AndroidDownloadManagerOverwriteInfoBarDelegate
    : public DownloadOverwriteInfoBarDelegate {
 public:
  ~AndroidDownloadManagerOverwriteInfoBarDelegate() override;

  static void Create(InfoBarService* infobar_service,
                     const std::string& file_name,
                     const std::string& dir_name,
                     const std::string& dir_full_path,
                     jobject chrome_download_delegate,
                     jobject download_info);

 private:
  AndroidDownloadManagerOverwriteInfoBarDelegate(
      const std::string& file_name,
      const std::string& dir_name,
      const std::string& dir_full_path,
      jobject chrome_download_delegate,
      jobject download_info);

  // DownloadOverwriteInfoBarDelegate:
  bool OverwriteExistingFile() override;
  bool CreateNewFile() override;
  std::string GetFileName() const override;
  std::string GetDirName() const override;
  std::string GetDirFullPath() const override;

  std::string file_name_;
  std::string dir_name_;
  std::string dir_full_path_;
  base::android::ScopedJavaGlobalRef<jobject> chrome_download_delegate_;
  base::android::ScopedJavaGlobalRef<jobject> download_info_;

  DISALLOW_COPY_AND_ASSIGN(AndroidDownloadManagerOverwriteInfoBarDelegate);
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_OVERWRITE_INFOBAR_DELEGATE_H_
