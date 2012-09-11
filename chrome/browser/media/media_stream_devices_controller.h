// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_

#include <string>

#include "content/public/browser/web_contents_delegate.h"

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
  const std::string& GetSecurityOriginSpec() const;
  content::MediaStreamDevices GetAudioDevices() const;
  content::MediaStreamDevices GetVideoDevices() const;
  bool IsSafeToAlwaysAllowAudio() const;
  bool IsSafeToAlwaysAllowVideo() const;
  void Accept(const std::string& audio_id,
              const std::string& video_id,
              bool always_allow);
  void Deny();

 private:
  // Used by the various helper methods below to filter an operation on devices
  // of a particular type.
  typedef bool (*FilterByDeviceTypeFunc)(content::MediaStreamDeviceType);

  // Returns true if a secure scheme is being used by the origin AND only
  // devices of the given physical |device_type| are present in the subset of
  // devices selected by the |is_included| function.
  bool IsSafeToAlwaysAllow(FilterByDeviceTypeFunc is_included,
                           content::MediaStreamDeviceType device_type) const;

  // Returns true if the media section in content settings is set to
  // |CONTENT_SETTING_BLOCK|, otherwise returns false.
  bool IsMediaDeviceBlocked();

  // NOTE on AlwaysAllowOrigin functionality: The rules only apply to physical
  // capture devices, and not tab mirroring (or other "virtual device" types).
  // Virtual devices are always denied an AlwaysAllowOrigin status because they
  // refer to internal objects whose "IDs" might be re-used for different
  // objects across browser sessions.

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

  // Copies all devices passing the |is_included| predicate to the given output
  // container.
  void FindSubsetOfDevices(FilterByDeviceTypeFunc is_included,
                           content::MediaStreamDevices* out) const;

  // Finds the first device with the given |device_id| within the subset of
  // devices passing the |is_included| predicate, or return NULL.
  const content::MediaStreamDevice* FindFirstDeviceWithIdInSubset(
      FilterByDeviceTypeFunc is_included,
      const std::string& device_id) const;

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
