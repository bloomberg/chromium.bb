// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_DUPLICATE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_DUPLICATE_INFOBAR_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/download/duplicate_download_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"

class InfoBarService;

namespace chrome {
namespace android {

// An infobar delegate that is initiated from Java side. This infobar is used
// when user tries to download an OMA DRM file that has already been downloaded.
class AndroidDownloadManagerDuplicateInfoBarDelegate
    : public DuplicateDownloadInfoBarDelegate {
 public:
  ~AndroidDownloadManagerDuplicateInfoBarDelegate() override;

  static void Create(InfoBarService* infobar_service,
                     const std::string& file_path,
                     jobject chrome_download_delegate,
                     jobject download_info,
                     jboolean is_off_the_record);

 private:
  AndroidDownloadManagerDuplicateInfoBarDelegate(
      const std::string& file_path,
      jobject chrome_download_delegate,
      jobject download_info,
      jboolean is_off_the_record);

  // DownloadOverwriteInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool Accept() override;
  bool Cancel() override;
  std::string GetFilePath() const override;
  bool IsOffTheRecord() const override;

  std::string file_path_;
  base::android::ScopedJavaGlobalRef<jobject> chrome_download_delegate_;
  base::android::ScopedJavaGlobalRef<jobject> download_info_;
  bool is_off_the_record_;

  DISALLOW_COPY_AND_ASSIGN(AndroidDownloadManagerDuplicateInfoBarDelegate);
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_ANDROID_DOWNLOAD_MANAGER_DUPLICATE_INFOBAR_DELEGATE_H_
