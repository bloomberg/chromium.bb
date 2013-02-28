// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_manager.h"

#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/web_contents_video_capture_device.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/media_stream_request.h"
#include "media/video/capture/fake_video_capture_device.h"
#include "media/video/capture/screen/screen_capture_device.h"
#include "media/video/capture/video_capture_device.h"

namespace content {

// Starting id for the first capture session.
// VideoCaptureManager::kStartOpenSessionId is used as default id without
// explicitly calling open device.
enum { kFirstSessionId = VideoCaptureManager::kStartOpenSessionId + 1 };

struct VideoCaptureManager::Controller {
  Controller(
      VideoCaptureController* vc_controller,
      VideoCaptureControllerEventHandler* handler)
      : controller(vc_controller),
        ready_to_delete(false) {
    handlers.push_front(handler);
  }
  ~Controller() {}

  scoped_refptr<VideoCaptureController> controller;
  bool ready_to_delete;
  Handlers handlers;
};

VideoCaptureManager::VideoCaptureManager()
    : listener_(NULL),
      new_capture_session_id_(kFirstSessionId),
      use_fake_device_(false) {
}

VideoCaptureManager::~VideoCaptureManager() {
  DCHECK(devices_.empty());
  DCHECK(controllers_.empty());
}

void VideoCaptureManager::Register(MediaStreamProviderListener* listener,
                                   base::MessageLoopProxy* device_thread_loop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!listener_);
  DCHECK(!device_loop_);
  listener_ = listener;
  device_loop_ = device_thread_loop;
}

void VideoCaptureManager::Unregister() {
  DCHECK(listener_);
  listener_ = NULL;
}

void VideoCaptureManager::EnumerateDevices(MediaStreamType stream_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);
  device_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureManager::OnEnumerateDevices, this, stream_type));
}

int VideoCaptureManager::Open(const StreamDeviceInfo& device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);

  // Generate a new id for this device.
  int video_capture_session_id = new_capture_session_id_++;

  device_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureManager::OnOpen, this, video_capture_session_id,
                 device));

  return video_capture_session_id;
}

void VideoCaptureManager::Close(int capture_session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);
  device_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureManager::OnClose, this, capture_session_id));
}

void VideoCaptureManager::Start(
    const media::VideoCaptureParams& capture_params,
    media::VideoCaptureDevice::EventHandler* video_capture_receiver) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  device_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureManager::OnStart, this, capture_params,
                 video_capture_receiver));
}

void VideoCaptureManager::Stop(
    const media::VideoCaptureSessionId& capture_session_id,
    base::Closure stopped_cb) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  device_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureManager::OnStop, this, capture_session_id,
                 stopped_cb));
}

void VideoCaptureManager::UseFakeDevice() {
  use_fake_device_ = true;
}

void VideoCaptureManager::OnEnumerateDevices(MediaStreamType stream_type) {
  DCHECK(IsOnDeviceThread());

  media::VideoCaptureDevice::Names device_names;
  GetAvailableDevices(stream_type, &device_names);

  scoped_ptr<StreamDeviceInfoArray> devices(new StreamDeviceInfoArray());
  for (media::VideoCaptureDevice::Names::iterator it =
           device_names.begin(); it != device_names.end(); ++it) {
    bool opened = DeviceOpened(*it);
    devices->push_back(StreamDeviceInfo(
        stream_type, it->device_name, it->unique_id, opened));
  }

  PostOnDevicesEnumerated(stream_type, devices.Pass());
}

