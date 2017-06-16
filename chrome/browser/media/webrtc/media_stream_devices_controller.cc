// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"

#include <map>
#include <utility>

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
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

// Calls |action_callback| for each permission requested.
void RecordPermissionAction(bool is_asking_for_audio,
                            bool is_asking_for_video,
                            const GURL& security_origin,
                            Profile* profile,
                            PermissionActionCallback action_callback) {
  // TODO(stefanocs): Pass the actual |gesture_type| once this file has been
  // refactored into PermissionContext.
  if (is_asking_for_audio) {
    action_callback.Run(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                        PermissionRequestGestureType::UNKNOWN, security_origin,
                        profile);
  }
  if (is_asking_for_video) {
    action_callback.Run(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                        PermissionRequestGestureType::UNKNOWN, security_origin,
                        profile);
  }
}

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

bool HasAvailableDevices(ContentSettingsType content_type,
                         const std::string& device_id) {
  const content::MediaStreamDevices* devices = nullptr;
  if (content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
    devices =
        &MediaCaptureDevicesDispatcher::GetInstance()->GetAudioCaptureDevices();
  } else if (content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    devices =
        &MediaCaptureDevicesDispatcher::GetInstance()->GetVideoCaptureDevices();
  } else {
    NOTREACHED();
  }

  // TODO(tommi): It's kind of strange to have this here since if we fail this
  // test, there'll be a UI shown that indicates to the user that access to
  // non-existing audio/video devices has been denied.  The user won't have
  // any way to change that but there will be a UI shown which indicates that
  // access is blocked.
  if (devices->empty())
    return false;

  // Note: we check device_id before dereferencing devices. If the requested
  // device id is non-empty, then the corresponding device list must not be
  // NULL.
  if (!device_id.empty() && !devices->FindById(device_id))
    return false;

  return true;
}

}  // namespace

MediaStreamDevicesController::Request::Request(
    Profile* profile,
    bool is_asking_for_audio,
    bool is_asking_for_video,
    const GURL& security_origin,
    PromptAnsweredCallback prompt_answered_callback)
    : profile_(profile),
      is_asking_for_audio_(is_asking_for_audio),
      is_asking_for_video_(is_asking_for_video),
      security_origin_(security_origin),
      prompt_answered_callback_(prompt_answered_callback),
      responded_(false) {}

MediaStreamDevicesController::Request::~Request() {
  if (!responded_) {
    RecordPermissionAction(is_asking_for_audio_, is_asking_for_video_,
                           security_origin_, profile_,
                           base::Bind(PermissionUmaUtil::PermissionIgnored));
  }
}

bool MediaStreamDevicesController::Request::IsAskingForAudio() const {
  return is_asking_for_audio_;
}

bool MediaStreamDevicesController::Request::IsAskingForVideo() const {
  return is_asking_for_video_;
}

PermissionRequest::IconId MediaStreamDevicesController::Request::GetIconId()
    const {
#if defined(OS_ANDROID)
  return IsAskingForVideo() ? IDR_INFOBAR_MEDIA_STREAM_CAMERA
                            : IDR_INFOBAR_MEDIA_STREAM_MIC;
#else
  return IsAskingForVideo() ? ui::kVideocamIcon : ui::kMicrophoneIcon;
#endif
}

