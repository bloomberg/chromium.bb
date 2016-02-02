// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_capture_devices_dispatcher.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/media/desktop_streams_registry.h"
#include "chrome/browser/media/media_access_handler.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/media/permission_bubble_media_access_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_capture_devices.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/media_stream_request.h"
#include "extensions/common/constants.h"
#include "media/base/media_switches.h"
#include "net/base/net_util.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#endif  // defined(OS_CHROMEOS)

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/media/desktop_capture_access_handler.h"
#include "chrome/browser/media/extension_media_access_handler.h"
#include "chrome/browser/media/tab_capture_access_handler.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#endif

using content::BrowserThread;
using content::MediaCaptureDevices;
using content::MediaStreamDevices;

namespace {

// Finds a device in |devices| that has |device_id|, or NULL if not found.
const content::MediaStreamDevice* FindDeviceWithId(
    const content::MediaStreamDevices& devices,
    const std::string& device_id) {
  content::MediaStreamDevices::const_iterator iter = devices.begin();
  for (; iter != devices.end(); ++iter) {
    if (iter->id == device_id) {
      return &(*iter);
    }
  }
  return NULL;
}

#if defined(ENABLE_EXTENSIONS)
inline DesktopCaptureAccessHandler* ToDesktopCaptureAccessHandler(
    MediaAccessHandler* handler) {
  return static_cast<DesktopCaptureAccessHandler*>(handler);
}
#endif
}  // namespace

MediaCaptureDevicesDispatcher* MediaCaptureDevicesDispatcher::GetInstance() {
  return base::Singleton<MediaCaptureDevicesDispatcher>::get();
}

MediaCaptureDevicesDispatcher::MediaCaptureDevicesDispatcher()
    : is_device_enumeration_disabled_(false),
      media_stream_capture_indicator_(new MediaStreamCaptureIndicator()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(OS_MACOSX)
  // AVFoundation is used for video/audio device monitoring and video capture.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceQTKit)) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableAVFoundation);
  }
#endif

#if defined(ENABLE_EXTENSIONS)
  media_access_handlers_.push_back(new ExtensionMediaAccessHandler());
  media_access_handlers_.push_back(new DesktopCaptureAccessHandler());
  media_access_handlers_.push_back(new TabCaptureAccessHandler());
#endif
  media_access_handlers_.push_back(new PermissionBubbleMediaAccessHandler());
}

MediaCaptureDevicesDispatcher::~MediaCaptureDevicesDispatcher() {}

void MediaCaptureDevicesDispatcher::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kDefaultAudioCaptureDevice,
                               std::string());
  registry->RegisterStringPref(prefs::kDefaultVideoCaptureDevice,
                               std::string());
}

bool MediaCaptureDevicesDispatcher::IsOriginForCasting(const GURL& origin) {
  // Whitelisted tab casting extensions.
  return
      // Dev
      origin.spec() == "chrome-extension://enhhojjnijigcajfphajepfemndkmdlo/" ||
      // Canary
      origin.spec() == "chrome-extension://hfaagokkkhdbgiakmmlclaapfelnkoah/" ||
      // Beta (internal)
      origin.spec() == "chrome-extension://fmfcbgogabcbclcofgocippekhfcmgfj/" ||
      // Google Cast Beta
      origin.spec() == "chrome-extension://dliochdbjfkdbacpmhlcpmleaejidimm/" ||
      // Google Cast Stable
      origin.spec() == "chrome-extension://boadgeojelhgndaghljhdicfkmllpafd/" ||
      // http://crbug.com/457908
      origin.spec() == "chrome-extension://ekpaaapppgpmolpcldedioblbkmijaca/" ||
      // http://crbug.com/574889
      origin.spec() == "chrome-extension://pkedcjkdefgpdelpbcmbmeomcjbeemfm/";
}

void MediaCaptureDevicesDispatcher::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void MediaCaptureDevicesDispatcher::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

