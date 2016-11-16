// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_INFOBAR_DELEGATE_ANDROID_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"
#include "chrome/browser/permissions/permission_infobar_delegate.h"

// This class configures an infobar shown when a page requests access to a
// user's microphone and/or video camera.  The user is shown a message asking
// which audio and/or video devices they wish to use with the current page, and
// buttons to give access to the selected devices to the page, or to deny access
// to them.
class MediaStreamInfoBarDelegateAndroid : public PermissionInfoBarDelegate {
 public:
  // Prompts the user by creating a media stream infobar and delegate,
  // then checks for an existing infobar for |web_contents| and replaces it if
  // found, or just adds the new infobar otherwise.  Returns whether an infobar
  // was created.
  static bool Create(content::WebContents* web_contents,
                     bool user_gesture,
                     std::unique_ptr<MediaStreamDevicesController> controller);

  MediaStreamInfoBarDelegateAndroid(
      Profile* profile,
      bool user_gesture,
      std::unique_ptr<MediaStreamDevicesController> controller);
 private:
  friend class WebRtcTestBase;

  ~MediaStreamInfoBarDelegateAndroid() override;

  // PermissionInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  Type GetInfoBarType() const override;
  int GetIconId() const override;
  void InfoBarDismissed() override;
  MediaStreamInfoBarDelegateAndroid* AsMediaStreamInfoBarDelegateAndroid()
      override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;
  int GetMessageResourceId() const override;
  std::vector<int> content_settings_types() const override;

  std::unique_ptr<MediaStreamDevicesController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_INFOBAR_DELEGATE_ANDROID_H_
