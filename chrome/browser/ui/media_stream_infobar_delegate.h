// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_STREAM_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_MEDIA_STREAM_INFOBAR_DELEGATE_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/api/infobars/infobar_delegate.h"
#include "chrome/browser/media/media_stream_devices_controller.h"

class InfoBarTabHelper;
class InfoBarService;

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
  const std::string& GetSecurityOriginSpec() const;

  void set_selected_audio_device(const std::string& device_id) {
    selected_audio_device_ = device_id;
  }
  void set_selected_video_device(const std::string& device_id) {
    selected_video_device_ = device_id;
  }
  void toggle_always_allow() { always_allow_ = !always_allow_; }

  const std::string& selected_audio_device() const {
    return selected_audio_device_;
  }
  const std::string& selected_video_device() const {
    return selected_video_device_;
  }
  bool always_allow() const { return always_allow_; }

  // These determine whether all audio (or video) devices can be auto-accepted
  // in the future should the user accept them this time.
  bool IsSafeToAlwaysAllowAudio() const;
  bool IsSafeToAlwaysAllowVideo() const;

  // Callbacks to handle accepting devices or denying the request.
  void Accept();
  void Deny();

 private:
  // InfoBarDelegate:
  virtual InfoBar* CreateInfoBar(InfoBarService* owner) OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual MediaStreamInfoBarDelegate* AsMediaStreamInfoBarDelegate() OVERRIDE;


 private:
  scoped_ptr<MediaStreamDevicesController> controller_;

  std::string selected_audio_device_;
  std::string selected_video_device_;
  bool always_allow_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_MEDIA_STREAM_INFOBAR_DELEGATE_H_
