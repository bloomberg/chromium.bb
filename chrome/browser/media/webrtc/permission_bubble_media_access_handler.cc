// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"

#include <memory>
#include <utility>

#include "base/metrics/field_trial.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/media/webrtc/screen_capture_infobar_delegate_android.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX)
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#endif

using content::BrowserThread;

using RepeatingMediaResponseCallback =
    base::RepeatingCallback<void(const blink::MediaStreamDevices& devices,
                                 blink::MediaStreamRequestResult result,
                                 std::unique_ptr<content::MediaStreamUI> ui)>;

struct PermissionBubbleMediaAccessHandler::PendingAccessRequest {
  PendingAccessRequest(const content::MediaStreamRequest& request,
                       RepeatingMediaResponseCallback callback)
      : request(request), callback(callback) {}
  ~PendingAccessRequest() {}

  // TODO(gbillock): make the MediaStreamDevicesController owned by
  // this object when we're using bubbles.
  content::MediaStreamRequest request;
  RepeatingMediaResponseCallback callback;
};

PermissionBubbleMediaAccessHandler::PermissionBubbleMediaAccessHandler() {
  // PermissionBubbleMediaAccessHandler should be created on UI thread.
  // Otherwise, it will not receive
  // content::NOTIFICATION_WEB_CONTENTS_DESTROYED, and that will result in
  // possible use after free.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notifications_registrar_.Add(this,
                               content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                               content::NotificationService::AllSources());
}

PermissionBubbleMediaAccessHandler::~PermissionBubbleMediaAccessHandler() {}

bool PermissionBubbleMediaAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::MediaStreamType type,
    const extensions::Extension* extension) {
#if defined(OS_ANDROID)
  return type == blink::MEDIA_DEVICE_VIDEO_CAPTURE ||
         type == blink::MEDIA_DEVICE_AUDIO_CAPTURE ||
         type == blink::MEDIA_GUM_DESKTOP_VIDEO_CAPTURE ||
         type == blink::MEDIA_DISPLAY_VIDEO_CAPTURE;
#else
  return type == blink::MEDIA_DEVICE_VIDEO_CAPTURE ||
         type == blink::MEDIA_DEVICE_AUDIO_CAPTURE;
#endif
}

bool PermissionBubbleMediaAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::MediaStreamType type,
    const extensions::Extension* extension) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  ContentSettingsType content_settings_type =
      type == blink::MEDIA_DEVICE_AUDIO_CAPTURE
          ? CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC
          : CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA;

  DCHECK(!security_origin.is_empty());
  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();
  PermissionManager* permission_manager = PermissionManager::Get(profile);
  return permission_manager
             ->GetPermissionStatusForFrame(content_settings_type,
                                           render_frame_host, security_origin)
             .content_setting == CONTENT_SETTING_ALLOW;
}

void PermissionBubbleMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(OS_ANDROID)
  if (IsScreenCaptureMediaType(request.video_type) &&
      !base::FeatureList::IsEnabled(
          chrome::android::kUserMediaScreenCapturing)) {
    // If screen capturing isn't enabled on Android, we'll use "invalid state"
    // as result, same as on desktop.
    std::move(callback).Run(blink::MediaStreamDevices(),
                            blink::MEDIA_DEVICE_INVALID_STATE, nullptr);
    return;
  }
#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX)
  // Fail if access is denied in system permissions. Note that if permissions
  // have not yet been determined, we don't fail. If all other permissions are
  // OK, we'll allow access. The reason for doing this is that if the permission
  // is not yet determined, the user will get an async system dialog and we
  // don't wait on that response before resolving getUserMedia. getUserMedia
  // will succeed, but audio/video will be silent/black until user allows
  // permission in the dialog. If the user denies permission audio/video will
  // continue to be silent/black but they will likely understand why since they
  // denied access. We trigger the system dialog explicitly in
  // OnAccessRequestResponse().
  // TODO(https://crbug.com/885184): Handle the not determined case better.
  if ((request.audio_type == blink::MEDIA_DEVICE_AUDIO_CAPTURE &&
       SystemAudioCapturePermissionIsDisallowed()) ||
      (request.video_type == blink::MEDIA_DEVICE_VIDEO_CAPTURE &&
       SystemVideoCapturePermissionIsDisallowed())) {
    std::move(callback).Run(blink::MediaStreamDevices(),
                            blink::MEDIA_DEVICE_SYSTEM_PERMISSION_DENIED,
                            nullptr);
    return;
  }
