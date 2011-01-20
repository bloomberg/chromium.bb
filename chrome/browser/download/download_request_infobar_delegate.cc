// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_infobar_delegate.h"

#include "app/l10n_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

DownloadRequestInfoBarDelegate::DownloadRequestInfoBarDelegate(TabContents* tab,
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

string16 DownloadRequestInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_MULTI_DOWNLOAD_WARNING);
}

SkBitmap* DownloadRequestInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_MULTIPLE_DOWNLOADS);
}

int DownloadRequestInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 DownloadRequestInfoBarDelegate::GetButtonLabel(
    ConfirmInfoBarDelegate::InfoBarButton button) const {
  if (button == BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_MULTI_DOWNLOAD_WARNING_ALLOW);
  else
    return l10n_util::GetStringUTF16(IDS_MULTI_DOWNLOAD_WARNING_DENY);
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
