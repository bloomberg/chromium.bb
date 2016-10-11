// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/downloads/offline_page_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/download_overwrite_infobar.h"

namespace offline_pages {

// static
void OfflinePageInfoBarDelegate::Create(
    const base::Callback<void(Action)>& confirm_continuation,
    const std::string& downloads_label,
    const std::string& page_name,
    content::WebContents* web_contents) {
  InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(DownloadOverwriteInfoBar::CreateInfoBar(
          base::WrapUnique(new OfflinePageInfoBarDelegate(
              confirm_continuation, downloads_label, page_name))));
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
