// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_INFOBAR_DELEGATE_H_

#include "base/callback.h"
#include "chrome/browser/android/download/duplicate_download_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace offline_pages {

// An InfoBarDelegate that appears when a user attempt to save offline pages for
// a URL that is already saved.  This piggy-backs off the Download infobar,
// since the UI should be the same between Downloads and Offline Pages in this
// case.  There are two actions: Create New, and Overwrite.  Since Overwrite is
// not straightforward for offline pages, the behavior is to delete ALL other
// pages that are saved for the given URL, then save the newly requested page.
class OfflinePageInfoBarDelegate
    : public chrome::android::DuplicateDownloadInfoBarDelegate {
 public:
  // Creates an offline page infobar and a delegate and adds the infobar to the
  // InfoBarService associated with |web_contents|. |page_name| is the name
  // shown for this file in the infobar text.
  static void Create(const base::Closure& confirm_continuation,
                     const GURL& page_to_download,
                     content::WebContents* web_contents);
  ~OfflinePageInfoBarDelegate() override;

 private:
  OfflinePageInfoBarDelegate(
      const base::Closure& confirm_continuation,
      const std::string& page_name,
      const GURL& page_to_download);

  // DuplicateDownloadInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool EqualsDelegate(InfoBarDelegate* delegate) const override;
  bool Accept() override;
  bool Cancel() override;
  std::string GetFilePath() const override;
  bool IsOfflinePage() const override;
  std::string GetPageURL() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  OfflinePageInfoBarDelegate* AsOfflinePageInfoBarDelegate() override;

  // Continuation called when the user chooses to create a new file.
  base::Closure confirm_continuation_;

  std::string page_name_;
  GURL page_to_download_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageInfoBarDelegate);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_INFOBAR_DELEGATE_H_
