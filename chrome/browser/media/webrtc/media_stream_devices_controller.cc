// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"

#include <map>
#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_permission.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/origin_util.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include <vector>

#include "chrome/browser/android/preferences/pref_service_bridge.h"
#include "chrome/browser/media/webrtc/media_stream_infobar_delegate_android.h"
#include "chrome/browser/permissions/permission_dialog_delegate.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/android/content_view_core.h"
#include "ui/android/window_android.h"
#else  // !defined(OS_ANDROID)
#include "chrome/browser/permissions/permission_request_manager.h"
#include "ui/vector_icons/vector_icons.h"
#endif

using content::BrowserThread;

namespace {

// Returns true if the given ContentSettingsType is being requested in
// |request|.
bool ContentTypeIsRequested(ContentSettingsType type,
                            const content::MediaStreamRequest& request) {
  if (request.request_type == content::MEDIA_OPEN_DEVICE_PEPPER_ONLY)
    return true;

  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC)
    return request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE;

  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA)
    return request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE;

  return false;
}

using PermissionActionCallback =
    base::Callback<void(ContentSettingsType,
                        PermissionRequestGestureType,
                        const GURL&,
                        Profile*)>;

void RecordSinglePermissionAction(const content::MediaStreamRequest& request,
                                  ContentSettingsType content_type,
                                  Profile* profile,
                                  PermissionActionCallback callback) {
  if (ContentTypeIsRequested(content_type, request)) {
    // TODO(stefanocs): Pass the actual |gesture_type| once this file has been
    // refactored into PermissionContext.
    callback.Run(content_type, PermissionRequestGestureType::UNKNOWN,
                 request.security_origin, profile);
  }
}

// Calls |action_function| for each permission requested by |request|.
void RecordPermissionAction(const content::MediaStreamRequest& request,
                            Profile* profile,
                            PermissionActionCallback callback) {
  RecordSinglePermissionAction(request, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                               profile, callback);
  RecordSinglePermissionAction(
      request, CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, profile, callback);
}

#if defined(OS_ANDROID)
// Callback for the permission update infobar when the site and Chrome
// permissions are mismatched on Android.
void OnPermissionConflictResolved(
    std::unique_ptr<MediaStreamDevicesController> controller,
    bool allowed) {
  if (allowed)
    controller->PermissionGranted();
  else
    controller->ForcePermissionDeniedTemporarily();
}

#endif  // defined(OS_ANDROID)

// This helper class helps to measure the number of media stream requests that
// occur. It ensures that only one request will be recorded per navigation, per
// frame. TODO(raymes): Remove this when https://crbug.com/526324 is fixed.
class MediaPermissionRequestLogger : content::WebContentsObserver {
  // Map of <render process id, render frame id> ->
  // MediaPermissionRequestLogger.
  using RequestMap = std::map<std::pair<int, int>,
                              std::unique_ptr<MediaPermissionRequestLogger>>;

 public:
  static void LogRequest(content::WebContents* contents,
                         int render_process_id,
                         int render_frame_id,
                         bool is_secure) {
    RequestMap::key_type key =
        std::make_pair(render_process_id, render_frame_id);
    if (!base::ContainsKey(GetRequestMap(), key)) {
      UMA_HISTOGRAM_BOOLEAN("Pepper.SecureOrigin.MediaStreamRequest",
                            is_secure);
      GetRequestMap()[key] =
          base::WrapUnique(new MediaPermissionRequestLogger(contents, key));
    }
  }

 private:
  MediaPermissionRequestLogger(content::WebContents* contents,
                               RequestMap::key_type key)
      : WebContentsObserver(contents), key_(key) {}

  void PageChanged(content::RenderFrameHost* render_frame_host) {
    if (std::make_pair(render_frame_host->GetProcess()->GetID(),
                       render_frame_host->GetRoutingID()) == key_) {
      GetRequestMap().erase(key_);
    }
  }

  static RequestMap& GetRequestMap() {
    CR_DEFINE_STATIC_LOCAL(RequestMap, request_map, ());
    return request_map;
  }

  // content::WebContentsObserver overrides
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->HasCommitted())
      PageChanged(navigation_handle->GetRenderFrameHost());
  }

  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    PageChanged(render_frame_host);
  }

  RequestMap::key_type key_;
};

}  // namespace

