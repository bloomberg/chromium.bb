// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "chrome/browser/download/download_request_manager.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"

class TabContents;

// An infobar delegate that presents the user with a choice to allow or deny
// multiple downloads from the same site. This confirmation step protects
// against "carpet-bombing", where a malicious site forces multiple downloads
// on an unsuspecting user.
class DownloadRequestInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  DownloadRequestInfoBarDelegate(
      TabContents* tab, DownloadRequestManager::TabDownloadState* host);

  virtual ~DownloadRequestInfoBarDelegate();

  void set_host(DownloadRequestManager::TabDownloadState* host) {
    host_ = host;
  }

  virtual void InfoBarClosed();

  virtual std::wstring GetMessageText() const;

  virtual SkBitmap* GetIcon() const;

  virtual int GetButtons() const;

  virtual std::wstring GetButtonLabel(
      ConfirmInfoBarDelegate::InfoBarButton button) const;

  virtual bool Accept();

  virtual bool Cancel();

 private:
  DownloadRequestManager::TabDownloadState* host_;

  DISALLOW_COPY_AND_ASSIGN(DownloadRequestInfoBarDelegate);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_INFOBAR_DELEGATE_H_
