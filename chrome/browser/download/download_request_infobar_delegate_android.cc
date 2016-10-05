// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_infobar_delegate_android.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/infobars/core/infobar.h"
#include "ui/base/l10n/l10n_util.h"

DownloadRequestInfoBarDelegateAndroid::FakeCreateCallback*
    DownloadRequestInfoBarDelegateAndroid::callback_ = NULL;

DownloadRequestInfoBarDelegateAndroid::
    ~DownloadRequestInfoBarDelegateAndroid() {
  if (!responded_ && host_)
    host_->CancelOnce();
}

// static
void DownloadRequestInfoBarDelegateAndroid::Create(
    InfoBarService* infobar_service,
    base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host) {
  if (DownloadRequestInfoBarDelegateAndroid::callback_ &&
      !DownloadRequestInfoBarDelegateAndroid::callback_->is_null()) {
    DownloadRequestInfoBarDelegateAndroid::callback_->Run(infobar_service,
                                                          host);
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
    infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
        std::unique_ptr<ConfirmInfoBarDelegate>(
            new DownloadRequestInfoBarDelegateAndroid(host))));
  }
}

// static
void DownloadRequestInfoBarDelegateAndroid::SetCallbackForTesting(
    FakeCreateCallback* callback) {
  DownloadRequestInfoBarDelegateAndroid::callback_ = callback;
}

DownloadRequestInfoBarDelegateAndroid::DownloadRequestInfoBarDelegateAndroid(
    base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host)
    : ConfirmInfoBarDelegate(), responded_(false), host_(host) {}

infobars::InfoBarDelegate::InfoBarIdentifier
DownloadRequestInfoBarDelegateAndroid::GetIdentifier() const {
  return DOWNLOAD_REQUEST_INFOBAR_DELEGATE_ANDROID;
}

int DownloadRequestInfoBarDelegateAndroid::GetIconId() const {
  return IDR_ANDROID_INFOBAR_MULTIPLE_DOWNLOADS;
}

base::string16 DownloadRequestInfoBarDelegateAndroid::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_MULTI_DOWNLOAD_WARNING);
}

base::string16 DownloadRequestInfoBarDelegateAndroid::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK)
                                       ? IDS_MULTI_DOWNLOAD_WARNING_ALLOW
                                       : IDS_MULTI_DOWNLOAD_WARNING_BLOCK);
}

bool DownloadRequestInfoBarDelegateAndroid::Accept() {
  DCHECK(!responded_);
  responded_ = true;
  if (host_) {
    // This may invalidate |host_|.
    host_->Accept();
  }
  return !host_;
}

bool DownloadRequestInfoBarDelegateAndroid::Cancel() {
  DCHECK(!responded_);
  responded_ = true;
  if (host_) {
    // This may invalidate |host_|.
    host_->Cancel();
  }
  return !host_;
}
