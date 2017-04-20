// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICES_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICES_CONTROLLER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "chrome/browser/permissions/permission_request.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/web_contents_delegate.h"

class MediaStreamDevicesController;
class Profile;
class TabSpecificContentSettings;

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {
class MediaStreamDevicesControllerBrowserTest;
}

namespace test {
class MediaStreamDevicesControllerTestApi;
}

class MediaStreamDevicesController {
 public:
  // This class is only needed until we unify the codepaths for permission
  // requests. It can be removed once crbug.com/606138 is fixed.
  class Request : public PermissionRequest {
   public:
    using PromptAnsweredCallback =
        base::Callback<void(ContentSetting, bool /* persist */)>;

    Request(Profile* profile,
            bool is_asking_for_audio,
            bool is_asking_for_video,
            const GURL& security_origin,
            PromptAnsweredCallback prompt_answered_callback);

    ~Request() override;

    bool IsAskingForAudio() const;
    bool IsAskingForVideo() const;
    base::string16 GetMessageText() const;

    // PermissionRequest:
    IconId GetIconId() const override;
    base::string16 GetMessageTextFragment() const override;
    GURL GetOrigin() const override;
    void PermissionGranted() override;
    void PermissionDenied() override;
    void Cancelled() override;
    void RequestFinished() override;
    PermissionRequestType GetPermissionRequestType() const override;
    bool ShouldShowPersistenceToggle() const override;

   private:
    Profile* profile_;

    bool is_asking_for_audio_;
    bool is_asking_for_video_;

    GURL security_origin_;

    PromptAnsweredCallback prompt_answered_callback_;

    // Whether the prompt has been answered.
    bool responded_;

    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  // This class is only needed interally and for tests. It can be removed once
  // crbug.com/606138 is fixed. Delegate for showing permission prompts. It's
  // only public because subclassing from a friend class doesn't work in gcc
  // (see https://codereview.chromium.org/2768923003).
  class PermissionPromptDelegate {
   public:
    virtual void ShowPrompt(
        bool user_gesture,
        content::WebContents* web_contents,
        std::unique_ptr<MediaStreamDevicesController::Request> request) = 0;
  };

  static void RequestPermissions(
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback);

  // Registers the prefs backing the audio and video policies.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  ~MediaStreamDevicesController();

  bool IsAskingForAudio() const;
  bool IsAskingForVideo() const;

  // Called when a permission prompt has been answered, with the |response| and
  // whether the choice should be persisted.
  void PromptAnswered(ContentSetting response, bool persist);

#if defined(OS_ANDROID)
  // Called when the Android OS-level prompt is answered.
  void AndroidOSPromptAnswered(bool allowed);
#endif  // defined(OS_ANDROID)

 private:
  friend class MediaStreamDevicesControllerTest;
  friend class test::MediaStreamDevicesControllerTestApi;
  friend class policy::MediaStreamDevicesControllerBrowserTest;

  class PermissionPromptDelegateImpl;

  static void RequestPermissionsWithDelegate(
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      PermissionPromptDelegate* delegate);

  MediaStreamDevicesController(content::WebContents* web_contents,
                               const content::MediaStreamRequest& request,
                               const content::MediaResponseCallback& callback);

  bool IsAllowedForAudio() const;
  bool IsAllowedForVideo() const;

  // Returns a list of devices available for the request for the given
  // audio/video permission settings.
  content::MediaStreamDevices GetDevices(ContentSetting audio_setting,
                                         ContentSetting video_setting);

  // Runs |callback_| with the given audio/video permission settings. If neither
  // |audio_setting| or |video_setting| is set to allow, |denial_reason| should
  // be set to the error to be reported when running |callback_|.
  void RunCallback(ContentSetting audio_setting,
                   ContentSetting video_setting,
                   content::MediaStreamRequestResult denial_reason);

  // Store the permission to use media devices for the origin of the request.
  // This is triggered when the user makes a decision.
  void StorePermission(ContentSetting new_audio_setting,
                       ContentSetting new_video_setting) const;

  // Called when the permission has been set to update the
  // TabSpecificContentSettings.
  void UpdateTabSpecificContentSettings(ContentSetting audio_setting,
                                        ContentSetting video_setting) const;

  // Returns the content settings for the given content type and request.
  ContentSetting GetContentSetting(
      ContentSettingsType content_type,
      const content::MediaStreamRequest& request,
      content::MediaStreamRequestResult* denial_reason) const;

  // Returns the content setting that should apply given an old content setting
  // and a user decision that has been made. If a user isn't being asked for one
  // of audio/video then we shouldn't change that setting, even if they accept
  // the dialog.
  ContentSetting GetNewSetting(ContentSettingsType content_type,
                               ContentSetting old_setting,
                               ContentSetting user_decision) const;

  // Returns true if clicking allow on the dialog should give access to the
  // requested devices.
  bool IsUserAcceptAllowed(ContentSettingsType content_type) const;

  // The audio/video content settings BEFORE the user clicks accept/deny.
  ContentSetting old_audio_setting_;
  ContentSetting old_video_setting_;

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

  std::unique_ptr<PermissionPromptDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDevicesController);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_DEVICES_CONTROLLER_H_
