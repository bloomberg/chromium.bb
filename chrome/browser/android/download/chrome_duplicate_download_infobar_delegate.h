// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_CHROME_DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_CHROME_DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/android/download/duplicate_download_infobar_delegate.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/download_item.h"

class InfoBarService;

namespace chrome {
namespace android {

// An infobar delegate that starts from the given file path.
class ChromeDuplicateDownloadInfoBarDelegate
    : public DuplicateDownloadInfoBarDelegate,
      public content::DownloadItem::Observer {
 public:
  ~ChromeDuplicateDownloadInfoBarDelegate() override;

  static void Create(
      InfoBarService* infobar_service,
      content::DownloadItem* download_item,
      const base::FilePath& file_path,
      const DownloadTargetDeterminerDelegate::FileSelectedCallback&
          file_selected_callback);

  // content::DownloadItem::Observer
  void OnDownloadDestroyed(content::DownloadItem* download_item) override;

 private:
  ChromeDuplicateDownloadInfoBarDelegate(
      content::DownloadItem* download_item,
      const base::FilePath& file_path,
      const DownloadTargetDeterminerDelegate::FileSelectedCallback& callback);

  // DownloadOverwriteInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool Accept() override;
  bool Cancel() override;
  std::string GetFilePath() const override;
  void InfoBarDismissed() override;
  bool IsOffTheRecord() const override;


  // The download item that is requesting the infobar. Could get deleted while
  // the infobar is showing.
  content::DownloadItem* download_item_;

  // The target file path to be downloaded. This is used to show users the
  // file name that will be used.
  base::FilePath file_path_;

  // Whether the download is off the record.
  bool is_off_the_record_;

  // A callback to download target determiner to notify that file selection
  // is made (or cancelled).
  DownloadTargetDeterminerDelegate::FileSelectedCallback
      file_selected_callback_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDuplicateDownloadInfoBarDelegate);
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_CHROME_DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_H_
