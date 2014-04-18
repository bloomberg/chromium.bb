// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

DownloadRequestInfoBarDelegate::FakeCreateCallback*
    DownloadRequestInfoBarDelegate::callback_ = NULL;

DownloadRequestInfoBarDelegate::~DownloadRequestInfoBarDelegate() {
  if (!responded_ && host_)
    host_->CancelOnce();
}

// static
void DownloadRequestInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host) {
  if (DownloadRequestInfoBarDelegate::callback_ &&
      !DownloadRequestInfoBarDelegate::callback_->is_null()) {
    DownloadRequestInfoBarDelegate::callback_->Run(infobar_service, host);
  } else if (!infobar_service) {
    // |web_contents| may not have a InfoBarService if it's actually a
    // WebContents like those used for extension popups/bubbles and hosted apps
    // etc.
    // TODO(benjhayden): If this is an automatic download from an extension,
    // it would be convenient for the extension author if we send a message to
    // the extension's DevTools console (as we do for CSP) about how
    // extensions should use chrome.downloads.download() (requires the
    // "downloads" permission) to automatically download >1 files.
    host->Cancel();
  } else {
    infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
        scoped_ptr<ConfirmInfoBarDelegate>(
            new DownloadRequestInfoBarDelegate(host))));
  }
}

// static
void DownloadRequestInfoBarDelegate::SetCallbackForTesting(
    FakeCreateCallback* callback) {
  DownloadRequestInfoBarDelegate::callback_ = callback;
}

DownloadRequestInfoBarDelegate::DownloadRequestInfoBarDelegate(
    base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host)
    : ConfirmInfoBarDelegate(),
      responded_(false),
      host_(host) {
}

int DownloadRequestInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_MULTIPLE_DOWNLOADS;
}

base::string16 DownloadRequestInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_MULTI_DOWNLOAD_WARNING);
}

base::string16 DownloadRequestInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_MULTI_DOWNLOAD_WARNING_ALLOW : IDS_MULTI_DOWNLOAD_WARNING_DENY);
}

bool DownloadRequestInfoBarDelegate::Accept() {
  DCHECK(!responded_);
  responded_ = true;
  if (host_) {
    // This may invalidate |host_|.
    host_->Accept();
  }
  return !host_;
}

bool DownloadRequestInfoBarDelegate::Cancel() {
  DCHECK(!responded_);
  responded_ = true;
  if (host_) {
    // This may invalidate |host_|.
    host_->Cancel();
  }
  return !host_;
}
