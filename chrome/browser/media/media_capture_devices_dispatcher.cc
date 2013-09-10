// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_capture_devices_dispatcher.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry_factory.h"
#include "chrome/browser/media/audio_stream_indicator.h"
#include "chrome/browser/media/desktop_streams_registry.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_devices_monitor.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/desktop_media_id.h"
#include "content/public/common/media_stream_request.h"
#include "extensions/common/constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(USE_CRAS)
#include "media/audio/cras/audio_manager_cras.h"
#endif

using content::BrowserThread;
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
};

// This is a short-term solution to allow testing of the the Screen Capture API
// with Google Hangouts in M27.
// TODO(sergeyu): Remove this whitelist as soon as possible.
bool IsOriginWhitelistedForScreenCapture(const GURL& origin) {
#if defined(OFFICIAL_BUILD)
  if (// Google Hangouts.
      (origin.SchemeIs("https") &&
       EndsWith(origin.spec(), ".talkgadget.google.com/", true)) ||
      origin.spec() == "https://plus.google.com/" ||
      origin.spec() == "chrome-extension://pkedcjkdefgpdelpbcmbmeomcjbeemfm/" ||
      origin.spec() == "chrome-extension://fmfcbgogabcbclcofgocippekhfcmgfj/" ||
      origin.spec() == "chrome-extension://hfaagokkkhdbgiakmmlclaapfelnkoah/" ||
      origin.spec() == "chrome-extension://gfdkimpbcpahaombhbimeihdjnejgicl/") {
    return true;
  }
  // Check against hashed origins.
  const std::string origin_hash = base::SHA1HashString(origin.spec());
  DCHECK_EQ(origin_hash.length(), base::kSHA1Length);
  const std::string hexencoded_origin_hash =
      base::HexEncode(origin_hash.data(), origin_hash.length());
  return
      hexencoded_origin_hash == "3C2705BC432E7C51CA8553FDC5BEE873FF2468EE" ||
      hexencoded_origin_hash == "50F02B8A668CAB274527D58356F07C2143080FCC";
#else
  return false;
#endif
}

// Helper to get title of the calling application shown in the screen capture
// notification.
string16 GetApplicationTitle(content::WebContents* web_contents,
                             const extensions::Extension* extension) {
  // Use extension name as title for extensions and origin for drive-by web.
  std::string title;
  if (extension) {
    title = extension->name();
  } else {
    title = web_contents->GetURL().GetOrigin().spec();
  }
  return UTF8ToUTF16(title);
}

}  // namespace

MediaCaptureDevicesDispatcher::PendingAccessRequest::PendingAccessRequest(
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback)
    : request(request),
      callback(callback) {
}

MediaCaptureDevicesDispatcher::PendingAccessRequest::~PendingAccessRequest() {}

MediaCaptureDevicesDispatcher* MediaCaptureDevicesDispatcher::GetInstance() {
  return Singleton<MediaCaptureDevicesDispatcher>::get();
}

MediaCaptureDevicesDispatcher::MediaCaptureDevicesDispatcher()
    : devices_enumerated_(false),
      is_device_enumeration_disabled_(false),
      media_stream_capture_indicator_(new MediaStreamCaptureIndicator()),
      audio_stream_indicator_(new AudioStreamIndicator()) {
  // MediaCaptureDevicesDispatcher is a singleton. It should be created on
  // UI thread. Otherwise, it will not receive
  // content::NOTIFICATION_WEB_CONTENTS_DESTROYED, and that will result in
  // possible use after free.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  notifications_registrar_.Add(
      this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::NotificationService::AllSources());
}

MediaCaptureDevicesDispatcher::~MediaCaptureDevicesDispatcher() {}

void MediaCaptureDevicesDispatcher::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(
      prefs::kDefaultAudioCaptureDevice,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultVideoCaptureDevice,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void MediaCaptureDevicesDispatcher::AddObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void MediaCaptureDevicesDispatcher::RemoveObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

const MediaStreamDevices&
MediaCaptureDevicesDispatcher::GetAudioCaptureDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!is_device_enumeration_disabled_ && !devices_enumerated_) {
    content::EnsureMonitorCaptureDevices();
    devices_enumerated_ = true;
  }
  return audio_devices_;
}

