// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_manager.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/browser_thread.h"
#include "media/video/capture/fake_video_capture_device.h"
#include "media/video/capture/video_capture_device.h"

namespace media_stream {

// Starting id for the first capture session.
// VideoCaptureManager::kStartOpenSessionId is used as default id without
// explicitly calling open device.
enum { kFirstSessionId = VideoCaptureManager::kStartOpenSessionId + 1 };

VideoCaptureManager::VideoCaptureManager()
  : vc_device_thread_("VideoCaptureManagerThread"),
    listener_(NULL),
    new_capture_session_id_(kFirstSessionId),
    use_fake_device_(false) {
  vc_device_thread_.Start();
}

VideoCaptureManager::~VideoCaptureManager() {
  vc_device_thread_.Stop();
  // TODO(mflodman) Remove this temporary solution when shut-down issue is
  // resolved, i.e. all code below this comment.
  // Temporary solution: close all open devices and delete them, after the
  // thread is stopped.
  DLOG_IF(ERROR, !devices_.empty()) << "VideoCaptureManager: Open devices!";
  for (VideoCaptureDevices::iterator it = devices_.begin();
       it != devices_.end();
       ++it) {
    it->second->DeAllocate();
    delete it->second;
  }
}

void VideoCaptureManager::Register(MediaStreamProviderListener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!listener_);
  listener_ = listener;
}

void VideoCaptureManager::Unregister() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);
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

int VideoCaptureManager::Open(const StreamDeviceInfo& device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);

  // Generate a new id for this device.
  int video_capture_session_id = new_capture_session_id_++;

  vc_device_thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &VideoCaptureManager::OnOpen,
                        video_capture_session_id,
                        device));

  return video_capture_session_id;
}

void VideoCaptureManager::Close(int capture_session_id) {
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
    const media::VideoCaptureSessionId& capture_session_id,
    Task* stopped_task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  vc_device_thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &VideoCaptureManager::OnStop,
                        capture_session_id,
                        stopped_task));
}

void VideoCaptureManager::Error(
    const media::VideoCaptureSessionId& capture_session_id) {
  PostOnError(capture_session_id, kDeviceNotAvailable);
}

void VideoCaptureManager::UseFakeDevice() {
  use_fake_device_ = true;
}

MessageLoop* VideoCaptureManager::GetMessageLoop() {
  return vc_device_thread_.message_loop();
}

void VideoCaptureManager::OnEnumerateDevices() {
  DCHECK(IsOnCaptureDeviceThread());

  media::VideoCaptureDevice::Names device_names;
  GetAvailableDevices(&device_names);

  StreamDeviceInfoArray devices;
  for (media::VideoCaptureDevice::Names::iterator it =
           device_names.begin(); it != device_names.end(); ++it) {
    bool opened = DeviceOpened(*it);
    devices.push_back(StreamDeviceInfo(kVideoCapture, it->device_name,
                                       it->unique_id, opened));
  }

  PostOnDevicesEnumerated(devices);
}

void VideoCaptureManager::OnOpen(int capture_session_id,
                                 const StreamDeviceInfo& device) {
  DCHECK(IsOnCaptureDeviceThread());
  DCHECK(devices_.find(capture_session_id) == devices_.end());

  // Check if another session has already opened this device, only one user per
  // device is supported.
  if (DeviceOpened(device)) {
    PostOnError(capture_session_id, kDeviceAlreadyInUse);
    return;
  }

  // Open the device.
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
  if (!video_capture_device) {
    PostOnError(capture_session_id, kDeviceNotAvailable);
    return;
  }

  devices_[capture_session_id] = video_capture_device;
  PostOnOpened(capture_session_id);
}

void VideoCaptureManager::OnClose(int capture_session_id) {
  DCHECK(IsOnCaptureDeviceThread());

  VideoCaptureDevices::iterator it = devices_.find(capture_session_id);
  if (it != devices_.end()) {
    // Deallocate (if not done already) and delete the device.
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
  DCHECK(video_capture_receiver != NULL);

  // Solution for not using MediaStreamManager.
  // This session id won't be returned by Open().
  if (capture_params.session_id == kStartOpenSessionId) {
    // Start() is called without using Open(), we need to open a device.
    media::VideoCaptureDevice::Names device_names;
    GetAvailableDevices(&device_names);
    if (device_names.empty()) {
      // No devices available.
      video_capture_receiver->OnError();
      return;
    }
    StreamDeviceInfo device(kVideoCapture,
                            device_names.front().device_name,
                            device_names.front().unique_id, false);

    // Call OnOpen to open using the first device in the list.
    OnOpen(capture_params.session_id, device);
  }

  VideoCaptureDevices::iterator it = devices_.find(capture_params.session_id);
  if (it == devices_.end()) {
    // Invalid session id.
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

void VideoCaptureManager::OnOpened(int capture_session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->Opened(kVideoCapture, capture_session_id);
}

void VideoCaptureManager::OnClosed(int capture_session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->Closed(kVideoCapture, capture_session_id);
}

void VideoCaptureManager::OnDevicesEnumerated(
    const StreamDeviceInfoArray& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->DevicesEnumerated(kVideoCapture, devices);
}

void VideoCaptureManager::OnError(int capture_session_id,
                                  MediaStreamProviderError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->Error(kVideoCapture, capture_session_id, error);
}

void VideoCaptureManager::PostOnOpened(int capture_session_id) {
  DCHECK(IsOnCaptureDeviceThread());
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          NewRunnableMethod(this,
                                            &VideoCaptureManager::OnOpened,
                                            capture_session_id));
}

void VideoCaptureManager::PostOnClosed(int capture_session_id) {
  DCHECK(IsOnCaptureDeviceThread());
  BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            NewRunnableMethod(this,
                                              &VideoCaptureManager::OnClosed,
                                              capture_session_id));
}

void VideoCaptureManager::PostOnDevicesEnumerated(
    const StreamDeviceInfoArray& devices) {
  DCHECK(IsOnCaptureDeviceThread());
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          NewRunnableMethod(
                              this,
                              &VideoCaptureManager::OnDevicesEnumerated,
                              devices));
}

void VideoCaptureManager::PostOnError(int capture_session_id,
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

bool VideoCaptureManager::DeviceOpened(const StreamDeviceInfo& device_info) {
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

}  // namespace media_stream
