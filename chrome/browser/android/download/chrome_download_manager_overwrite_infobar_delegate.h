// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_OVERWRITE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_OVERWRITE_INFOBAR_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/android/download/download_overwrite_infobar_delegate.h"
#include "chrome/browser/download/download_path_reservation_tracker.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/download_item.h"

using base::android::ScopedJavaGlobalRef;

class InfoBarService;

namespace chrome {
namespace android {

// An infobar delegate that starts from the given file path.
class ChromeDownloadManagerOverwriteInfoBarDelegate
    : public DownloadOverwriteInfoBarDelegate,
      public content::DownloadItem::Observer {
 public:
  ~ChromeDownloadManagerOverwriteInfoBarDelegate() override;

  static void Create(
      InfoBarService* infobar_service,
      content::DownloadItem* download_item,
      const base::FilePath& suggested_download_path,
      const DownloadTargetDeterminerDelegate::FileSelectedCallback&
          file_selected_callback);

  // content::DownloadItem::Observer
  void OnDownloadDestroyed(content::DownloadItem* download_item) override;

 private:
  ChromeDownloadManagerOverwriteInfoBarDelegate(
      content::DownloadItem* download_item,
      const base::FilePath& suggested_path,
      const DownloadTargetDeterminerDelegate::FileSelectedCallback& callback);

  // DownloadOverwriteInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool OverwriteExistingFile() override;
  bool CreateNewFile() override;
  std::string GetFileName() const override;
  std::string GetDirName() const override;
  std::string GetDirFullPath() const override;
  void InfoBarDismissed() override;

  // The download item that is requesting the infobar. Could get deleted while
  // the infobar is showing.
  content::DownloadItem* download_item_;

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
