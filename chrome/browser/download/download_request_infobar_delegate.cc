// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_infobar_delegate.h"

#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

DownloadRequestInfoBarDelegate::DownloadRequestInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    DownloadRequestLimiter::TabDownloadState* host)
    : ConfirmInfoBarDelegate(infobar_helper),
      host_(host) {
}

DownloadRequestInfoBarDelegate::~DownloadRequestInfoBarDelegate() {
  if (host_)
    host_->Cancel();
}

gfx::Image* DownloadRequestInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
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