// static
void MediaStreamDevicesController::RequestPermissions(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  std::unique_ptr<MediaStreamDevicesController> controller(
      new MediaStreamDevicesController(web_contents, request, callback));
  if (!controller->IsAskingForAudio() && !controller->IsAskingForVideo()) {
#if defined(OS_ANDROID)
    // If either audio or video was previously allowed and Chrome no longer has
    // the necessary permissions, show a infobar to attempt to address this
    // mismatch.
    std::vector<ContentSettingsType> content_settings_types;
    if (controller->IsAllowedForAudio())
      content_settings_types.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);

    if (controller->IsAllowedForVideo()) {
      content_settings_types.push_back(
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
    }
    if (!content_settings_types.empty() &&
        PermissionUpdateInfoBarDelegate::ShouldShowPermissionInfobar(
            web_contents, content_settings_types)) {
      PermissionUpdateInfoBarDelegate::Create(
          web_contents, content_settings_types,
          base::Bind(&OnPermissionConflictResolved, base::Passed(&controller)));
    }
#endif
    return;
  }

#if defined(OS_ANDROID)
  PermissionUmaUtil::RecordPermissionPromptShown(
      controller->GetPermissionRequestType(),
      PermissionUtil::GetGestureType(request.user_gesture));
  if (PermissionDialogDelegate::ShouldShowDialog(request.user_gesture)) {
    PermissionDialogDelegate::CreateMediaStreamDialog(
        web_contents, request.user_gesture, std::move(controller));
  } else {
    MediaStreamInfoBarDelegateAndroid::Create(
        web_contents, request.user_gesture, std::move(controller));
  }
#else
  PermissionRequestManager* permission_request_manager =
      PermissionRequestManager::FromWebContents(web_contents);
  if (permission_request_manager)
    permission_request_manager->AddRequest(controller.release());
#endif
}

// static
void MediaStreamDevicesController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kVideoCaptureAllowed, true);
  prefs->RegisterBooleanPref(prefs::kAudioCaptureAllowed, true);
  prefs->RegisterListPref(prefs::kVideoCaptureAllowedUrls);
  prefs->RegisterListPref(prefs::kAudioCaptureAllowedUrls);
}

MediaStreamDevicesController::~MediaStreamDevicesController() {
  if (!callback_.is_null()) {
    RecordPermissionAction(request_, profile_,
                           base::Bind(PermissionUmaUtil::PermissionIgnored));
    callback_.Run(content::MediaStreamDevices(),
                  content::MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN,
                  std::unique_ptr<content::MediaStreamUI>());
  }
}

bool MediaStreamDevicesController::IsAskingForAudio() const {
  return old_audio_setting_ == CONTENT_SETTING_ASK;
}

bool MediaStreamDevicesController::IsAskingForVideo() const {
  return old_video_setting_ == CONTENT_SETTING_ASK;
}

base::string16 MediaStreamDevicesController::GetMessageText() const {
  int message_id = IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO;
  if (!IsAskingForAudio())
    message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY;
  else if (!IsAskingForVideo())
    message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY;
  return l10n_util::GetStringFUTF16(
      message_id,
      url_formatter::FormatUrlForSecurityDisplay(
          GetOrigin(), url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}

void MediaStreamDevicesController::ForcePermissionDeniedTemporarily() {
  set_persist(false);
  // TODO(tsergeant): Determine whether it is appropriate to record permission
  // action metrics here, as this is a different sort of user action.
  RunCallback(CONTENT_SETTING_BLOCK,
              CONTENT_SETTING_BLOCK,
              content::MEDIA_DEVICE_PERMISSION_DENIED);
  set_persist(true);
}

PermissionRequest::IconId MediaStreamDevicesController::GetIconId() const {
#if defined(OS_ANDROID)
  return IsAskingForVideo() ? IDR_INFOBAR_MEDIA_STREAM_CAMERA
                            : IDR_INFOBAR_MEDIA_STREAM_MIC;
#else
  return IsAskingForVideo() ? ui::kVideocamIcon : ui::kMicrophoneIcon;
#endif
}

base::string16 MediaStreamDevicesController::GetMessageTextFragment() const {
  int message_id = IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO_PERMISSION_FRAGMENT;
  if (!IsAskingForAudio())
    message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_FRAGMENT;
  else if (!IsAskingForVideo())
    message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_FRAGMENT;
  return l10n_util::GetStringUTF16(message_id);
}

GURL MediaStreamDevicesController::GetOrigin() const {
  return request_.security_origin;
}

void MediaStreamDevicesController::PermissionGranted() {
  RecordPermissionAction(request_, profile_,
                         base::Bind(PermissionUmaUtil::PermissionGranted));
  RunCallback(GetNewSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                            old_audio_setting_, CONTENT_SETTING_ALLOW),
              GetNewSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                            old_video_setting_, CONTENT_SETTING_ALLOW),
              content::MEDIA_DEVICE_PERMISSION_DENIED);
}

