// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_infobar_delegate.h"

#include "chrome/browser/api/infobars/infobar_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

DownloadRequestInfoBarDelegate::FakeCreateCallback*
  DownloadRequestInfoBarDelegate::callback_ = NULL;

DownloadRequestInfoBarDelegate::~DownloadRequestInfoBarDelegate() {
  if (host_)
    host_->Cancel();
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
    infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
        new DownloadRequestInfoBarDelegate(infobar_service, host)));
  }
}

// static
void DownloadRequestInfoBarDelegate::SetCallbackForTesting(
    FakeCreateCallback* callback) {
  DownloadRequestInfoBarDelegate::callback_ = callback;
}

DownloadRequestInfoBarDelegate::DownloadRequestInfoBarDelegate(
    InfoBarService* infobar_service,
    base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host)
    : ConfirmInfoBarDelegate(infobar_service),
      host_(host) {
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
    // Accept() call will invalidate host_ weak pointer if no further
    // prompts are required.
    host_->Accept();
  }

  return !host_;
}