#if defined(OS_ANDROID)
base::string16 MediaStreamDevicesController::Request::GetMessageText() const {
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
#endif

base::string16 MediaStreamDevicesController::Request::GetMessageTextFragment()
    const {
  int message_id = IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO_PERMISSION_FRAGMENT;
  if (!IsAskingForAudio())
    message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_FRAGMENT;
  else if (!IsAskingForVideo())
    message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_FRAGMENT;
  return l10n_util::GetStringUTF16(message_id);
}

GURL MediaStreamDevicesController::Request::GetOrigin() const {
  return security_origin_;
}

void MediaStreamDevicesController::Request::PermissionGranted() {
  RecordPermissionAction(is_asking_for_audio_, is_asking_for_video_,
                         security_origin_, profile_,
                         base::Bind(PermissionUmaUtil::PermissionGranted));
  responded_ = true;
  prompt_answered_callback_.Run(CONTENT_SETTING_ALLOW, persist());
}

void MediaStreamDevicesController::Request::PermissionDenied() {
  RecordPermissionAction(is_asking_for_audio_, is_asking_for_video_,
                         security_origin_, profile_,
                         base::Bind(PermissionUmaUtil::PermissionDenied));
  responded_ = true;
  prompt_answered_callback_.Run(CONTENT_SETTING_BLOCK, persist());
}

void MediaStreamDevicesController::Request::Cancelled() {
  RecordPermissionAction(is_asking_for_audio_, is_asking_for_video_,
                         security_origin_, profile_,
                         base::Bind(PermissionUmaUtil::PermissionDismissed));
  responded_ = true;
  prompt_answered_callback_.Run(CONTENT_SETTING_ASK, false /* persist */);
}

void MediaStreamDevicesController::Request::RequestFinished() {
  delete this;
}

PermissionRequestType
MediaStreamDevicesController::Request::GetPermissionRequestType() const {
  return PermissionRequestType::MEDIA_STREAM;
}

bool MediaStreamDevicesController::Request::ShouldShowPersistenceToggle()
    const {
  // Camera and Mic are handled the same w.r.t. showing the persistence toggle,
  // just check camera here for simplicity (this class will be removed once the
  // Android and media refactorings are finished).
  return PermissionUtil::ShouldShowPersistenceToggle(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
}

// Implementation of PermissionPromptDelegate which actually shows a permission
// prompt.
class MediaStreamDevicesController::PermissionPromptDelegateImpl
    : public MediaStreamDevicesController::PermissionPromptDelegate {
 public:
  void ShowPrompt(
      bool user_gesture,
      content::WebContents* web_contents,
      std::unique_ptr<MediaStreamDevicesController::Request> request) override {
#if defined(OS_ANDROID)
    PermissionUmaUtil::RecordPermissionPromptShown(
        request->GetPermissionRequestType(),
        PermissionUtil::GetGestureType(user_gesture));
    if (PermissionDialogDelegate::ShouldShowDialog(user_gesture)) {
      PermissionDialogDelegate::CreateMediaStreamDialog(
          web_contents, user_gesture, std::move(request));
    } else {
      MediaStreamInfoBarDelegateAndroid::Create(web_contents, user_gesture,
                                                std::move(request));
    }
#else
    PermissionRequestManager* permission_request_manager =
        PermissionRequestManager::FromWebContents(web_contents);
    if (permission_request_manager)
      permission_request_manager->AddRequest(request.release());
#endif
  }
};

// static
void MediaStreamDevicesController::RequestPermissions(
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  PermissionPromptDelegateImpl delegate;
  RequestPermissionsWithDelegate(request, callback, &delegate);
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
    callback_.Run(content::MediaStreamDevices(),
                  content::MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN,
                  std::unique_ptr<content::MediaStreamUI>());
  }
}

bool MediaStreamDevicesController::IsAskingForAudio() const {
  return audio_setting_ == CONTENT_SETTING_ASK;
}

bool MediaStreamDevicesController::IsAskingForVideo() const {
  return video_setting_ == CONTENT_SETTING_ASK;
}

void MediaStreamDevicesController::PromptAnswered(ContentSetting setting,
                                                  bool persist) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  if (audio_setting_ == CONTENT_SETTING_ASK) {
    if (persist && setting != CONTENT_SETTING_ASK) {
      host_content_settings_map->SetContentSettingDefaultScope(
          request_.security_origin, GURL(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string(), setting);
    }
    audio_setting_ = setting;
  }

  if (video_setting_ == CONTENT_SETTING_ASK) {
    if (persist && setting != CONTENT_SETTING_ASK) {
      host_content_settings_map->SetContentSettingDefaultScope(
          request_.security_origin, GURL(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, std::string(), setting);
    }
    video_setting_ = setting;
  }

  if (setting == CONTENT_SETTING_BLOCK)
    denial_reason_ = content::MEDIA_DEVICE_PERMISSION_DENIED;
  else if (setting == CONTENT_SETTING_ASK)
    denial_reason_ = content::MEDIA_DEVICE_PERMISSION_DISMISSED;

  RunCallback();
}

void MediaStreamDevicesController::PromptAnsweredGroupedRequest(
    const std::vector<ContentSetting>& responses) {
  DCHECK(responses.size() == 1 || responses.size() == 2);

  // The audio setting will always be the first one in the vector, if it was
  // requested.
  if (audio_setting_ == CONTENT_SETTING_ASK)
    audio_setting_ = responses.front();

  if (video_setting_ == CONTENT_SETTING_ASK)
    video_setting_ = responses.back();

  for (ContentSetting response : responses) {
    if (response == CONTENT_SETTING_BLOCK)
      denial_reason_ = content::MEDIA_DEVICE_PERMISSION_DENIED;
    else if (response == CONTENT_SETTING_ASK)
      denial_reason_ = content::MEDIA_DEVICE_PERMISSION_DISMISSED;
  }

  RunCallback();
}

#if defined(OS_ANDROID)
void MediaStreamDevicesController::AndroidOSPromptAnswered(bool allowed) {
  DCHECK(audio_setting_ != CONTENT_SETTING_ASK &&
         video_setting_ != CONTENT_SETTING_ASK);

  if (!allowed) {
    denial_reason_ = content::MEDIA_DEVICE_PERMISSION_DENIED;
    // Only permissions that were previously ALLOW for a site will have had
    // their android permissions requested. It's only in that case that we need
    // to change the setting to BLOCK to reflect that it wasn't allowed.
    if (audio_setting_ == CONTENT_SETTING_ALLOW)
      audio_setting_ = CONTENT_SETTING_BLOCK;
    if (video_setting_ == CONTENT_SETTING_ALLOW)
      video_setting_ = CONTENT_SETTING_BLOCK;
  }

  RunCallback();
}
#endif  // defined(OS_ANDROID)

void MediaStreamDevicesController::RequestFinishedNoPrompt() {
  RunCallback();
}

// static
void MediaStreamDevicesController::RequestPermissionsWithDelegate(
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    PermissionPromptDelegate* delegate) {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request.render_process_id, request.render_frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (request.request_type == content::MEDIA_OPEN_DEVICE_PEPPER_ONLY) {
    MediaPermissionRequestLogger::LogRequest(
        web_contents, request.render_process_id, request.render_frame_id,
        content::IsOriginSecure(request.security_origin));
  }

  std::unique_ptr<MediaStreamDevicesController> controller(
      new MediaStreamDevicesController(web_contents, request, callback));

  // Show a prompt if needed.
  bool is_asking_for_audio = controller->IsAskingForAudio();
  bool is_asking_for_video = controller->IsAskingForVideo();
  if (is_asking_for_audio || is_asking_for_video) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    if (base::FeatureList::IsEnabled(
            features::kUsePermissionManagerForMediaRequests) &&
        PermissionRequestManager::IsEnabled()) {
      std::vector<ContentSettingsType> content_settings_types;

      if (is_asking_for_audio)
        content_settings_types.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
      if (is_asking_for_video) {
        content_settings_types.push_back(
            CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
      }

      PermissionManager::Get(profile)->RequestPermissions(
          content_settings_types, rfh, request.security_origin,
          request.user_gesture,
          base::Bind(
              &MediaStreamDevicesController::PromptAnsweredGroupedRequest,
              base::Passed(&controller)));
    } else {
      delegate->ShowPrompt(
          request.user_gesture, web_contents,
          base::MakeUnique<Request>(
              profile, is_asking_for_audio, is_asking_for_video,
              request.security_origin,
              base::Bind(&MediaStreamDevicesController::PromptAnswered,
                         base::Passed(&controller))));
    }
    return;
  }

#if defined(OS_ANDROID)
  // If either audio or video was previously allowed and Chrome no longer has
  // the necessary permissions, show a infobar to attempt to address this
  // mismatch.
  std::vector<ContentSettingsType> content_settings_types;
  if (controller->IsAllowedForAudio())
    content_settings_types.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);

  if (controller->IsAllowedForVideo()) {
    content_settings_types.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  }
  if (!content_settings_types.empty() &&
      PermissionUpdateInfoBarDelegate::ShouldShowPermissionInfobar(
          web_contents, content_settings_types)) {
    PermissionUpdateInfoBarDelegate::Create(
        web_contents, content_settings_types,
        base::Bind(&MediaStreamDevicesController::AndroidOSPromptAnswered,
                   base::Passed(&controller)));
    return;
  }
#endif

  // If we reach here, no prompt needed to be shown.
  controller->RequestFinishedNoPrompt();
}

MediaStreamDevicesController::MediaStreamDevicesController(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback)
    : web_contents_(web_contents), request_(request), callback_(callback) {
  DCHECK(content::IsOriginSecure(request_.security_origin) ||
         request_.request_type == content::MEDIA_OPEN_DEVICE_PEPPER_ONLY);

  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  content_settings_ = TabSpecificContentSettings::FromWebContents(web_contents);

  denial_reason_ = content::MEDIA_DEVICE_OK;
  audio_setting_ = GetContentSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                                     request, &denial_reason_);
  video_setting_ = GetContentSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                                     request, &denial_reason_);
}

