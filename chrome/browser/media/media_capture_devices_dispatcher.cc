// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_capture_devices_dispatcher.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/media/audio_stream_indicator.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_devices_monitor.h"
#include "content/public/common/media_stream_request.h"

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
    PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kDefaultAudioCaptureDevice,
                               std::string(),
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kDefaultVideoCaptureDevice,
                               std::string(),
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
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

void MediaCaptureDevicesDispatcher::OnCaptureDevicesOpened(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices,
    const base::Closure& close_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  media_stream_capture_indicator_->CaptureDevicesOpened(
      render_process_id, render_view_id, devices, close_callback);
}

void MediaCaptureDevicesDispatcher::OnCaptureDevicesClosed(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  media_stream_capture_indicator_->CaptureDevicesClosed(render_process_id,
                                                        render_view_id,
                                                        devices);
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
    int render_process_id, int render_view_id, int stream_id, bool playing) {
  audio_stream_indicator_->UpdateWebContentsStatus(render_process_id,
                                                   render_view_id,
                                                   stream_id,
                                                   playing);
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
