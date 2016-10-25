// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/downloads/offline_page_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/download_overwrite_infobar.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/gfx/text_elider.h"

namespace offline_pages {

// static
void OfflinePageInfoBarDelegate::Create(
    const base::Callback<void(Action)>& confirm_continuation,
    const std::string& downloads_label,
    const GURL& page_to_download,
    content::WebContents* web_contents) {
  // The URL could be very long, especially since we are including query
  // parameters, path, etc.  Elide the URL to a shorter length because the
  // infobar cannot handle scrolling and completely obscures Chrome if the text
  // is too long.
  //
  // 150 was chosen as it does not cause the infobar to overrun the screen on a
  // test Android One device with 480 x 854 resolution.  At this resolution the
  // infobar covers approximately 2/3 of the screen, and all controls are still
  // visible.
  //
  // TODO(dewittj): Display something better than an elided URL string in the
  // infobar.
  const size_t kMaxLengthOfDisplayedPageUrl = 150;

  base::string16 formatted_url = url_formatter::FormatUrl(page_to_download);
  base::string16 elided_url;
  gfx::ElideString(formatted_url, kMaxLengthOfDisplayedPageUrl, &elided_url);

  InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(DownloadOverwriteInfoBar::CreateInfoBar(base::WrapUnique(
          new OfflinePageInfoBarDelegate(confirm_continuation, downloads_label,
                                         base::UTF16ToUTF8(elided_url)))));
}

OfflinePageInfoBarDelegate::~OfflinePageInfoBarDelegate() {}

OfflinePageInfoBarDelegate::OfflinePageInfoBarDelegate(
    const base::Callback<void(Action)>& confirm_continuation,
    const std::string& downloads_label,
    const std::string& page_name)
    : confirm_continuation_(confirm_continuation),
      downloads_label_(downloads_label),
      page_name_(page_name) {}

infobars::InfoBarDelegate::InfoBarIdentifier
OfflinePageInfoBarDelegate::GetIdentifier() const {
  return OFFLINE_PAGE_INFOBAR_DELEGATE;
}

bool OfflinePageInfoBarDelegate::EqualsDelegate(
    InfoBarDelegate* delegate) const {
  OfflinePageInfoBarDelegate* confirm_delegate =
      delegate->AsOfflinePageInfoBarDelegate();
  return confirm_delegate && GetFileName() == confirm_delegate->GetFileName();
}

bool OfflinePageInfoBarDelegate::OverwriteExistingFile() {
  // TODO(dewittj): Downloads UI intends to remove this functionality.
  confirm_continuation_.Run(Action::OVERWRITE);
  return true;
}

bool OfflinePageInfoBarDelegate::CreateNewFile() {
  confirm_continuation_.Run(Action::CREATE_NEW);
  return true;
}

std::string OfflinePageInfoBarDelegate::GetFileName() const {
  return page_name_;
}

std::string OfflinePageInfoBarDelegate::GetDirName() const {
  return downloads_label_;
}

std::string OfflinePageInfoBarDelegate::GetDirFullPath() const {
  return std::string();
}

bool OfflinePageInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return InfoBarDelegate::ShouldExpire(details);
}

OfflinePageInfoBarDelegate*
OfflinePageInfoBarDelegate::AsOfflinePageInfoBarDelegate() {
  return this;
}

}  // namespace offline_pages