const MediaStreamDevices&
MediaCaptureDevicesDispatcher::GetVideoCaptureDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!is_device_enumeration_disabled_ && !devices_enumerated_) {
    content::EnsureMonitorCaptureDevices();
    devices_enumerated_ = true;
  }
  return video_devices_;
}

void MediaCaptureDevicesDispatcher::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED) {
    content::WebContents* web_contents =
        content::Source<content::WebContents>(source).ptr();
    pending_requests_.erase(web_contents);
  }
}

void MediaCaptureDevicesDispatcher::ProcessMediaAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (request.video_type == content::MEDIA_DESKTOP_VIDEO_CAPTURE ||
      request.audio_type == content::MEDIA_SYSTEM_AUDIO_CAPTURE) {
    ProcessDesktopCaptureAccessRequest(
        web_contents, request, callback, extension);
  } else if (request.video_type == content::MEDIA_TAB_VIDEO_CAPTURE ||
             request.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE) {
    ProcessTabCaptureAccessRequest(
        web_contents, request, callback, extension);
  } else if (extension && extension->is_platform_app()) {
    // For extensions access is approved based on extension permissions.
    ProcessMediaAccessRequestFromPlatformApp(
        web_contents, request, callback, extension);
  } else {
    ProcessRegularMediaAccessRequest(web_contents, request, callback);
  }
}

void MediaCaptureDevicesDispatcher::ProcessDesktopCaptureAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  content::MediaStreamDevices devices;
  scoped_ptr<content::MediaStreamUI> ui;

  if (request.video_type != content::MEDIA_DESKTOP_VIDEO_CAPTURE) {
    callback.Run(devices, ui.Pass());
    return;
  }

  // First check if Desktop Capture API (i.e.
  // chrome.desktopCapture.chooseDesktopMedia()) was used to generate device Id.
  content::DesktopMediaID media_id =
      GetDesktopStreamsRegistry()->RequestMediaForStreamId(
          request.requested_video_device_id, request.security_origin);

  // If the id wasn't generated using Desktop Capture API then process it as a
  // screen capture request.
  if (media_id.type == content::DesktopMediaID::TYPE_NONE) {
    ProcessScreenCaptureAccessRequest(
        web_contents, request, callback, extension);
    return;
  }

  // Add selected desktop source to the list.
  devices.push_back(content::MediaStreamDevice(
      content::MEDIA_DESKTOP_VIDEO_CAPTURE, media_id.ToString(),
      std::string()));

  // Audio is only supported for screen capture streams.
  if (media_id.type == content::DesktopMediaID::TYPE_SCREEN &&
      request.audio_type == content::MEDIA_SYSTEM_AUDIO_CAPTURE) {
    devices.push_back(content::MediaStreamDevice(
        content::MEDIA_SYSTEM_AUDIO_CAPTURE, media_id.ToString(),
        "System Audio"));
  }

  ui = ScreenCaptureNotificationUI::Create(l10n_util::GetStringFUTF16(
      IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_TEXT,
      GetApplicationTitle(web_contents, extension)));

  callback.Run(devices, ui.Pass());
}

void MediaCaptureDevicesDispatcher::ProcessScreenCaptureAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  content::MediaStreamDevices devices;
  scoped_ptr<content::MediaStreamUI> ui;

  DCHECK_EQ(request.video_type, content::MEDIA_DESKTOP_VIDEO_CAPTURE);

  content::DesktopMediaID media_id =
      content::DesktopMediaID::Parse(request.requested_video_device_id);
  if (media_id.is_null()) {
    LOG(ERROR) << "Invalid desktop media ID: "
               << request.requested_video_device_id;
    callback.Run(devices, ui.Pass());
    return;
  }

  // Only screen capture can be requested without using desktop media picker.
  if (media_id.type != content::DesktopMediaID::TYPE_SCREEN) {
    LOG(ERROR) << "Unsupported desktop media ID: "
               << request.requested_video_device_id;
    callback.Run(devices, ui.Pass());
    return;
  }

  const bool system_audio_capture_requested =
      request.audio_type == content::MEDIA_SYSTEM_AUDIO_CAPTURE;