const MediaStreamDevices&
MediaCaptureDevicesDispatcher::GetAudioCaptureDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_device_enumeration_disabled_ || !test_audio_devices_.empty())
    return test_audio_devices_;

  return MediaCaptureDevices::GetInstance()->GetAudioCaptureDevices();
}

const MediaStreamDevices&
MediaCaptureDevicesDispatcher::GetVideoCaptureDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_device_enumeration_disabled_ || !test_video_devices_.empty())
    return test_video_devices_;

  return MediaCaptureDevices::GetInstance()->GetVideoCaptureDevices();
}

void MediaCaptureDevicesDispatcher::ProcessMediaAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (MediaAccessHandler* handler : media_access_handlers_) {
    if (handler->SupportsStreamType(request.video_type, extension) ||
        handler->SupportsStreamType(request.audio_type, extension)) {
      handler->HandleRequest(web_contents, request, callback, extension);
      return;
    }
  }
  callback.Run(content::MediaStreamDevices(),
               content::MEDIA_DEVICE_NOT_SUPPORTED, nullptr);
}

bool MediaCaptureDevicesDispatcher::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CheckMediaAccessPermission(web_contents, security_origin, type,
                                    nullptr);
}

bool MediaCaptureDevicesDispatcher::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (MediaAccessHandler* handler : media_access_handlers_) {
    if (handler->SupportsStreamType(type, extension)) {
      return handler->CheckMediaAccessPermission(web_contents, security_origin,
                                                 type, extension);
    }
  }
  return false;
}

void MediaCaptureDevicesDispatcher::GetDefaultDevicesForProfile(
    Profile* profile,
    bool audio,
    bool video,
    content::MediaStreamDevices* devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(audio || video);

  PrefService* prefs = profile->GetPrefs();
  std::string default_device;
  if (audio) {
    default_device = prefs->GetString(prefs::kDefaultAudioCaptureDevice);
    const content::MediaStreamDevice* device =
        GetRequestedAudioDevice(default_device);
    if (!device)
      device = GetFirstAvailableAudioDevice();
    if (device)
      devices->push_back(*device);
  }

  if (video) {
    default_device = prefs->GetString(prefs::kDefaultVideoCaptureDevice);
    const content::MediaStreamDevice* device =
        GetRequestedVideoDevice(default_device);
    if (!device)
      device = GetFirstAvailableVideoDevice();
    if (device)
      devices->push_back(*device);
  }
}

const content::MediaStreamDevice*
MediaCaptureDevicesDispatcher::GetRequestedAudioDevice(
    const std::string& requested_audio_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const content::MediaStreamDevices& audio_devices = GetAudioCaptureDevices();
  const content::MediaStreamDevice* const device =
      FindDeviceWithId(audio_devices, requested_audio_device_id);
  return device;
}

const content::MediaStreamDevice*
MediaCaptureDevicesDispatcher::GetFirstAvailableAudioDevice() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const content::MediaStreamDevices& audio_devices = GetAudioCaptureDevices();
  if (audio_devices.empty())
    return NULL;
  return &(*audio_devices.begin());
}

const content::MediaStreamDevice*
MediaCaptureDevicesDispatcher::GetRequestedVideoDevice(
    const std::string& requested_video_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const content::MediaStreamDevices& video_devices = GetVideoCaptureDevices();
  const content::MediaStreamDevice* const device =
      FindDeviceWithId(video_devices, requested_video_device_id);
  return device;
}

const content::MediaStreamDevice*
MediaCaptureDevicesDispatcher::GetFirstAvailableVideoDevice() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const content::MediaStreamDevices& video_devices = GetVideoCaptureDevices();
  if (video_devices.empty())
    return NULL;
  return &(*video_devices.begin());
}

void MediaCaptureDevicesDispatcher::DisableDeviceEnumerationForTesting() {
  is_device_enumeration_disabled_ = true;
}

scoped_refptr<MediaStreamCaptureIndicator>
MediaCaptureDevicesDispatcher::GetMediaStreamCaptureIndicator() {
  return media_stream_capture_indicator_;
}

