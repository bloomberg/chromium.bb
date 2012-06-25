// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_STREAM_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_MEDIA_STREAM_INFOBAR_DELEGATE_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/media/media_stream_devices_controller.h"

// This class configures an infobar shown when a page requests access to a
// user's microphone and/or video camera.  The user is shown a message asking
// which audio and/or video devices he wishes to use with the current page, and
// buttons to give access to the selected devices to the page, or to deny access
// to them.
class MediaStreamInfoBarDelegate : public InfoBarDelegate {
 public:
  // MediaStreamInfoBarDelegate takes the ownership of the |controller|.
  MediaStreamInfoBarDelegate(
      InfoBarTabHelper* tab_helper,
      MediaStreamDevicesController* controller);

  virtual ~MediaStreamInfoBarDelegate();

  // These tell whether the user has to select audio and/or video devices.
  bool HasAudio() const;
  bool HasVideo() const;

  // Returns lists of audio and/or video devices from which the user will have
  // to choose.
  content::MediaStreamDevices GetAudioDevices() const;
  content::MediaStreamDevices GetVideoDevices() const;

  // Returns the security origin (e.g. "www.html5rocks.com") at the origin
  // of this request.
  const GURL& GetSecurityOrigin() const;


  // Callbacks to handle accepting devices or denying the request. |audio_id|
  // and |video_id| are the device IDs of the accepted audio and video devices.
  // |always_allow| is true if the "always allow" option is checked.
  // The |audio_id| or |video_id| values are ignored if the request did not ask
  // for audio or video devices respectively.
  void Accept(const std::string& audio_id,
              const std::string& video_id,
              bool always_allow);
  void Deny();

 private:
  // InfoBarDelegate:
  virtual InfoBar* CreateInfoBar(InfoBarTabHelper* owner) OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual MediaStreamInfoBarDelegate* AsMediaStreamInfoBarDelegate() OVERRIDE;


 private:
  scoped_ptr<MediaStreamDevicesController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_MEDIA_STREAM_INFOBAR_DELEGATE_H_