#if defined(USE_CRAS)
  const bool system_audio_capture_supported = true;
#else
  const bool system_audio_capture_supported = false;
#endif

  // Reject request when audio capture was requested but is not supported on
  // this system.
  if (system_audio_capture_requested && !system_audio_capture_supported) {
    callback.Run(devices, ui.Pass());
    return;
  }

  const bool component_extension =
      extension && extension->location() == extensions::Manifest::COMPONENT;

  const bool screen_capture_enabled =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableUserMediaScreenCapturing) ||
      IsOriginWhitelistedForScreenCapture(request.security_origin);

  const bool origin_is_secure =
      request.security_origin.SchemeIsSecure() ||
      request.security_origin.SchemeIs(extensions::kExtensionScheme) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowHttpScreenCapture);

  // Approve request only when the following conditions are met:
  //  1. Screen capturing is enabled via command line switch or white-listed for
  //     the given origin.
  //  2. Request comes from a page with a secure origin or from an extension.
  if (screen_capture_enabled && origin_is_secure) {
    // For component extensions, bypass message box.
    bool user_approved = false;
    if (!component_extension) {
      string16 application_name = UTF8ToUTF16(
          extension ? extension->name() : request.security_origin.spec());
      string16 confirmation_text = l10n_util::GetStringFUTF16(
          request.audio_type == content::MEDIA_NO_SERVICE ?
              IDS_MEDIA_SCREEN_CAPTURE_CONFIRMATION_TEXT :
              IDS_MEDIA_SCREEN_AND_AUDIO_CAPTURE_CONFIRMATION_TEXT,
          application_name);
      chrome::MessageBoxResult result = chrome::ShowMessageBox(
          NULL,
          l10n_util::GetStringFUTF16(
              IDS_MEDIA_SCREEN_CAPTURE_CONFIRMATION_TITLE, application_name),
          confirmation_text,
          chrome::MESSAGE_BOX_TYPE_QUESTION);
      user_approved = (result == chrome::MESSAGE_BOX_RESULT_YES);
    }

    if (user_approved || component_extension) {
      devices.push_back(content::MediaStreamDevice(
          content::MEDIA_DESKTOP_VIDEO_CAPTURE, media_id.ToString(), "Screen"));
      if (system_audio_capture_requested) {
#if defined(USE_CRAS)
        // Use the special loopback device ID for system audio capture.
        devices.push_back(content::MediaStreamDevice(
            content::MEDIA_SYSTEM_AUDIO_CAPTURE,
            media::AudioManagerCras::kLoopbackDeviceId,
            "System Audio"));
#endif
      }
    }
  }

  // Unless we're being invoked from a component extension, register to display
  // the notification for stream capture.
  if (!devices.empty() && !component_extension) {
    ui = ScreenCaptureNotificationUI::Create(l10n_util::GetStringFUTF16(
        IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_TEXT,
        GetApplicationTitle(web_contents, extension)));
  }

  callback.Run(devices, ui.Pass());
}

void MediaCaptureDevicesDispatcher::ProcessTabCaptureAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  content::MediaStreamDevices devices;
  scoped_ptr<content::MediaStreamUI> ui;

#if defined(OS_ANDROID)
  // Tab capture is not supported on Android.
  callback.Run(devices, ui.Pass());
