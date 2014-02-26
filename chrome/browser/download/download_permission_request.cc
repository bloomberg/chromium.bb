// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_permission_request.h"

#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

DownloadPermissionRequest::DownloadPermissionRequest(
    base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host)
    : host_(host) {}

DownloadPermissionRequest::~DownloadPermissionRequest() {}

base::string16 DownloadPermissionRequest::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_MULTI_DOWNLOAD_WARNING);
}

base::string16 DownloadPermissionRequest::GetMessageTextFragment() const {
  return l10n_util::GetStringUTF16(IDS_MULTI_DOWNLOAD_PERMISSION_FRAGMENT);
}

base::string16 DownloadPermissionRequest::GetAlternateAcceptButtonText() const {
  return l10n_util::GetStringUTF16(IDS_MULTI_DOWNLOAD_WARNING_ALLOW);
}

base::string16 DownloadPermissionRequest::GetAlternateDenyButtonText() const {
  return l10n_util::GetStringUTF16(IDS_MULTI_DOWNLOAD_WARNING_DENY);
}

void DownloadPermissionRequest::PermissionGranted() {
  if (host_) {
    // This may invalidate |host_|.
    host_->Accept();
  }
}

void DownloadPermissionRequest::PermissionDenied() {
  if (host_) {
    // This may invalidate |host_|.
    host_->Cancel();
  }
}

void DownloadPermissionRequest::Cancelled() {
  // TODO(gbillock): There's currently no suitable method for telling the host
  // that a request is cancelled.
}

void DownloadPermissionRequest::RequestFinished() {
  delete this;
}
