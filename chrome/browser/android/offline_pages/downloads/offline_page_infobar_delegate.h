// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_INFOBAR_DELEGATE_H_

#include "base/callback_forward.h"
#include "chrome/browser/android/download/download_overwrite_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"

namespace content {
class WebContents;
}

namespace offline_pages {

class OfflinePageInfoBarDelegate
    : public chrome::android::DownloadOverwriteInfoBarDelegate {
 public:
  // Creates an offline page infobar and a delegate and adds the infobar to the
  // InfoBarService associated with |web_contents|. |page_name| is the name
  // shown for this file in the infobar text.
  static void Create(const base::Closure& confirm_continuation,
                     const std::string& page_name,
                     content::WebContents* web_contents);
  ~OfflinePageInfoBarDelegate() override;

 private:
  OfflinePageInfoBarDelegate(const base::Closure& confirm_continuation,
                             const std::string& page_name);

  // DownloadOverwriteInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool EqualsDelegate(InfoBarDelegate* delegate) const override;
  bool OverwriteExistingFile() override;
  bool CreateNewFile() override;
  std::string GetFileName() const override;
  std::string GetDirName() const override;
  std::string GetDirFullPath() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  OfflinePageInfoBarDelegate* AsOfflinePageInfoBarDelegate() override;

  // Continuation called when the user chooses to create a new file.
  base::Closure confirm_continuation_;

  std::string page_name_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageInfoBarDelegate);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_INFOBAR_DELEGATE_H_