DesktopStreamsRegistry*
MediaCaptureDevicesDispatcher::GetDesktopStreamsRegistry() {
  if (!desktop_streams_registry_)
    desktop_streams_registry_.reset(new DesktopStreamsRegistry());
  return desktop_streams_registry_.get();
}

void MediaCaptureDevicesDispatcher::OnAudioCaptureDevicesChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &MediaCaptureDevicesDispatcher::NotifyAudioDevicesChangedOnUIThread,
          base::Unretained(this)));
}

void MediaCaptureDevicesDispatcher::OnVideoCaptureDevicesChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &MediaCaptureDevicesDispatcher::NotifyVideoDevicesChangedOnUIThread,
          base::Unretained(this)));
}

void MediaCaptureDevicesDispatcher::OnMediaRequestStateChanged(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    const GURL& security_origin,
    content::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &MediaCaptureDevicesDispatcher::UpdateMediaRequestStateOnUIThread,
          base::Unretained(this), render_process_id, render_frame_id,
          page_request_id, security_origin, stream_type, state));
}

void MediaCaptureDevicesDispatcher::OnCreatingAudioStream(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &MediaCaptureDevicesDispatcher::OnCreatingAudioStreamOnUIThread,
          base::Unretained(this), render_process_id, render_frame_id));
}

void MediaCaptureDevicesDispatcher::NotifyAudioDevicesChangedOnUIThread() {
  MediaStreamDevices devices = GetAudioCaptureDevices();
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnUpdateAudioDevices(devices));
}

void MediaCaptureDevicesDispatcher::NotifyVideoDevicesChangedOnUIThread() {
  MediaStreamDevices devices = GetVideoCaptureDevices();
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnUpdateVideoDevices(devices));
}

void MediaCaptureDevicesDispatcher::UpdateMediaRequestStateOnUIThread(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    const GURL& security_origin,
    content::MediaStreamType stream_type,
    content::MediaRequestState state) {
  for (MediaAccessHandler* handler : media_access_handlers_) {
    if (handler->SupportsStreamType(stream_type, nullptr)) {
      handler->UpdateMediaRequestState(render_process_id, render_frame_id,
                                       page_request_id, stream_type, state);
      break;
    }
  }

#if defined(OS_CHROMEOS)
  if (IsOriginForCasting(security_origin) && IsVideoMediaType(stream_type)) {
    // Notify ash that casting state has changed.
    if (state == content::MEDIA_REQUEST_STATE_DONE) {
      ash::Shell::GetInstance()->OnCastingSessionStartedOrStopped(true);
    } else if (state == content::MEDIA_REQUEST_STATE_CLOSING) {
      ash::Shell::GetInstance()->OnCastingSessionStartedOrStopped(false);
    }
  }
#endif

  FOR_EACH_OBSERVER(Observer, observers_,
                    OnRequestUpdate(render_process_id,
                                    render_frame_id,
                                    stream_type,
                                    state));
}

void MediaCaptureDevicesDispatcher::OnCreatingAudioStreamOnUIThread(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnCreatingAudioStream(render_process_id, render_frame_id));
}

bool MediaCaptureDevicesDispatcher::IsDesktopCaptureInProgress() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(ENABLE_EXTENSIONS)
  for (MediaAccessHandler* handler : media_access_handlers_) {
    if (handler->SupportsStreamType(content::MEDIA_DESKTOP_VIDEO_CAPTURE,
                                    NULL)) {
      return ToDesktopCaptureAccessHandler(handler)->IsCaptureInProgress();
    }
  }
#endif
  return false;
}

void MediaCaptureDevicesDispatcher::SetTestAudioCaptureDevices(
    const MediaStreamDevices& devices) {
  test_audio_devices_ = devices;
}

void MediaCaptureDevicesDispatcher::SetTestVideoCaptureDevices(
    const MediaStreamDevices& devices) {
  test_video_devices_ = devices;
}