void MediaStreamDevicesController::PermissionDenied() {
  RecordPermissionAction(request_, profile_,
                         base::Bind(PermissionUmaUtil::PermissionDenied));
  RunCallback(GetNewSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                            old_audio_setting_, CONTENT_SETTING_BLOCK),
              GetNewSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                            old_video_setting_, CONTENT_SETTING_BLOCK),
              content::MEDIA_DEVICE_PERMISSION_DENIED);
}

bool MediaStreamDevicesController::ShouldShowPersistenceToggle() const {
  return PermissionUtil::ShouldShowPersistenceToggle();
}

void MediaStreamDevicesController::Cancelled() {
  RecordPermissionAction(request_, profile_,
                         base::Bind(PermissionUmaUtil::PermissionDismissed));
  RunCallback(old_audio_setting_, old_video_setting_,
              content::MEDIA_DEVICE_PERMISSION_DISMISSED);
}

void MediaStreamDevicesController::RequestFinished() {
  delete this;
}

PermissionRequestType MediaStreamDevicesController::GetPermissionRequestType()
    const {
  return PermissionRequestType::MEDIA_STREAM;
}

MediaStreamDevicesController::MediaStreamDevicesController(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback)
    : web_contents_(web_contents), request_(request), callback_(callback) {
  if (request_.request_type == content::MEDIA_OPEN_DEVICE_PEPPER_ONLY) {
    MediaPermissionRequestLogger::LogRequest(
        web_contents, request.render_process_id, request.render_frame_id,
        content::IsOriginSecure(request_.security_origin));
  }
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  content_settings_ = TabSpecificContentSettings::FromWebContents(web_contents);

  content::MediaStreamRequestResult denial_reason = content::MEDIA_DEVICE_OK;
  old_audio_setting_ = GetContentSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                                         request_, &denial_reason);
  old_video_setting_ = GetContentSetting(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, request_, &denial_reason);

  // If either setting is ask, we show the infobar.
  if (old_audio_setting_ == CONTENT_SETTING_ASK ||
      old_video_setting_ == CONTENT_SETTING_ASK) {
    return;
  }

#if defined(OS_ANDROID)
  std::vector<ContentSettingsType> content_settings_types;
  if (IsAllowedForAudio())
    content_settings_types.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);

  if (IsAllowedForVideo()) {
    content_settings_types.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  }

  // If the site had been previously granted the access to audio or video but
  // Chrome is now missing the necessary permission, we need to show an infobar
  // to resolve the difference.
  if (!content_settings_types.empty() &&
      PermissionUpdateInfoBarDelegate::ShouldShowPermissionInfobar(
          web_contents, content_settings_types)) {
    return;
  }
#endif

  // Otherwise we can run the callback immediately.
  RunCallback(old_audio_setting_, old_video_setting_, denial_reason);
}

bool MediaStreamDevicesController::IsAllowedForAudio() const {
  return old_audio_setting_ == CONTENT_SETTING_ALLOW;
}

bool MediaStreamDevicesController::IsAllowedForVideo() const {
  return old_video_setting_ == CONTENT_SETTING_ALLOW;
}

