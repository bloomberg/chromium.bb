// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_

#include <map>
#include <string>

#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "content/public/browser/web_contents_delegate.h"

class Profile;
class TabSpecificContentSettings;

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class MediaStreamDevicesController : public PermissionBubbleRequest {
 public:
  // Permissions for media stream types.
  enum Permission {
    MEDIA_NONE,
    MEDIA_ALLOWED,
    MEDIA_BLOCKED_BY_POLICY,
    MEDIA_BLOCKED_BY_USER_SETTING,
    MEDIA_BLOCKED_BY_USER,
  };

  struct MediaStreamTypeSettings {
    MediaStreamTypeSettings(Permission permission,
                            const std::string& requested_device_id);
    MediaStreamTypeSettings();
    ~MediaStreamTypeSettings();

    Permission permission;
    std::string requested_device_id;
  };

  typedef std::map<content::MediaStreamType, MediaStreamTypeSettings>
      MediaStreamTypeSettingsMap;

  MediaStreamDevicesController(content::WebContents* web_contents,
                               const content::MediaStreamRequest& request,
                               const content::MediaResponseCallback& callback);

  virtual ~MediaStreamDevicesController();

  // TODO(tommi): Clean up all the policy code and integrate with
  // HostContentSettingsMap instead.  This will make creating the UI simpler
  // and the code cleaner.  crbug.com/244389.

  // Registers the prefs backing the audio and video policies.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Public method to be called before creating the MediaStreamInfoBarDelegate.
  // This function will check the content settings exceptions and take the
  // corresponding action on exception which matches the request.
  bool DismissInfoBarAndTakeActionOnSettings();

  // Public methods to be called by MediaStreamInfoBarDelegate;
  bool HasAudio() const;
  bool HasVideo() const;
  const std::string& GetSecurityOriginSpec() const;
  void Accept(bool update_content_setting);
  void Deny(bool update_content_setting,
            content::MediaStreamRequestResult result);

  // PermissionBubbleRequest:
  virtual int GetIconID() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetMessageTextFragment() const OVERRIDE;
  virtual bool HasUserGesture() const OVERRIDE;
  virtual GURL GetRequestingHostname() const OVERRIDE;
  virtual void PermissionGranted() OVERRIDE;
  virtual void PermissionDenied() OVERRIDE;
  virtual void Cancelled() OVERRIDE;
  virtual void RequestFinished() OVERRIDE;

 private:
  enum DevicePolicy {
    POLICY_NOT_SET,
    ALWAYS_DENY,
    ALWAYS_ALLOW,
  };

  // Called by GetAudioDevicePolicy and GetVideoDevicePolicy to check
  // the currently set capture device policy.
  DevicePolicy GetDevicePolicy(const char* policy_name,
                               const char* whitelist_policy_name) const;

  // Returns true if the origin of the request has been granted the media
  // access before, otherwise returns false.
  bool IsRequestAllowedByDefault() const;

  // Check if any device of the request has been blocked for the origin of the
  // request and clears |microphone_requested_| or |webcam_requested_| flags if
  // they are not allowed anymore. Returns the number of devices that are
  // allowed after this step. If the count reaches zero the request can be
  // denied completely, else it still has to be partially fullfilled.
  int FilterBlockedByDefaultDevices();

  // Returns true if the media section in content settings is set to
  // |CONTENT_SETTING_BLOCK|, otherwise returns false.
  bool IsDefaultMediaAccessBlocked() const;

  // Returns true if the origin is a secure scheme, otherwise returns false.
  bool IsSchemeSecure() const;

  // Returns true if request's origin is from internal objects like
  // chrome://URLs, otherwise returns false.
  bool ShouldAlwaysAllowOrigin() const;

  // Sets the permission of the origin of the request. This is triggered when
  // the users deny the request or allow the request for https sites.
  void SetPermission(bool allowed) const;

  // Notifies the content setting UI that the media stream access request or
  // part of the request is accepted.
  void NotifyUIRequestAccepted() const;

  // Notifies the content setting UI that the media stream access request or
  // part of the request is denied.
  void NotifyUIRequestDenied();

  // Return true if the type has been requested and permission is currently set
  // to allowed. Note that it does not reflect the final permission decision.
  // This function is called during the filtering steps to check if the type has
  // been blocked yet or not and the permission may be changed to blocked during
  // these filterings. See also the initialization in the constructor and
  // comments on that.
  bool IsDeviceAudioCaptureRequestedAndAllowed() const;
  bool IsDeviceVideoCaptureRequestedAndAllowed() const;

  content::WebContents* web_contents_;

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

  // Holds the requested media types and the permission for each type. It is
  // passed to the tab specific content settings when the permissions have been
  // resolved. Currently only used by MEDIA_DEVICE_AUDIO_CAPTURE and
  // MEDIA_DEVICE_VIDEO_CAPTURE since those are the only types that require
  // updates in the settings.
  MediaStreamTypeSettingsMap request_permissions_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDevicesController);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STREAM_DEVICES_CONTROLLER_H_