#else  // defined(OS_ANDROID)
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  extensions::TabCaptureRegistry* tab_capture_registry =
      extensions::TabCaptureRegistryFactory::GetForProfile(profile);
  bool tab_capture_allowed =
      tab_capture_registry->VerifyRequest(request.render_process_id,
                                          request.render_view_id);

  if (request.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE &&
      tab_capture_allowed &&
      extension->HasAPIPermission(extensions::APIPermission::kTabCapture)) {
    devices.push_back(content::MediaStreamDevice(
        content::MEDIA_TAB_AUDIO_CAPTURE, std::string(), std::string()));
  }

  if (request.video_type == content::MEDIA_TAB_VIDEO_CAPTURE &&
      tab_capture_allowed &&
      extension->HasAPIPermission(extensions::APIPermission::kTabCapture)) {
    devices.push_back(content::MediaStreamDevice(
        content::MEDIA_TAB_VIDEO_CAPTURE, std::string(), std::string()));
  }

  if (!devices.empty()) {
    ui = media_stream_capture_indicator_->RegisterMediaStream(
        web_contents, devices);
  }
  callback.Run(devices, ui.Pass());
#endif  // !defined(OS_ANDROID)
}

void MediaCaptureDevicesDispatcher::ProcessMediaAccessRequestFromPlatformApp(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  content::MediaStreamDevices devices;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  if (request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE &&
      extension->HasAPIPermission(extensions::APIPermission::kAudioCapture)) {
    GetDefaultDevicesForProfile(profile, true, false, &devices);
  }

  if (request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE &&
      extension->HasAPIPermission(extensions::APIPermission::kVideoCapture)) {
    GetDefaultDevicesForProfile(profile, false, true, &devices);
  }

  scoped_ptr<content::MediaStreamUI> ui;
  if (!devices.empty()) {
    ui = media_stream_capture_indicator_->RegisterMediaStream(
        web_contents, devices);
  }
  callback.Run(devices, ui.Pass());
}

void MediaCaptureDevicesDispatcher::ProcessRegularMediaAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RequestsQueue& queue = pending_requests_[web_contents];
  queue.push_back(PendingAccessRequest(request, callback));

  // If this is the only request then show the infobar.
  if (queue.size() == 1)
    ProcessQueuedAccessRequest(web_contents);
}

void MediaCaptureDevicesDispatcher::ProcessQueuedAccessRequest(
    content::WebContents* web_contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::map<content::WebContents*, RequestsQueue>::iterator it =
      pending_requests_.find(web_contents);

  if (it == pending_requests_.end() || it->second.empty()) {
    // Don't do anything if the tab was was closed.
    return;
  }

  DCHECK(!it->second.empty());

  MediaStreamInfoBarDelegate::Create(
      web_contents, it->second.front().request,
      base::Bind(&MediaCaptureDevicesDispatcher::OnAccessRequestResponse,
                 base::Unretained(this), web_contents));
}

void MediaCaptureDevicesDispatcher::OnAccessRequestResponse(
    content::WebContents* web_contents,
    const content::MediaStreamDevices& devices,
    scoped_ptr<content::MediaStreamUI> ui) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::map<content::WebContents*, RequestsQueue>::iterator it =
      pending_requests_.find(web_contents);
  if (it == pending_requests_.end()) {
    // WebContents has been destroyed. Don't need to do anything.
    return;
  }

  RequestsQueue& queue(it->second);
  if (queue.empty())
    return;

  content::MediaResponseCallback callback = queue.front().callback;
  queue.pop_front();

  if (!queue.empty()) {
    // Post a task to process next queued request. It has to be done
    // asynchronously to make sure that calling infobar is not destroyed until
    // after this function returns.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&MediaCaptureDevicesDispatcher::ProcessQueuedAccessRequest,
                   base::Unretained(this), web_contents));
  }

  callback.Run(devices, ui.Pass());
}

void MediaCaptureDevicesDispatcher::GetDefaultDevicesForProfile(
    Profile* profile,
    bool audio,
    bool video,
    content::MediaStreamDevices* devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const content::MediaStreamDevices& audio_devices = GetAudioCaptureDevices();
  const content::MediaStreamDevice* const device =
      FindDeviceWithId(audio_devices, requested_audio_device_id);
  return device;
}

const content::MediaStreamDevice*
MediaCaptureDevicesDispatcher::GetFirstAvailableAudioDevice() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const content::MediaStreamDevices& audio_devices = GetAudioCaptureDevices();
  if (audio_devices.empty())
    return NULL;
  return &(*audio_devices.begin());
}

