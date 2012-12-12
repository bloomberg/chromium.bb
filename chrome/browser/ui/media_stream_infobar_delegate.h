// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_STREAM_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_MEDIA_STREAM_INFOBAR_DELEGATE_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/media/media_stream_devices_controller.h"

class InfoBarTabHelper;

// This class configures an infobar shown when a page requests access to a
// user's microphone and/or video camera.  The user is shown a message asking
// which audio and/or video devices he wishes to use with the current page, and
// buttons to give access to the selected devices to the page, or to deny access
// to them.
class MediaStreamInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // MediaStreamInfoBarDelegate takes the ownership of the |controller|.
  MediaStreamInfoBarDelegate(
      InfoBarTabHelper* tab_helper,
      MediaStreamDevicesController* controller);

  virtual ~MediaStreamInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual MediaStreamInfoBarDelegate* AsMediaStreamInfoBarDelegate() OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

 private:
  scoped_ptr<MediaStreamDevicesController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_MEDIA_STREAM_INFOBAR_DELEGATE_H_
