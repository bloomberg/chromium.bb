// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SCREEN_CAPTURE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_SCREEN_CAPTURE_INFOBAR_DELEGATE_H_

#include "base/compiler_specific.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/media_stream_request.h"

class InfoBarService;

class ScreenCaptureInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static void Create(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback);

  virtual ~ScreenCaptureInfoBarDelegate();

 private:
  ScreenCaptureInfoBarDelegate(
      InfoBarService* infobar_service,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback);

  // Base class: ConfirmInfoBarDelegate.
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual ScreenCaptureInfoBarDelegate*
      AsScreenCaptureInfoBarDelegate() OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  void Deny();

  const content::MediaStreamRequest request_;
  const content::MediaResponseCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_SCREEN_CAPTURE_INFOBAR_DELEGATE_H_
