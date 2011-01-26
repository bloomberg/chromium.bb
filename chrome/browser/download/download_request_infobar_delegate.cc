// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_infobar_delegate.h"

#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

DownloadRequestInfoBarDelegate::DownloadRequestInfoBarDelegate(
    TabContents* tab,
    DownloadRequestLimiter::TabDownloadState* host)
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

SkBitmap* DownloadRequestInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_MULTIPLE_DOWNLOADS);
}

string16 DownloadRequestInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_MULTI_DOWNLOAD_WARNING);
}

string16 DownloadRequestInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_MULTI_DOWNLOAD_WARNING_ALLOW : IDS_MULTI_DOWNLOAD_WARNING_DENY);
}

bool DownloadRequestInfoBarDelegate::Accept() {
  if (host_) {
    // Accept() call will nullify host_ if no further prompts are required.
    host_->Accept();
  }

  return !host_;
}

bool DownloadRequestInfoBarDelegate::Cancel() {
  if (host_) {
    host_->Cancel();
    host_ = NULL;
  }
  return true;
}