const content::MediaStreamDevice*
MediaCaptureDevicesDispatcher::GetRequestedVideoDevice(
    const std::string& requested_video_device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const content::MediaStreamDevices& video_devices = GetVideoCaptureDevices();
  const content::MediaStreamDevice* const device =
      FindDeviceWithId(video_devices, requested_video_device_id);
  return device;
}

const content::MediaStreamDevice*
MediaCaptureDevicesDispatcher::GetFirstAvailableVideoDevice() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

scoped_refptr<AudioStreamIndicator>
MediaCaptureDevicesDispatcher::GetAudioStreamIndicator() {
  return audio_stream_indicator_;
}

DesktopStreamsRegistry*
MediaCaptureDevicesDispatcher::GetDesktopStreamsRegistry() {
  if (!desktop_streams_registry_)
    desktop_streams_registry_.reset(new DesktopStreamsRegistry());
  return desktop_streams_registry_.get();
}

void MediaCaptureDevicesDispatcher::OnAudioCaptureDevicesChanged(
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MediaCaptureDevicesDispatcher::UpdateAudioDevicesOnUIThread,
                 base::Unretained(this), devices));
}

void MediaCaptureDevicesDispatcher::OnVideoCaptureDevicesChanged(
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MediaCaptureDevicesDispatcher::UpdateVideoDevicesOnUIThread,
                 base::Unretained(this), devices));
}

void MediaCaptureDevicesDispatcher::OnMediaRequestStateChanged(
    int render_process_id,
    int render_view_id,
    int page_request_id,
    const content::MediaStreamDevice& device,
    content::MediaRequestState state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &MediaCaptureDevicesDispatcher::UpdateMediaRequestStateOnUIThread,
          base::Unretained(this), render_process_id, render_view_id,
          page_request_id, device, state));

}

void MediaCaptureDevicesDispatcher::OnAudioStreamPlayingChanged(
    int render_process_id, int render_view_id, int stream_id,
    bool is_playing, float power_dbfs, bool clipped) {
  audio_stream_indicator_->UpdateWebContentsStatus(
      render_process_id, render_view_id, stream_id,
      is_playing, power_dbfs, clipped);
}

void MediaCaptureDevicesDispatcher::OnCreatingAudioStream(
    int render_process_id,
    int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &MediaCaptureDevicesDispatcher::OnCreatingAudioStreamOnUIThread,
          base::Unretained(this), render_process_id, render_view_id));
}

void MediaCaptureDevicesDispatcher::UpdateAudioDevicesOnUIThread(
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  devices_enumerated_ = true;
  audio_devices_ = devices;
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnUpdateAudioDevices(audio_devices_));
}

void MediaCaptureDevicesDispatcher::UpdateVideoDevicesOnUIThread(
    const content::MediaStreamDevices& devices){
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  devices_enumerated_ = true;
  video_devices_ = devices;
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnUpdateVideoDevices(video_devices_));
}

void MediaCaptureDevicesDispatcher::UpdateMediaRequestStateOnUIThread(
    int render_process_id,
    int render_view_id,
    int page_request_id,
    const content::MediaStreamDevice& device,
    content::MediaRequestState state) {
  // Cancel the request.
  if (state == content::MEDIA_REQUEST_STATE_CLOSING) {
    bool found = false;
    for (RequestsQueues::iterator rqs_it = pending_requests_.begin();
         rqs_it != pending_requests_.end(); ++rqs_it) {
      RequestsQueue& queue = rqs_it->second;
      for (RequestsQueue::iterator it = queue.begin();
           it != queue.end(); ++it) {
        if (it->request.render_process_id == render_process_id &&
            it->request.render_view_id == render_view_id &&
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

  FOR_EACH_OBSERVER(Observer, observers_,
                    OnRequestUpdate(render_process_id,
                                    render_view_id,
                                    device,
                                    state));
}

void MediaCaptureDevicesDispatcher::OnCreatingAudioStreamOnUIThread(
    int render_process_id,
    int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnCreatingAudioStream(render_process_id, render_view_id));
}