bool MediaStreamDevicesController::IsAllowedForAudio() const {
  return audio_setting_ == CONTENT_SETTING_ALLOW;
}

bool MediaStreamDevicesController::IsAllowedForVideo() const {
  return video_setting_ == CONTENT_SETTING_ALLOW;
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

void MediaStreamDevicesController::RunCallback() {
  CHECK(!callback_.is_null());

  // If the kill switch is on we don't update the tab context.
  if (denial_reason_ != content::MEDIA_DEVICE_KILL_SWITCH_ON)
    UpdateTabSpecificContentSettings(audio_setting_, video_setting_);

  content::MediaStreamDevices devices =
      GetDevices(audio_setting_, video_setting_);

  // If either audio or video are allowed then the callback should report
  // success, otherwise we report |denial_reason_|.
  content::MediaStreamRequestResult request_result = content::MEDIA_DEVICE_OK;
  if (audio_setting_ != CONTENT_SETTING_ALLOW &&
      video_setting_ != CONTENT_SETTING_ALLOW) {
    DCHECK_NE(content::MEDIA_DEVICE_OK, denial_reason_);
    request_result = denial_reason_;
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
  DCHECK(!request_.security_origin.is_empty());
  DCHECK(content::IsOriginSecure(request_.security_origin) ||
         request_.request_type == content::MEDIA_OPEN_DEVICE_PEPPER_ONLY);
  if (!ContentTypeIsRequested(content_type, request)) {
    // No denial reason set as it will have been previously set.
    return CONTENT_SETTING_DEFAULT;
  }

  std::string device_id;
  if (content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC)
    device_id = request.requested_audio_device_id;
  else
    device_id = request.requested_video_device_id;
  if (!HasAvailableDevices(content_type, device_id)) {
    *denial_reason = content::MEDIA_DEVICE_NO_HARDWARE;
    return CONTENT_SETTING_BLOCK;
  }

  if (!IsUserAcceptAllowed(content_type)) {
    *denial_reason = content::MEDIA_DEVICE_PERMISSION_DENIED;
    return CONTENT_SETTING_BLOCK;
  }

  PermissionResult result =
      PermissionManager::Get(profile_)->GetPermissionStatus(
          content_type, request.security_origin,
          web_contents_->GetLastCommittedURL().GetOrigin());
  if (result.content_setting == CONTENT_SETTING_BLOCK) {
    *denial_reason = (result.source == PermissionStatusSource::KILL_SWITCH)
                         ? content::MEDIA_DEVICE_KILL_SWITCH_ON
                         : content::MEDIA_DEVICE_PERMISSION_DENIED;
  }

  return result.content_setting;
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
