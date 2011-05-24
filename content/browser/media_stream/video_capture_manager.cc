// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media_stream/video_capture_manager.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/browser_thread.h"
#include "media/video/capture/fake_video_capture_device.h"
#include "media/video/capture/video_capture_device.h"

namespace media_stream {

// Starting id for the first capture session.
// VideoCaptureManager::kStartOpenSessionId is used as default id without
// explicitly calling open device.
enum { kFirstSessionId = VideoCaptureManager::kStartOpenSessionId + 1 };

static ::base::LazyInstance<VideoCaptureManager> g_video_capture_manager(
    base::LINKER_INITIALIZED);

VideoCaptureManager* VideoCaptureManager::Get() {
  return g_video_capture_manager.Pointer();
}

VideoCaptureManager::VideoCaptureManager()
  : vc_device_thread_("VideoCaptureManagerThread"),
    listener_(NULL),
    new_capture_session_id_(kFirstSessionId),
    devices_(),
    use_fake_device_(false) {
  vc_device_thread_.Start();
}

VideoCaptureManager::~VideoCaptureManager() {
  vc_device_thread_.Stop();
}

bool VideoCaptureManager::Register(MediaStreamProviderListener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!listener_);
  listener_ = listener;
  return true;
}

void VideoCaptureManager::Unregister() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  listener_ = NULL;
}

void VideoCaptureManager::EnumerateDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);

  vc_device_thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &VideoCaptureManager::OnEnumerateDevices));
}

MediaCaptureSessionId VideoCaptureManager::Open(
    const MediaCaptureDeviceInfo& device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);

  // Generate a new id for this device
  MediaCaptureSessionId video_capture_session_id = new_capture_session_id_++;

  vc_device_thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &VideoCaptureManager::OnOpen,
                        video_capture_session_id,
                        device));

  return video_capture_session_id;
}

void VideoCaptureManager::Close(MediaCaptureSessionId capture_session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);

  vc_device_thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &VideoCaptureManager::OnClose,
                        capture_session_id));
}

void VideoCaptureManager::Start(
    const media::VideoCaptureParams& capture_params,
    media::VideoCaptureDevice::EventHandler* video_capture_receiver) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  vc_device_thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &VideoCaptureManager::OnStart,
                        capture_params,
                        video_capture_receiver));
}

void VideoCaptureManager::Stop(
    const media::VideoCaptureSessionId capture_session_id, Task* stopped_task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  vc_device_thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &VideoCaptureManager::OnStop,
                        capture_session_id,
                        stopped_task));
}

void VideoCaptureManager::UseFakeDevice() {
  use_fake_device_ = true;
}

MessageLoop* VideoCaptureManager::GetMessageLoop() {
  return vc_device_thread_.message_loop();
}

void VideoCaptureManager::OnEnumerateDevices() {
  DCHECK(IsOnCaptureDeviceThread());

  scoped_ptr<media::VideoCaptureDevice::Names> device_names(
      new media::VideoCaptureDevice::Names());
  GetAvailableDevices(device_names.get());

  MediaCaptureDevices devices;
  for (media::VideoCaptureDevice::Names::iterator it =
           device_names.get()->begin(); it != device_names.get()->end(); ++it) {
    bool opened = DeviceOpened(*it);
    devices.push_back(MediaCaptureDeviceInfo(kVideoCapture, it->device_name,
                                             it->unique_id, opened));
  }

  PostOnDevicesEnumerated(devices);

  // Clean-up
  devices.clear();
  device_names.get()->clear();
}

void VideoCaptureManager::OnOpen(MediaCaptureSessionId capture_session_id,
                                 const MediaCaptureDeviceInfo device) {
  DCHECK(IsOnCaptureDeviceThread());
  DCHECK(devices_.find(capture_session_id) == devices_.end());

  // Check if another session has already opened this device, only one user per
  // device is supported.
  if (DeviceOpened(device)) {
    PostOnError(capture_session_id, kDeviceAlreadyInUse);
    return;
  }

  // Open the device
  media::VideoCaptureDevice::Name vc_device_name;
  vc_device_name.device_name = device.name;
  vc_device_name.unique_id = device.device_id;

  media::VideoCaptureDevice* video_capture_device = NULL;
  if (!use_fake_device_) {
    video_capture_device = media::VideoCaptureDevice::Create(vc_device_name);
  } else {
    video_capture_device =
        media::FakeVideoCaptureDevice::Create(vc_device_name);
  }
  if (video_capture_device == NULL) {
    PostOnError(capture_session_id, kDeviceNotAvailable);
    return;
  }

  devices_[capture_session_id] = video_capture_device;
  PostOnOpened(capture_session_id);
}

void VideoCaptureManager::OnClose(
    MediaCaptureSessionId capture_session_id) {
  DCHECK(IsOnCaptureDeviceThread());

  VideoCaptureDevices::iterator it = devices_.find(capture_session_id);
  if (it != devices_.end()) {
    // Deallocate (if not done already) and delete the device
    media::VideoCaptureDevice* video_capture_device = it->second;
    video_capture_device->DeAllocate();
    delete video_capture_device;
    devices_.erase(it);
  }

  PostOnClosed(capture_session_id);
}