void VideoCaptureManager::OnOpen(int capture_session_id,
                                 const StreamDeviceInfo& device) {
  DCHECK(IsOnDeviceThread());
  DCHECK(devices_.find(capture_session_id) == devices_.end());
  DVLOG(1) << "VideoCaptureManager::OnOpen, id " << capture_session_id;

  // Check if another session has already opened this device. If so, just
  // use that opened device.
  media::VideoCaptureDevice* video_capture_device = GetOpenedDevice(device);
  if (video_capture_device) {
    DeviceEntry& new_entry = devices_[capture_session_id];
    new_entry.stream_type = device.device.type;
    new_entry.capture_device = video_capture_device;
    PostOnOpened(device.device.type, capture_session_id);
    return;
  }

  // Open the device.
  media::VideoCaptureDevice::Name vc_device_name;
  vc_device_name.device_name = device.device.name;
  vc_device_name.unique_id = device.device.id;

  if (use_fake_device_) {
    video_capture_device =
        media::FakeVideoCaptureDevice::Create(vc_device_name);
  } else {
    switch (device.device.type) {
      case MEDIA_DEVICE_VIDEO_CAPTURE: {
        video_capture_device =
            media::VideoCaptureDevice::Create(vc_device_name);
        break;
      }
      case MEDIA_TAB_VIDEO_CAPTURE: {
        video_capture_device = WebContentsVideoCaptureDevice::Create(
            vc_device_name.unique_id, base::Closure());
        break;
      }
      case MEDIA_SCREEN_VIDEO_CAPTURE: {
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
        CHECK(CommandLine::ForCurrentProcess()->HasSwitch(
                switches::kEnableUserMediaScreenCapturing));
        scoped_refptr<base::SequencedWorkerPool> blocking_pool =
            BrowserThread::GetBlockingPool();
        video_capture_device = new media::ScreenCaptureDevice(
            blocking_pool->GetSequencedTaskRunner(
                blocking_pool->GetSequenceToken()));
#endif  // defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
        break;
      }
      default: {
        NOTIMPLEMENTED();
        break;
      }
    }
  }
  if (!video_capture_device) {
    PostOnError(capture_session_id, kDeviceNotAvailable);
    return;
  }

  DeviceEntry& new_entry = devices_[capture_session_id];
  new_entry.stream_type = device.device.type;
  new_entry.capture_device = video_capture_device;
  PostOnOpened(device.device.type, capture_session_id);
}

void VideoCaptureManager::OnClose(int capture_session_id) {
  DCHECK(IsOnDeviceThread());
  DVLOG(1) << "VideoCaptureManager::OnClose, id " << capture_session_id;

  VideoCaptureDevices::iterator device_it = devices_.find(capture_session_id);
  if (device_it == devices_.end()) {
    return;
  }
  const DeviceEntry removed_entry = device_it->second;
  devices_.erase(device_it);

  Controllers::iterator cit = controllers_.find(removed_entry.capture_device);
  if (cit != controllers_.end()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&VideoCaptureController::StopSession,
                   cit->second->controller, capture_session_id));
  }

  if (!DeviceInUse(removed_entry.capture_device)) {
    // No other users of this device, deallocate (if not done already) and
    // delete the device. No need to take care of the controller, that is done
    // by |OnStop|.
    removed_entry.capture_device->DeAllocate();
    Controllers::iterator cit = controllers_.find(removed_entry.capture_device);
    if (cit != controllers_.end()) {
      delete cit->second;
      controllers_.erase(cit);
    }
    delete removed_entry.capture_device;
  }

  PostOnClosed(removed_entry.stream_type, capture_session_id);
}

void VideoCaptureManager::OnStart(
    const media::VideoCaptureParams capture_params,
    media::VideoCaptureDevice::EventHandler* video_capture_receiver) {
  DCHECK(IsOnDeviceThread());
  DCHECK(video_capture_receiver != NULL);
  DVLOG(1) << "VideoCaptureManager::OnStart, (" << capture_params.width
           << ", " << capture_params.height
           << ", " << capture_params.frame_per_second
           << ", " << capture_params.session_id
           << ")";

  media::VideoCaptureDevice* video_capture_device =
      GetDeviceInternal(capture_params.session_id);
  if (!video_capture_device) {
    // Invalid session id.
    video_capture_receiver->OnError();
    return;
  }
  Controllers::iterator cit = controllers_.find(video_capture_device);
  if (cit != controllers_.end()) {
    cit->second->ready_to_delete = false;
  }

  // Possible errors are signaled to video_capture_receiver by
  // video_capture_device. video_capture_receiver to perform actions.
  video_capture_device->Allocate(capture_params.width, capture_params.height,
                                 capture_params.frame_per_second,
                                 video_capture_receiver);
  video_capture_device->Start();
}

