// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_

#include <string>

#include "content/public/browser/web_contents_delegate.h"

class PrefServiceSyncable;
class Profile;

class MediaStreamDevicesController {
 public:
  MediaStreamDevicesController(Profile* profile,
                               const content::MediaStreamRequest& request,
                               const content::MediaResponseCallback& callback);

  virtual ~MediaStreamDevicesController();

  // Registers the prefs backing the audio and video policies.
  static void RegisterUserPrefs(PrefServiceSyncable* prefs);

  // Public method to be called before creating the MediaStreamInfoBarDelegate.
  // This function will check the content settings exceptions and take the
  // corresponding action on exception which matches the request.
  bool DismissInfoBarAndTakeActionOnSettings();

  // Public methods to be called by MediaStreamInfoBarDelegate;
  bool has_audio() const { return has_audio_; }
  bool has_video() const { return has_video_; }
  const std::string& GetSecurityOriginSpec() const;
  void Accept(bool update_content_setting);
  void Deny(bool update_content_setting);

 private:
  // Returns true if audio capture is disabled by policy.
  bool IsAudioDeviceBlockedByPolicy() const;

  // Returns true if video capture is disabled by policy.
  bool IsVideoDeviceBlockedByPolicy() const;

  // Returns true if the origin of the request has been granted the media
  // access before, otherwise returns false.
  bool IsRequestAllowedByDefault() const;

  // Returns true if the media access for the origin of the request has been
  // blocked before. Otherwise returns false.
  bool IsRequestBlockedByDefault() const;

  // Returns true if the media section in content settings is set to
  // |CONTENT_SETTING_BLOCK|, otherwise returns false.
  bool IsDefaultMediaAccessBlocked() const;

  // Handles Tab Capture media request.
  void HandleTapMediaRequest();

  // Returns true if the origin is a secure scheme, otherwise returns false.
  bool IsSchemeSecure() const;

  // Returns true if request's origin is from internal objects like
  // chrome://URLs, otherwise returns false.
  bool ShouldAlwaysAllowOrigin() const;

  // Sets the permission of the origin of the request. This is triggered when
  // the users deny the request or allow the request for https sites.
  void SetPermission(bool allowed) const;

  // The owner of this class needs to make sure it does not outlive the profile.
  Profile* profile_;

  // The original request for access to devices.
  const content::MediaStreamRequest request_;

  // The callback that needs to be Run to notify WebRTC of whether access to
  // audio/video devices was granted or not.
  content::MediaResponseCallback callback_;

  bool has_audio_;
  bool has_video_;
  DISALLOW_COPY_AND_ASSIGN(MediaStreamDevicesController);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_
