// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_permission_request.h"

#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

DownloadPermissionRequest::DownloadPermissionRequest(
    base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host)
    : host_(host) {
  const content::WebContents* web_contents = host_->web_contents();
  DCHECK(web_contents);
  request_origin_ = web_contents->GetURL().GetOrigin();
}

DownloadPermissionRequest::~DownloadPermissionRequest() {}

int DownloadPermissionRequest::GetIconId() const {
  return IDR_INFOBAR_MULTIPLE_DOWNLOADS;
}

base::string16 DownloadPermissionRequest::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_MULTI_DOWNLOAD_WARNING);
}

base::string16 DownloadPermissionRequest::GetMessageTextFragment() const {
  return l10n_util::GetStringUTF16(IDS_MULTI_DOWNLOAD_PERMISSION_FRAGMENT);
}

GURL DownloadPermissionRequest::GetOrigin() const {
  return request_origin_;
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
  if (host_) {
    // This may invalidate |host_|.
    host_->CancelOnce();
  }
}

void DownloadPermissionRequest::RequestFinished() {
  delete this;
}

PermissionBubbleType DownloadPermissionRequest::GetPermissionBubbleType()
    const {
  return PermissionBubbleType::DOWNLOAD;
}