content::MediaStreamDevices MediaStreamDevicesController::GetDevices(
    ContentSetting audio_setting,
    ContentSetting video_setting) {
  bool audio_allowed = audio_setting == CONTENT_SETTING_ALLOW;
  bool video_allowed = video_setting == CONTENT_SETTING_ALLOW;

  if (!audio_allowed && !video_allowed)
    return content::MediaStreamDevices();

  content::MediaStreamDevices devices;
  switch (request_.request_type) {
    case content::MEDIA_OPEN_DEVICE_PEPPER_ONLY: {
      const content::MediaStreamDevice* device = NULL;
      // For open device request, when requested device_id is empty, pick
      // the first available of the given type. If requested device_id is
      // not empty, return the desired device if it's available. Otherwise,
      // return no device.
      if (audio_allowed &&
          request_.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE) {
        DCHECK_EQ(content::MEDIA_NO_SERVICE, request_.video_type);
        if (!request_.requested_audio_device_id.empty()) {
          device =
              MediaCaptureDevicesDispatcher::GetInstance()
                  ->GetRequestedAudioDevice(request_.requested_audio_device_id);
        } else {
          device = MediaCaptureDevicesDispatcher::GetInstance()
                       ->GetFirstAvailableAudioDevice();
        }
      } else if (video_allowed &&
                 request_.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE) {
        DCHECK_EQ(content::MEDIA_NO_SERVICE, request_.audio_type);
        // Pepper API opens only one device at a time.
        if (!request_.requested_video_device_id.empty()) {
          device =
              MediaCaptureDevicesDispatcher::GetInstance()
                  ->GetRequestedVideoDevice(request_.requested_video_device_id);
        } else {
          device = MediaCaptureDevicesDispatcher::GetInstance()
                       ->GetFirstAvailableVideoDevice();
        }
      }
      if (device)
        devices.push_back(*device);
      break;
    }
    case content::MEDIA_GENERATE_STREAM: {
      bool get_default_audio_device = audio_allowed;
      bool get_default_video_device = video_allowed;

      // Get the exact audio or video device if an id is specified.
      if (audio_allowed && !request_.requested_audio_device_id.empty()) {
        const content::MediaStreamDevice* audio_device =
            MediaCaptureDevicesDispatcher::GetInstance()
                ->GetRequestedAudioDevice(request_.requested_audio_device_id);
        if (audio_device) {
          devices.push_back(*audio_device);
          get_default_audio_device = false;
        }
      }
      if (video_allowed && !request_.requested_video_device_id.empty()) {
        const content::MediaStreamDevice* video_device =
            MediaCaptureDevicesDispatcher::GetInstance()
                ->GetRequestedVideoDevice(request_.requested_video_device_id);
        if (video_device) {
          devices.push_back(*video_device);
          get_default_video_device = false;
        }
      }

      // If either or both audio and video devices were requested but not
      // specified by id, get the default devices.
      if (get_default_audio_device || get_default_video_device) {
        MediaCaptureDevicesDispatcher::GetInstance()
            ->GetDefaultDevicesForProfile(profile_, get_default_audio_device,
                                          get_default_video_device, &devices);
      }
      break;
    }
    case content::MEDIA_DEVICE_ACCESS: {
      // Get the default devices for the request.
      MediaCaptureDevicesDispatcher::GetInstance()->GetDefaultDevicesForProfile(
          profile_, audio_allowed, video_allowed, &devices);
      break;
    }
  }  // switch

  return devices;
}

void MediaStreamDevicesController::RunCallback(
    ContentSetting audio_setting,
    ContentSetting video_setting,
    content::MediaStreamRequestResult denial_reason) {
  CHECK(!callback_.is_null());

  // If the kill switch is on we don't update the tab context or persist the
  // setting.
  if (denial_reason != content::MEDIA_DEVICE_KILL_SWITCH_ON) {
    if (persist())
      StorePermission(audio_setting, video_setting);
    UpdateTabSpecificContentSettings(audio_setting, video_setting);
  }

  content::MediaStreamDevices devices =
      GetDevices(audio_setting, video_setting);

  // If either audio or video are allowed then the callback should report
  // success, otherwise we report |denial_reason|.
  content::MediaStreamRequestResult request_result = content::MEDIA_DEVICE_OK;
  if (audio_setting != CONTENT_SETTING_ALLOW &&
      video_setting != CONTENT_SETTING_ALLOW) {
    DCHECK_NE(content::MEDIA_DEVICE_OK, denial_reason);
    request_result = denial_reason;
  } else if (devices.empty()) {
    // Even if one of the content settings was allowed, if there are no devices
    // at this point we still report a failure.
    request_result = content::MEDIA_DEVICE_NO_HARDWARE;
  }

  std::unique_ptr<content::MediaStreamUI> ui;
  if (!devices.empty()) {
    ui = MediaCaptureDevicesDispatcher::GetInstance()
             ->GetMediaStreamCaptureIndicator()
             ->RegisterMediaStream(web_contents_, devices);
  }
  base::ResetAndReturn(&callback_).Run(devices, request_result, std::move(ui));
}

void MediaStreamDevicesController::StorePermission(
    ContentSetting new_audio_setting,
    ContentSetting new_video_setting) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(content::IsOriginSecure(request_.security_origin) ||
         request_.request_type == content::MEDIA_OPEN_DEVICE_PEPPER_ONLY);

  if (IsAskingForAudio() && new_audio_setting != CONTENT_SETTING_ASK) {
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->SetContentSettingDefaultScope(request_.security_origin, GURL(),
                                        CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                                        std::string(), new_audio_setting);
  }
  if (IsAskingForVideo() && new_video_setting != CONTENT_SETTING_ASK) {
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->SetContentSettingDefaultScope(
            request_.security_origin, GURL(),
            CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, std::string(),
            new_video_setting);
  }
}

