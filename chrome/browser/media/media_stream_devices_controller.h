// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_

#include <string>

#include "chrome/common/content_settings.h"
#include "content/public/browser/web_contents_delegate.h"

class PrefRegistrySyncable;
class Profile;
class TabSpecificContentSettings;

class MediaStreamDevicesController {
 public:
  MediaStreamDevicesController(Profile* profile,
                               TabSpecificContentSettings* content_settings,
                               const content::MediaStreamRequest& request,
                               const content::MediaResponseCallback& callback);

  virtual ~MediaStreamDevicesController();

  // Registers the prefs backing the audio and video policies.
  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

  // Called before creating the MediaStreamInfoBarDelegate. Processes the media
  // stream request and returns true if the request could have been completed
  // based on the current permission settings (content settings). Returns false
  // if the user must be asked for permission.
  bool ProcessRequest();

  // Public methods to be called by MediaStreamInfoBarDelegate;

  // Returns true if the user has to be prompted for microphone stream access.
  bool IsMicrophoneRequested() const;

  // Returns true if the user has to be prompted for camera stream access.
  bool IsCameraRequested() const;

  // Returns the origin of the media stream requests.
  const GURL& GetRequestOrigin() const;

  // Called after the user has granted the requested media stream permissions.
  void OnGrantPermission();

  // Called after the user has denied the requested media stream permissions.
  void OnDenyPermission();

  // Called after the user has cancled the media stream permission request.
  void OnCancel();

 private:
  enum MediaAccess {
    MEDIA_ACCESS_NOT_REQUESTED = 0,
    MEDIA_ACCESS_REQUESTED,
    MEDIA_ACCESS_ALLOWED,
    MEDIA_ACCESS_BLOCKED,
    MEDIA_ACCESS_NO_DEVICE
  };

  // Process screen capture requests.
  bool ProcessScreenCaptureRequest();

  // Process microphone and camera requests.
  bool ProcessMicrophoneCameraRequest();

  // Completes processing the media stream request. This method is called if the
  // media stream request can be completed based on the existing permission
  // setting or after the user granted or denied the appropriate permission.
  void CompleteProcessingRequest();

  // Maps a content |setting| to media stream access value.
  MediaAccess ContentSettingToMediaAccess(ContentSetting setting) const;

  // The owner of this class needs to make sure it does not outlive the profile.
  Profile* profile_;

  // Weak pointer to the tab specific content settings of the tab for which the
  // MediaStreamDevicesController was created. The tab specific content
  // settings are associated with a the web contents of the tab. The
  // MediaStreamDeviceController must not outlive the web contents for which it
  // was created.
  TabSpecificContentSettings* content_settings_;

  // The original request for access to devices.
  const content::MediaStreamRequest request_;

  // The callback that needs to be Run to notify WebRTC of whether access to
  // audio/video devices was granted or not.
  content::MediaResponseCallback callback_;

  // Contains the microphone access state.
  MediaAccess microphone_access_;

  // Contains the camera access state.
  MediaAccess camera_access_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDevicesController);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_
