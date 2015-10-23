// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_INFOBAR_DELEGATE_ANDROID_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class InfoBarService;

// An infobar delegate that presents the user with a choice to allow or deny
// multiple downloads from the same site. This confirmation step protects
// against "carpet-bombing", where a malicious site forces multiple downloads
// on an unsuspecting user.
class DownloadRequestInfoBarDelegateAndroid : public ConfirmInfoBarDelegate {
 public:
  typedef base::Callback<void(
      InfoBarService* infobar_service,
      base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host)>
      FakeCreateCallback;

  ~DownloadRequestInfoBarDelegateAndroid() override;

  // Creates a download request delegate and adds it to |infobar_service|.
  static void Create(
      InfoBarService* infobar_service,
      base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host);

#if defined(UNIT_TEST)
  static scoped_ptr<DownloadRequestInfoBarDelegateAndroid> Create(
      base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host) {
    return scoped_ptr<DownloadRequestInfoBarDelegateAndroid>(
        new DownloadRequestInfoBarDelegateAndroid(host));
  }
#endif

  static void SetCallbackForTesting(FakeCreateCallback* callback);

 private:
  static FakeCreateCallback* callback_;

  explicit DownloadRequestInfoBarDelegateAndroid(
      base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host);

  // ConfirmInfoBarDelegate:
  int GetIconId() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  bool responded_;
  base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host_;

  DISALLOW_COPY_AND_ASSIGN(DownloadRequestInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_INFOBAR_DELEGATE_ANDROID_H_
