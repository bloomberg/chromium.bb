// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STREAM_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STREAM_INFOBAR_DELEGATE_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/media/media_stream_devices_controller.h"
#include "components/infobars/core/confirm_infobar_delegate.h"


// This class configures an infobar shown when a page requests access to a
// user's microphone and/or video camera.  The user is shown a message asking
// which audio and/or video devices he wishes to use with the current page, and
// buttons to give access to the selected devices to the page, or to deny access
// to them.
class MediaStreamInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  virtual ~MediaStreamInfoBarDelegate();

  // Handles a permission request (in |request|) for |web_contents|.  If this
  // involves prompting the user, creates a media stream infobar and delegate,
  // then checks for an existing infobar for |web_contents| and replaces it if
  // found, or just adds the new infobar otherwise.  Returns whether an infobar
  // was created.
  static bool Create(content::WebContents* web_contents,
                     const content::MediaStreamRequest& request,
                     const content::MediaResponseCallback& callback);

 private:
  friend class WebRtcTestBase;

  explicit MediaStreamInfoBarDelegate(
      scoped_ptr<MediaStreamDevicesController> controller);

  // ConfirmInfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual int GetIconID() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual MediaStreamInfoBarDelegate* AsMediaStreamInfoBarDelegate() OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual base::string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  scoped_ptr<MediaStreamDevicesController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamInfoBarDelegate);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STREAM_INFOBAR_DELEGATE_H_