void VideoCaptureManager::OnStart(
    const media::VideoCaptureParams capture_params,
    media::VideoCaptureDevice::EventHandler* video_capture_receiver) {
  DCHECK(IsOnCaptureDeviceThread());

  // Solution for not using MediaStreamManager
  // This session id won't be returned by Open()
  if (capture_params.session_id == kStartOpenSessionId) {
    // Start() is called without using Open(), we need to open a device
    scoped_ptr<media::VideoCaptureDevice::Names> device_names(
        new media::VideoCaptureDevice::Names());
    GetAvailableDevices(device_names.get());

    MediaCaptureDeviceInfo device(kVideoCapture,
                                  device_names.get()->front().device_name,
                                  device_names.get()->front().unique_id, false);

    // Call OnOpen to open using the first device in the list
    OnOpen(capture_params.session_id, device);
  }

  VideoCaptureDevices::iterator it = devices_.find(capture_params.session_id);
  if (it == devices_.end()) {
    // Invalid session id
    video_capture_receiver->OnError();
    return;
  }
  media::VideoCaptureDevice* video_capture_device = it->second;

  // Possible errors are signaled to video_capture_receiver by
  // video_capture_device. video_capture_receiver to perform actions.
  video_capture_device->Allocate(capture_params.width, capture_params.height,
                                 capture_params.frame_per_second,
                                 video_capture_receiver);
  video_capture_device->Start();
}

void VideoCaptureManager::OnStop(
    const media::VideoCaptureSessionId capture_session_id,
    Task* stopped_task) {
  DCHECK(IsOnCaptureDeviceThread());

  VideoCaptureDevices::iterator it = devices_.find(capture_session_id);
  if (it != devices_.end()) {
    media::VideoCaptureDevice* video_capture_device = it->second;
    // Possible errors are signaled to video_capture_receiver by
    // video_capture_device. video_capture_receiver to perform actions.
    video_capture_device->Stop();
    video_capture_device->DeAllocate();
  }

  if (stopped_task) {
    stopped_task->Run();
    delete stopped_task;
  }

  if (capture_session_id == kStartOpenSessionId) {
    // This device was opened from Start(), not Open(). Close it!
    OnClose(capture_session_id);
  }
}

void VideoCaptureManager::OnOpened(
    MediaCaptureSessionId capture_session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (listener_ == NULL) {
    // Listener has been removed
    return;
  }
  listener_->Opened(kVideoCapture, capture_session_id);
}

void VideoCaptureManager::OnClosed(
    MediaCaptureSessionId capture_session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (listener_ == NULL) {
    // Listener has been removed
    return;
  }
  listener_->Closed(kVideoCapture, capture_session_id);
}

void VideoCaptureManager::OnDevicesEnumerated(
    const MediaCaptureDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (listener_ == NULL) {
    // Listener has been removed
    return;
  }
  listener_->DevicesEnumerated(kVideoCapture, devices);
}

void VideoCaptureManager::OnError(MediaCaptureSessionId capture_session_id,
                                  MediaStreamProviderError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (listener_ == NULL) {
    // Listener has been removed
    return;
  }
  listener_->Error(kVideoCapture, capture_session_id, error);
}

void VideoCaptureManager::PostOnOpened(
    MediaCaptureSessionId capture_session_id) {
  DCHECK(IsOnCaptureDeviceThread());
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          NewRunnableMethod(this,
                                            &VideoCaptureManager::OnOpened,
                                            capture_session_id));
}

void VideoCaptureManager::PostOnClosed(
    MediaCaptureSessionId capture_session_id) {
  DCHECK(IsOnCaptureDeviceThread());
  BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            NewRunnableMethod(this,
                                              &VideoCaptureManager::OnClosed,
                                              capture_session_id));
}

void VideoCaptureManager::PostOnDevicesEnumerated(MediaCaptureDevices devices) {
  DCHECK(IsOnCaptureDeviceThread());

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          NewRunnableMethod(
                              this,
                              &VideoCaptureManager::OnDevicesEnumerated,
                              devices));
}

void VideoCaptureManager::PostOnError(MediaCaptureSessionId capture_session_id,
                                      MediaStreamProviderError error) {
  // Don't check thread here, can be called from both IO thread and device
  // thread.
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          NewRunnableMethod(this,
                                            &VideoCaptureManager::OnError,
                                            capture_session_id,
                                            error));
}

bool VideoCaptureManager::IsOnCaptureDeviceThread() const {
  return MessageLoop::current() == vc_device_thread_.message_loop();
}

void VideoCaptureManager::GetAvailableDevices(
    media::VideoCaptureDevice::Names* device_names) {
  DCHECK(IsOnCaptureDeviceThread());

  if (!use_fake_device_) {
     media::VideoCaptureDevice::GetDeviceNames(device_names);
  } else {
    media::FakeVideoCaptureDevice::GetDeviceNames(device_names);
  }
}

bool VideoCaptureManager::DeviceOpened(
    const media::VideoCaptureDevice::Name& device_name) {
  DCHECK(IsOnCaptureDeviceThread());

  for (VideoCaptureDevices::iterator it = devices_.begin();
      it != devices_.end();
      ++it) {
    if (device_name.unique_id == it->second->device_name().unique_id) {
      // We've found the device!
      return true;
    }
  }
  return false;
}

bool VideoCaptureManager::DeviceOpened(
    const MediaCaptureDeviceInfo& device_info) {
  DCHECK(IsOnCaptureDeviceThread());

  for (VideoCaptureDevices::iterator it = devices_.begin();
      it != devices_.end();
      it++) {
    if (device_info.device_id == it->second->device_name().unique_id) {
      return true;
    }
  }
  return false;
}

}  // namespace media
