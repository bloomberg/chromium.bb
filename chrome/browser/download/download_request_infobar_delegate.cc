// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_infobar_delegate.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

DownloadRequestInfoBarDelegate::DownloadRequestInfoBarDelegate(TabContents* tab,
    DownloadRequestManager::TabDownloadState* host)
    : ConfirmInfoBarDelegate(tab),
      host_(host) {
  if (tab)
    tab->AddInfoBar(this);
}

DownloadRequestInfoBarDelegate::~DownloadRequestInfoBarDelegate() {
}

void DownloadRequestInfoBarDelegate::InfoBarClosed() {
  Cancel();
  // This will delete us.
  ConfirmInfoBarDelegate::InfoBarClosed();
}

std::wstring DownloadRequestInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetString(IDS_MULTI_DOWNLOAD_WARNING);
}

SkBitmap* DownloadRequestInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_MULTIPLE_DOWNLOADS);
}

int DownloadRequestInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

std::wstring DownloadRequestInfoBarDelegate::GetButtonLabel(
    ConfirmInfoBarDelegate::InfoBarButton button) const {
  if (button == BUTTON_OK)
    return l10n_util::GetString(IDS_MULTI_DOWNLOAD_WARNING_ALLOW);
  else
    return l10n_util::GetString(IDS_MULTI_DOWNLOAD_WARNING_DENY);
}

bool DownloadRequestInfoBarDelegate::Accept() {
  if (host_) {
    host_->Accept();
    host_ = NULL;
  }
  return true;
}

bool DownloadRequestInfoBarDelegate::Cancel() {
  if (host_) {
    host_->Cancel();
    host_ = NULL;
  }
  return true;
}