#endif  // defined(OS_MACOSX)

  RequestsQueue& queue = pending_requests_[web_contents];
  queue.push_back(PendingAccessRequest(
      request, base::AdaptCallbackForRepeating(std::move(callback))));

  // If this is the only request then show the infobar.
  if (queue.size() == 1)
    ProcessQueuedAccessRequest(web_contents);
}

void PermissionBubbleMediaAccessHandler::ProcessQueuedAccessRequest(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = pending_requests_.find(web_contents);

  if (it == pending_requests_.end() || it->second.empty()) {
    // Don't do anything if the tab was closed.
    return;
  }

  DCHECK(!it->second.empty());

  const content::MediaStreamRequest request = it->second.front().request;
#if defined(OS_ANDROID)
  if (IsScreenCaptureMediaType(request.video_type)) {
    ScreenCaptureInfoBarDelegateAndroid::Create(
        web_contents, request,
        base::Bind(&PermissionBubbleMediaAccessHandler::OnAccessRequestResponse,
                   base::Unretained(this), web_contents));
    return;
  }
#endif

  MediaStreamDevicesController::RequestPermissions(
      request,
      base::Bind(&PermissionBubbleMediaAccessHandler::OnAccessRequestResponse,
                 base::Unretained(this), web_contents));
}

void PermissionBubbleMediaAccessHandler::UpdateMediaRequestState(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    blink::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (state != content::MEDIA_REQUEST_STATE_CLOSING)
    return;

  bool found = false;
  for (auto rqs_it = pending_requests_.begin();
       rqs_it != pending_requests_.end(); ++rqs_it) {
    RequestsQueue& queue = rqs_it->second;
    for (RequestsQueue::iterator it = queue.begin(); it != queue.end(); ++it) {
      if (it->request.render_process_id == render_process_id &&
          it->request.render_frame_id == render_frame_id &&
          it->request.page_request_id == page_request_id) {
        queue.erase(it);
        found = true;
        break;
      }
    }
    if (found)
      break;
  }
}

void PermissionBubbleMediaAccessHandler::OnAccessRequestResponse(
    content::WebContents* web_contents,
    const blink::MediaStreamDevices& devices,
    blink::MediaStreamRequestResult result,
    std::unique_ptr<content::MediaStreamUI> ui) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = pending_requests_.find(web_contents);
  if (it == pending_requests_.end()) {
    // WebContents has been destroyed. Don't need to do anything.
    return;
  }

  RequestsQueue& queue(it->second);
  if (queue.empty())
    return;

#if defined(OS_MACOSX)
  // If the request was approved, trigger system user dialogs if needed. We must
  // do this explicitly so that the system gives the correct information about
  // the permission states in future requests, see HandleRequest().
  if (result == blink::MEDIA_DEVICE_OK) {
    const content::MediaStreamRequest& request = queue.front().request;
    if (request.audio_type == blink::MEDIA_DEVICE_AUDIO_CAPTURE)
      EnsureSystemAudioCapturePermissionIsOrGetsDetermined();
    if (request.video_type == blink::MEDIA_DEVICE_VIDEO_CAPTURE)
      EnsureSystemVideoCapturePermissionIsOrGetsDetermined();
  }
#endif  // defined(OS_MACOSX)

  RepeatingMediaResponseCallback callback = queue.front().callback;
  queue.pop_front();

  if (!queue.empty()) {
    // Post a task to process next queued request. It has to be done
    // asynchronously to make sure that calling infobar is not destroyed until
    // after this function returns.
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &PermissionBubbleMediaAccessHandler::ProcessQueuedAccessRequest,
            base::Unretained(this), web_contents));
  }

  callback.Run(devices, result, std::move(ui));
}

void PermissionBubbleMediaAccessHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_DESTROYED, type);

  pending_requests_.erase(content::Source<content::WebContents>(source).ptr());
}
