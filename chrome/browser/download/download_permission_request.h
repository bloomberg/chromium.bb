// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PERMISSION_REQUEST_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"

// A permission bubble request that presents the user with a choice to allow
// or deny multiple downloads from the same site. This confirmation step
// protects against "carpet-bombing", where a malicious site forces multiple
// downloads on an unsuspecting user.
class DownloadPermissionRequest : public PermissionBubbleRequest {
 public:
  explicit DownloadPermissionRequest(
      base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host);
  ~DownloadPermissionRequest() override;

  // PermisisonBubbleDelegate:
  int GetIconId() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetMessageTextFragment() const override;
  bool HasUserGesture() const override;
  GURL GetOrigin() const override;
  void PermissionGranted() override;
  void PermissionDenied() override;
  void Cancelled() override;
  void RequestFinished() override;

 private:
  base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host_;
  GURL request_origin_;

  DISALLOW_COPY_AND_ASSIGN(DownloadPermissionRequest);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PERMISSION_REQUEST_H_
