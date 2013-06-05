// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_capture_devices_dispatcher.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry_factory.h"
#include "chrome/browser/media/audio_stream_indicator.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_devices_monitor.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/media_stream_request.h"
#include "extensions/common/constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::MediaStreamDevices;

namespace {

const content::MediaStreamDevice* FindDefaultDeviceWithId(
    const content::MediaStreamDevices& devices,
    const std::string& device_id) {
  if (devices.empty())
    return NULL;

  content::MediaStreamDevices::const_iterator iter = devices.begin();
  for (; iter != devices.end(); ++iter) {
    if (iter->id == device_id) {
      return &(*iter);
    }
  }

  return &(*devices.begin());
};

// This is a short-term solution to allow testing of the the Screen Capture API
// with Google Hangouts in M27.
// TODO(sergeyu): Remove this whitelist as soon as possible.
bool IsOriginWhitelistedForScreenCapture(const GURL& origin) {
#if defined(OFFICIAL_BUILD)
  if (// Google Hangouts.
      origin.spec() == "https://staging.talkgadget.google.com/" ||
      origin.spec() == "https://plus.google.com/" ||
      origin.spec() == "chrome-extension://pkedcjkdefgpdelpbcmbmeomcjbeemfm/" ||
      origin.spec() == "chrome-extension://fmfcbgogabcbclcofgocippekhfcmgfj/" ||
      origin.spec() == "chrome-extension://hfaagokkkhdbgiakmmlclaapfelnkoah/") {
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

}  // namespace

MediaCaptureDevicesDispatcher* MediaCaptureDevicesDispatcher::GetInstance() {
  return Singleton<MediaCaptureDevicesDispatcher>::get();
}

MediaCaptureDevicesDispatcher::MediaCaptureDevicesDispatcher()
    : devices_enumerated_(false),
      media_stream_capture_indicator_(new MediaStreamCaptureIndicator()),
      audio_stream_indicator_(new AudioStreamIndicator()) {}

MediaCaptureDevicesDispatcher::~MediaCaptureDevicesDispatcher() {}

void MediaCaptureDevicesDispatcher::RegisterUserPrefs(
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
  if (!devices_enumerated_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&content::EnsureMonitorCaptureDevices));
    devices_enumerated_ = true;
  }
  return audio_devices_;
}

const MediaStreamDevices&
MediaCaptureDevicesDispatcher::GetVideoCaptureDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!devices_enumerated_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&content::EnsureMonitorCaptureDevices));
    devices_enumerated_ = true;
  }
  return video_devices_;
}

void MediaCaptureDevicesDispatcher::ProcessMediaAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (request.video_type == content::MEDIA_SCREEN_VIDEO_CAPTURE) {
    ProcessScreenCaptureAccessRequest(web_contents, request, callback);
  } else if (extension) {
    // For extensions access is approved based on extension permissions.
    ProcessMediaAccessRequestFromExtension(
        web_contents, request, callback, extension);
  } else {
    // For all regular media requests show infobar.
    MediaStreamInfoBarDelegate::Create(web_contents, request, callback);
  }
}

void MediaCaptureDevicesDispatcher::ProcessScreenCaptureAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  content::MediaStreamDevices devices;

  bool screen_capture_enabled =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableUserMediaScreenCapturing) ||
      IsOriginWhitelistedForScreenCapture(request.security_origin);

  bool origin_is_secure =
      request.security_origin.SchemeIsSecure() ||
      request.security_origin.SchemeIs(extensions::kExtensionScheme) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowHttpScreenCapture);

  // Approve request only when the following conditions are met:
  //  1. Screen capturing is enabled via command line switch or white-listed for
  //     the given origin.
  //  2. Request comes from a page with a secure origin or from an extension.
  //  3. Audio capture was not requested (it's not supported yet).
  if (screen_capture_enabled && origin_is_secure &&
      request.audio_type == content::MEDIA_NO_SERVICE) {
    string16 application_name = UTF8ToUTF16(request.security_origin.spec());
    chrome::MessageBoxResult result = chrome::ShowMessageBox(
        NULL,
        l10n_util::GetStringFUTF16(IDS_MEDIA_SCREEN_CAPTURE_CONFIRMATION_TITLE,
                                   application_name),
        l10n_util::GetStringFUTF16(IDS_MEDIA_SCREEN_CAPTURE_CONFIRMATION_TEXT,
                                   application_name),
        chrome::MESSAGE_BOX_TYPE_QUESTION);
    if (result == chrome::MESSAGE_BOX_RESULT_YES) {
      devices.push_back(content::MediaStreamDevice(
          content::MEDIA_SCREEN_VIDEO_CAPTURE, std::string(), "Screen"));
    }
  }

  scoped_ptr<content::MediaStreamUI> ui;
  if (!devices.empty()) {
    ui = media_stream_capture_indicator_->RegisterMediaStream(
        web_contents, devices);
  }
  callback.Run(devices, ui.Pass());
}