void MediaStreamDevicesController::UpdateTabSpecificContentSettings(
    ContentSetting audio_setting,
    ContentSetting video_setting) const {
  if (!content_settings_)
    return;

  TabSpecificContentSettings::MicrophoneCameraState microphone_camera_state =
      TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED;
  std::string selected_audio_device;
  std::string selected_video_device;
  std::string requested_audio_device = request_.requested_audio_device_id;
  std::string requested_video_device = request_.requested_video_device_id;

  // TODO(raymes): Why do we use the defaults here for the selected devices?
  // Shouldn't we just use the devices that were actually selected?
  PrefService* prefs = Profile::FromBrowserContext(
                           web_contents_->GetBrowserContext())->GetPrefs();
  if (audio_setting != CONTENT_SETTING_DEFAULT) {
    selected_audio_device =
        requested_audio_device.empty()
            ? prefs->GetString(prefs::kDefaultAudioCaptureDevice)
            : requested_audio_device;
    microphone_camera_state |=
        TabSpecificContentSettings::MICROPHONE_ACCESSED |
        (audio_setting == CONTENT_SETTING_ALLOW
             ? 0
             : TabSpecificContentSettings::MICROPHONE_BLOCKED);
  }

  if (video_setting != CONTENT_SETTING_DEFAULT) {
    selected_video_device =
        requested_video_device.empty()
            ? prefs->GetString(prefs::kDefaultVideoCaptureDevice)
            : requested_video_device;
    microphone_camera_state |=
        TabSpecificContentSettings::CAMERA_ACCESSED |
        (video_setting == CONTENT_SETTING_ALLOW
             ? 0
             : TabSpecificContentSettings::CAMERA_BLOCKED);
  }

  content_settings_->OnMediaStreamPermissionSet(
      request_.security_origin, microphone_camera_state, selected_audio_device,
      selected_video_device, requested_audio_device, requested_video_device);
}

ContentSetting MediaStreamDevicesController::GetContentSetting(
    ContentSettingsType content_type,
    const content::MediaStreamRequest& request,
    content::MediaStreamRequestResult* denial_reason) const {
  DCHECK(content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
         content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);

  std::string requested_device_id;
  if (content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC)
    requested_device_id = request.requested_audio_device_id;
  else if (content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA)
    requested_device_id = request.requested_video_device_id;

  if (!IsUserAcceptAllowed(content_type)) {
    *denial_reason = content::MEDIA_DEVICE_PERMISSION_DENIED;
    return CONTENT_SETTING_BLOCK;
  }

  if (ContentTypeIsRequested(content_type, request)) {
    DCHECK(content::IsOriginSecure(request_.security_origin) ||
           request_.request_type == content::MEDIA_OPEN_DEVICE_PEPPER_ONLY);
    MediaPermission permission(content_type, request.security_origin,
                               web_contents_->GetLastCommittedURL().GetOrigin(),
                               profile_, web_contents_);
    return permission.GetPermissionStatusWithDeviceRequired(requested_device_id,
                                                            denial_reason);
  }
  // Return the default content setting if the device is not requested.
  return CONTENT_SETTING_DEFAULT;
}

ContentSetting MediaStreamDevicesController::GetNewSetting(
    ContentSettingsType content_type,
    ContentSetting old_setting,
    ContentSetting user_decision) const {
  DCHECK(user_decision == CONTENT_SETTING_ALLOW ||
         user_decision == CONTENT_SETTING_BLOCK);
  ContentSetting result = old_setting;
  if (old_setting == CONTENT_SETTING_ASK) {
    if (user_decision == CONTENT_SETTING_ALLOW &&
        IsUserAcceptAllowed(content_type)) {
      result = CONTENT_SETTING_ALLOW;
    } else if (user_decision == CONTENT_SETTING_BLOCK) {
      result = CONTENT_SETTING_BLOCK;
    }
  }
  return result;
}

bool MediaStreamDevicesController::IsUserAcceptAllowed(
    ContentSettingsType content_type) const {
#if defined(OS_ANDROID)
  content::ContentViewCore* cvc =
      content::ContentViewCore::FromWebContents(web_contents_);
  if (!cvc)
    return false;

  ui::WindowAndroid* window_android = cvc->GetWindowAndroid();
  if (!window_android)
    return false;

  std::vector<std::string> android_permissions;
  PrefServiceBridge::GetAndroidPermissionsForContentSetting(
      content_type, &android_permissions);
  for (const auto& android_permission : android_permissions) {
    if (!window_android->HasPermission(android_permission) &&
        !window_android->CanRequestPermission(android_permission)) {
      return false;
    }
  }

  // Don't approve device requests if the tab was hidden.
  // TODO(qinmin): Add a test for this. http://crbug.com/396869.
  // TODO(raymes): Shouldn't this apply to all permissions not just audio/video?
  return web_contents_->GetRenderWidgetHostView()->IsShowing();
#endif
  return true;
}
