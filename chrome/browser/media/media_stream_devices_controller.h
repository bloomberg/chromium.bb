// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_
#pragma once

#include <string>

#include "content/public/browser/web_contents_delegate.h"

class GURL;
class Profile;

class MediaStreamDevicesController {
 public:
  // TODO(xians): Use const content::MediaStreamRequest& instead of *.
  MediaStreamDevicesController(Profile* profile,
                               const content::MediaStreamRequest* request,
                               const content::MediaResponseCallback& callback);

  virtual ~MediaStreamDevicesController();

  // Public method to be called before creating the MediaStreamInfoBarDelegate.
  // This function will check the content settings exceptions and take the
  // corresponding action on exception which matches the request.
  bool DismissInfoBarAndTakeActionOnSettings();

  // Public methods to be called by MediaStreamInfoBarDelegate;
  bool has_audio() const { return has_audio_; }
  bool has_video() const { return has_video_; }
  content::MediaStreamDevices GetAudioDevices() const;
  content::MediaStreamDevices GetVideoDevices() const;
  const GURL& GetSecurityOrigin() const;
  void Accept(const std::string& audio_id,
              const std::string& video_id,
              bool always_allow);
  void Deny();

 private:
  // Finds a device in the current request with the specified |id| and |type|,
  // adds it to the |devices| array and also return the name of the device.
  void AddDeviceWithId(content::MediaStreamDeviceType type,
                       const std::string& id,
                       content::MediaStreamDevices* devices,
                       std::string* device_name);

  // Returns true if the media section in content settings is set to
  // |CONTENT_SETTING_BLOCK|, otherwise returns false.
  bool IsMediaDeviceBlocked();

  // Returns true if request's origin is from internal objects like
  // chrome://URLs, otherwise returns false.
  bool ShouldAlwaysAllowOrigin();

  // Grants "always allow" exception for the origin to use the selected devices.
  void AlwaysAllowOriginAndDevices(const std::string& audio_device,
                                   const std::string& video_device);

  // Gets the respective "always allowed" devices for the origin in |request_|.
  // |audio_id| and |video_id| will be empty if there is no "always allowed"
  // device for the origin, or any of the devices is not listed on the devices
  // list in |request_|.
  void GetAlwaysAllowedDevices(std::string* audio_id,
                               std::string* video_id);

  std::string GetDeviceIdByName(content::MediaStreamDeviceType type,
                                const std::string& name);

  std::string GetFirstDeviceId(content::MediaStreamDeviceType type);

  bool has_audio_;
  bool has_video_;

  // The owner of this class needs to make sure it does not outlive the profile.
  Profile* profile_;

  // The original request for access to devices.
  const content::MediaStreamRequest request_;

  // The callback that needs to be Run to notify WebRTC of whether access to
  // audio/video devices was granted or not.
  content::MediaResponseCallback callback_;
  DISALLOW_COPY_AND_ASSIGN(MediaStreamDevicesController);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_