void MediaCaptureDevicesDispatcher::ProcessMediaAccessRequestFromExtension(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  content::MediaStreamDevices devices;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();

#if defined(OS_ANDROID)
  // Tab capture is not supported on Android.
  bool tab_capture_allowed = false;
#else
  extensions::TabCaptureRegistry* tab_capture_registry =
      extensions::TabCaptureRegistryFactory::GetForProfile(profile);
  bool tab_capture_allowed =
      tab_capture_registry->VerifyRequest(request.render_process_id,
                                          request.render_view_id);
#endif

  if (request.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE &&
      tab_capture_allowed &&
      extensions::FeatureSwitch::tab_capture()->IsEnabled() &&
      extension->HasAPIPermission(extensions::APIPermission::kTabCapture)) {
    devices.push_back(content::MediaStreamDevice(
        content::MEDIA_TAB_AUDIO_CAPTURE, std::string(), std::string()));
  } else if (request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE &&
             extension->HasAPIPermission(
                 extensions::APIPermission::kAudioCapture)) {
    std::string default_device =
        prefs->GetString(prefs::kDefaultAudioCaptureDevice);
    GetRequestedDevice(default_device, true, false, &devices);
  }

  if (request.video_type == content::MEDIA_TAB_VIDEO_CAPTURE &&
      tab_capture_allowed &&
      extensions::FeatureSwitch::tab_capture()->IsEnabled() &&
      extension->HasAPIPermission(extensions::APIPermission::kTabCapture)) {
    devices.push_back(content::MediaStreamDevice(
        content::MEDIA_TAB_VIDEO_CAPTURE, std::string(), std::string()));
  } else if (request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE &&
       extension->HasAPIPermission(extensions::APIPermission::kVideoCapture)) {
    std::string default_device = prefs->GetString(
        prefs::kDefaultVideoCaptureDevice);
    GetRequestedDevice(default_device, false, true, &devices);
  }

  scoped_ptr<content::MediaStreamUI> ui;
  if (!devices.empty()) {
    ui = media_stream_capture_indicator_->RegisterMediaStream(
        web_contents, devices);
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
    GetRequestedDevice(default_device, true, false, devices);
  }

  if (video) {
    default_device = prefs->GetString(prefs::kDefaultVideoCaptureDevice);
    GetRequestedDevice(default_device, false, true, devices);
  }
}

void MediaCaptureDevicesDispatcher::GetRequestedDevice(
    const std::string& requested_device_id,
    bool audio,
    bool video,
    content::MediaStreamDevices* devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(audio || video);

  if (audio) {
    const content::MediaStreamDevices& audio_devices = GetAudioCaptureDevices();
    const content::MediaStreamDevice* const device =
        FindDefaultDeviceWithId(audio_devices, requested_device_id);
    if (device)
      devices->push_back(*device);
  }
  if (video) {
    const content::MediaStreamDevices& video_devices = GetVideoCaptureDevices();
    const content::MediaStreamDevice* const device =
        FindDefaultDeviceWithId(video_devices, requested_device_id);
    if (device)
      devices->push_back(*device);
  }
}

scoped_refptr<MediaStreamCaptureIndicator>
    MediaCaptureDevicesDispatcher::GetMediaStreamCaptureIndicator() {
  return media_stream_capture_indicator_;
}

scoped_refptr<AudioStreamIndicator>
MediaCaptureDevicesDispatcher::GetAudioStreamIndicator() {
  return audio_stream_indicator_;
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
    const content::MediaStreamDevice& device,
    content::MediaRequestState state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &MediaCaptureDevicesDispatcher::UpdateMediaRequestStateOnUIThread,
          base::Unretained(this), render_process_id, render_view_id, device,
          state));

}

void MediaCaptureDevicesDispatcher::OnAudioStreamPlayingChanged(
    int render_process_id, int render_view_id, int stream_id,
    bool is_playing_and_audible) {
  audio_stream_indicator_->UpdateWebContentsStatus(render_process_id,
                                                   render_view_id,
                                                   stream_id,
                                                   is_playing_and_audible);
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
    const content::MediaStreamDevice& device,
    content::MediaRequestState state) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnRequestUpdate(render_process_id,
                                    render_view_id,
                                    device,
                                    state));
}