void VideoCaptureManager::OnStop(
    const media::VideoCaptureSessionId capture_session_id,
    base::Closure stopped_cb) {
  DCHECK(IsOnDeviceThread());
  DVLOG(1) << "VideoCaptureManager::OnStop, id " << capture_session_id;

  VideoCaptureDevices::iterator it = devices_.find(capture_session_id);
  if (it != devices_.end()) {
    media::VideoCaptureDevice* video_capture_device = it->second.capture_device;
    // Possible errors are signaled to video_capture_receiver by
    // video_capture_device. video_capture_receiver to perform actions.
    video_capture_device->Stop();
    video_capture_device->DeAllocate();
    Controllers::iterator cit = controllers_.find(video_capture_device);
    if (cit != controllers_.end()) {
      cit->second->ready_to_delete = true;
      if (cit->second->handlers.empty()) {
        delete cit->second;
        controllers_.erase(cit);
      }
    }
  }

  if (!stopped_cb.is_null())
    stopped_cb.Run();

  if (capture_session_id == kStartOpenSessionId) {
    // This device was opened from Start(), not Open(). Close it!
    OnClose(capture_session_id);
  }
}

void VideoCaptureManager::OnOpened(MediaStreamType stream_type,
                                   int capture_session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->Opened(stream_type, capture_session_id);
}

void VideoCaptureManager::OnClosed(MediaStreamType stream_type,
                                   int capture_session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->Closed(stream_type, capture_session_id);
}

void VideoCaptureManager::OnDevicesEnumerated(
    MediaStreamType stream_type,
    scoped_ptr<StreamDeviceInfoArray> devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->DevicesEnumerated(stream_type, *devices);
}

void VideoCaptureManager::OnError(MediaStreamType stream_type,
                                  int capture_session_id,
                                  MediaStreamProviderError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->Error(stream_type, capture_session_id, error);
}

void VideoCaptureManager::PostOnOpened(
    MediaStreamType stream_type, int capture_session_id) {
  DCHECK(IsOnDeviceThread());
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&VideoCaptureManager::OnOpened, this,
                                     stream_type, capture_session_id));
}

void VideoCaptureManager::PostOnClosed(
    MediaStreamType stream_type, int capture_session_id) {
  DCHECK(IsOnDeviceThread());
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&VideoCaptureManager::OnClosed, this,
                                     stream_type, capture_session_id));
}

void VideoCaptureManager::PostOnDevicesEnumerated(
    MediaStreamType stream_type,
    scoped_ptr<StreamDeviceInfoArray> devices) {
  DCHECK(IsOnDeviceThread());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureManager::OnDevicesEnumerated,
                 this, stream_type, base::Passed(&devices)));
}

void VideoCaptureManager::PostOnError(int capture_session_id,
                                      MediaStreamProviderError error) {
  DCHECK(IsOnDeviceThread());
  MediaStreamType stream_type = MEDIA_DEVICE_VIDEO_CAPTURE;
  VideoCaptureDevices::const_iterator it = devices_.find(capture_session_id);
  if (it != devices_.end())
    stream_type = it->second.stream_type;
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&VideoCaptureManager::OnError, this,
                                     stream_type, capture_session_id, error));
}

bool VideoCaptureManager::IsOnDeviceThread() const {
  return device_loop_->BelongsToCurrentThread();
}

void VideoCaptureManager::GetAvailableDevices(
    MediaStreamType stream_type,
    media::VideoCaptureDevice::Names* device_names) {
  DCHECK(IsOnDeviceThread());

  switch (stream_type) {
    case MEDIA_DEVICE_VIDEO_CAPTURE:
      if (!use_fake_device_) {
        media::VideoCaptureDevice::GetDeviceNames(device_names);
      } else {
        media::FakeVideoCaptureDevice::GetDeviceNames(device_names);
      }
      break;

    case MEDIA_SCREEN_VIDEO_CAPTURE:
      device_names->clear();
      break;

    default:
      NOTREACHED();
      break;
  }
}

bool VideoCaptureManager::DeviceOpened(
    const media::VideoCaptureDevice::Name& device_name) {
  DCHECK(IsOnDeviceThread());

  for (VideoCaptureDevices::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if (device_name.unique_id ==
            it->second.capture_device->device_name().unique_id) {
      // We've found the device!
      return true;
    }
  }
  return false;
}

media::VideoCaptureDevice* VideoCaptureManager::GetOpenedDevice(
    const StreamDeviceInfo& device_info) {
  DCHECK(IsOnDeviceThread());

  for (VideoCaptureDevices::iterator it = devices_.begin();
       it != devices_.end(); it++) {
    if (device_info.device.id ==
            it->second.capture_device->device_name().unique_id) {
      return it->second.capture_device;
    }
  }
  return NULL;
}

bool VideoCaptureManager::DeviceInUse(
    const media::VideoCaptureDevice* video_capture_device) {
  DCHECK(IsOnDeviceThread());

  for (VideoCaptureDevices::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if (video_capture_device == it->second.capture_device) {
      // We've found the device!
      return true;
    }
  }
  return false;
}

void VideoCaptureManager::AddController(
    const media::VideoCaptureParams& capture_params,
    VideoCaptureControllerEventHandler* handler,
    base::Callback<void(VideoCaptureController*)> added_cb) {
  DCHECK(handler);
  device_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureManager::DoAddControllerOnDeviceThread,
                 this, capture_params, handler, added_cb));
}

void VideoCaptureManager::DoAddControllerOnDeviceThread(
    const media::VideoCaptureParams capture_params,
    VideoCaptureControllerEventHandler* handler,
    base::Callback<void(VideoCaptureController*)> added_cb) {
  DCHECK(IsOnDeviceThread());

  media::VideoCaptureDevice* video_capture_device =
      GetDeviceInternal(capture_params.session_id);
  scoped_refptr<VideoCaptureController> controller;
  if (video_capture_device) {
    Controllers::iterator cit = controllers_.find(video_capture_device);
    if (cit == controllers_.end()) {
      controller = new VideoCaptureController(this);
      controllers_[video_capture_device] = new Controller(controller, handler);
    } else {
      controllers_[video_capture_device]->handlers.push_front(handler);
      controller = controllers_[video_capture_device]->controller;
    }
  }
  added_cb.Run(controller);
}

void VideoCaptureManager::RemoveController(
    VideoCaptureController* controller,
    VideoCaptureControllerEventHandler* handler) {
  DCHECK(handler);
  device_loop_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureManager::DoRemoveControllerOnDeviceThread, this,
                 make_scoped_refptr(controller), handler));
}

void VideoCaptureManager::DoRemoveControllerOnDeviceThread(
    VideoCaptureController* controller,
    VideoCaptureControllerEventHandler* handler) {
  DCHECK(IsOnDeviceThread());

  for (Controllers::iterator cit = controllers_.begin();
       cit != controllers_.end(); ++cit) {
    if (controller == cit->second->controller) {
      Handlers& handlers = cit->second->handlers;
      for (Handlers::iterator hit = handlers.begin();
           hit != handlers.end(); ++hit) {
        if ((*hit) == handler) {
          handlers.erase(hit);
          break;
        }
      }
      if (handlers.empty() && cit->second->ready_to_delete) {
        delete cit->second;
        controllers_.erase(cit);
      }
      return;
    }
  }
}

media::VideoCaptureDevice* VideoCaptureManager::GetDeviceInternal(
    int capture_session_id) {
  DCHECK(IsOnDeviceThread());
  VideoCaptureDevices::iterator dit = devices_.find(capture_session_id);
  if (dit != devices_.end()) {
    return dit->second.capture_device;
  }

  // Solution for not using MediaStreamManager.
  // This session id won't be returned by Open().
  if (capture_session_id == kStartOpenSessionId) {
    media::VideoCaptureDevice::Names device_names;
    GetAvailableDevices(MEDIA_DEVICE_VIDEO_CAPTURE, &device_names);
    if (device_names.empty()) {
      // No devices available.
      return NULL;
    }
    StreamDeviceInfo device(MEDIA_DEVICE_VIDEO_CAPTURE,
                            device_names.front().device_name,
                            device_names.front().unique_id, false);

    // Call OnOpen to open using the first device in the list.
    OnOpen(capture_session_id, device);

    VideoCaptureDevices::iterator dit = devices_.find(capture_session_id);
    if (dit != devices_.end()) {
      return dit->second.capture_device;
    }
  }
  return NULL;
}

}  // namespace content
