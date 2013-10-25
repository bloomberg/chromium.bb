// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_manager.h"

#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/web_contents_video_capture_device.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/desktop_media_id.h"
#include "content/public/common/media_stream_request.h"
#include "media/base/scoped_histogram_timer.h"
#include "media/video/capture/fake_video_capture_device.h"
#include "media/video/capture/video_capture_device.h"

#if defined(ENABLE_SCREEN_CAPTURE)
#include "content/browser/renderer_host/media/desktop_capture_device.h"
#endif

namespace content {

VideoCaptureManager::DeviceEntry::DeviceEntry(
    MediaStreamType stream_type,
    const std::string& id,
    scoped_ptr<VideoCaptureController> controller)
    : stream_type(stream_type),
      id(id),
      video_capture_controller(controller.Pass()) {}

VideoCaptureManager::DeviceEntry::~DeviceEntry() {}

VideoCaptureManager::VideoCaptureManager()
    : listener_(NULL),
      new_capture_session_id_(1),
      use_fake_device_(false) {
}

VideoCaptureManager::~VideoCaptureManager() {
  DCHECK(devices_.empty());
}

void VideoCaptureManager::Register(MediaStreamProviderListener* listener,
                                   base::MessageLoopProxy* device_thread_loop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!listener_);
  DCHECK(!device_loop_.get());
  listener_ = listener;
  device_loop_ = device_thread_loop;
}

void VideoCaptureManager::Unregister() {
  DCHECK(listener_);
  listener_ = NULL;
}

void VideoCaptureManager::EnumerateDevices(MediaStreamType stream_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureManager::EnumerateDevices, type " << stream_type;
  DCHECK(listener_);
  base::PostTaskAndReplyWithResult(
      device_loop_, FROM_HERE,
      base::Bind(&VideoCaptureManager::GetAvailableDevicesOnDeviceThread, this,
                 stream_type),
      base::Bind(&VideoCaptureManager::OnDevicesEnumerated, this, stream_type));
}

int VideoCaptureManager::Open(const StreamDeviceInfo& device_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);

  // Generate a new id for the session being opened.
  const int capture_session_id = new_capture_session_id_++;

  DCHECK(sessions_.find(capture_session_id) == sessions_.end());
  DVLOG(1) << "VideoCaptureManager::Open, id " << capture_session_id;

  // We just save the stream info for processing later.
  sessions_[capture_session_id] = device_info.device;

  // Notify our listener asynchronously; this ensures that we return
  // |capture_session_id| to the caller of this function before using that same
  // id in a listener event.
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureManager::OnOpened, this,
                 device_info.device.type, capture_session_id));
  return capture_session_id;
}

void VideoCaptureManager::Close(int capture_session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);
  DVLOG(1) << "VideoCaptureManager::Close, id " << capture_session_id;

  std::map<int, MediaStreamDevice>::iterator session_it =
      sessions_.find(capture_session_id);
  if (session_it == sessions_.end()) {
    NOTREACHED();
    return;
  }

  DeviceEntry* const existing_device = GetDeviceEntryForMediaStreamDevice(
      session_it->second);
  if (existing_device) {
    // Remove any client that is still using the session. This is safe to call
    // even if there are no clients using the session.
    existing_device->video_capture_controller->StopSession(capture_session_id);

    // StopSession() may have removed the last client, so we might need to
    // close the device.
    DestroyDeviceEntryIfNoClients(existing_device);
  }

  // Notify listeners asynchronously, and forget the session.
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureManager::OnClosed, this, session_it->second.type,
                 capture_session_id));
  sessions_.erase(session_it);
}

void VideoCaptureManager::UseFakeDevice() {
  use_fake_device_ = true;
}

void VideoCaptureManager::DoStartDeviceOnDeviceThread(
    DeviceEntry* entry,
    const media::VideoCaptureCapability& capture_params,
    scoped_ptr<media::VideoCaptureDevice::Client> device_client) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(IsOnDeviceThread());

  scoped_ptr<media::VideoCaptureDevice> video_capture_device;
  switch (entry->stream_type) {
    case MEDIA_DEVICE_VIDEO_CAPTURE: {
      // We look up the device id from the renderer in our local enumeration
      // since the renderer does not have all the information that might be
      // held in the browser-side VideoCaptureDevice::Name structure.
      media::VideoCaptureDevice::Name* found =
          video_capture_devices_.FindById(entry->id);
      if (found) {
        video_capture_device.reset(use_fake_device_ ?
            media::FakeVideoCaptureDevice::Create(*found) :
            media::VideoCaptureDevice::Create(*found));
      }
      break;
    }
    case MEDIA_TAB_VIDEO_CAPTURE: {
      video_capture_device.reset(
          WebContentsVideoCaptureDevice::Create(entry->id));
      break;
    }
    case MEDIA_DESKTOP_VIDEO_CAPTURE: {
#if defined(ENABLE_SCREEN_CAPTURE)
      DesktopMediaID id = DesktopMediaID::Parse(entry->id);
      if (id.type != DesktopMediaID::TYPE_NONE) {
        video_capture_device = DesktopCaptureDevice::Create(id);
      }
#endif  // defined(ENABLE_SCREEN_CAPTURE)
      break;
    }
    default: {
      NOTIMPLEMENTED();
      break;
    }
  }

  if (!video_capture_device) {
    device_client->OnError();
    return;
  }

  video_capture_device->AllocateAndStart(capture_params, device_client.Pass());
  entry->video_capture_device = video_capture_device.Pass();
}

void VideoCaptureManager::StartCaptureForClient(
    const media::VideoCaptureParams& params,
    base::ProcessHandle client_render_process,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler,
    const DoneCB& done_cb) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureManager::StartCaptureForClient, ("
         << params.requested_format.width
         << ", " << params.requested_format.height
         << ", " << params.requested_format.frame_rate
         << ", #" << params.session_id
         << ")";

  DeviceEntry* entry = GetOrCreateDeviceEntry(params.session_id);
  if (!entry) {
    done_cb.Run(base::WeakPtr<VideoCaptureController>());
    return;
  }

  DCHECK(entry->video_capture_controller);

  // First client starts the device.
  if (entry->video_capture_controller->GetClientCount() == 0) {
    DVLOG(1) << "VideoCaptureManager starting device (type = "
             << entry->stream_type << ", id = " << entry->id << ")";

    media::VideoCaptureCapability params_as_capability;
    params_as_capability.width = params.requested_format.width;
    params_as_capability.height = params.requested_format.height;
    params_as_capability.frame_rate = params.requested_format.frame_rate;
    params_as_capability.frame_size_type =
        params.requested_format.frame_size_type;

    device_loop_->PostTask(FROM_HERE, base::Bind(
        &VideoCaptureManager::DoStartDeviceOnDeviceThread, this,
        entry, params_as_capability,
        base::Passed(entry->video_capture_controller->NewDeviceClient())));
  }
  // Run the callback first, as AddClient() may trigger OnFrameInfo().
  done_cb.Run(entry->video_capture_controller->GetWeakPtr());
  entry->video_capture_controller->AddClient(client_id,
                                             client_handler,
                                             client_render_process,
                                             params);
}

void VideoCaptureManager::StopCaptureForClient(
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(controller);
  DCHECK(client_handler);

  DeviceEntry* entry = GetDeviceEntryForController(controller);
  if (!entry) {
    NOTREACHED();
    return;
  }

  // Detach client from controller.
  int session_id = controller->RemoveClient(client_id, client_handler);
  DVLOG(1) << "VideoCaptureManager::StopCaptureForClient, session_id = "
           << session_id;

  // If controller has no more clients, delete controller and device.
  DestroyDeviceEntryIfNoClients(entry);
}

void VideoCaptureManager::DoStopDeviceOnDeviceThread(DeviceEntry* entry) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StopDeviceTime");
  DCHECK(IsOnDeviceThread());
  if (entry->video_capture_device) {
    entry->video_capture_device->StopAndDeAllocate();
  }
  entry->video_capture_device.reset();
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
    const media::VideoCaptureDevice::Names& device_names) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!listener_) {
    // Listener has been removed.
    return;
  }

  // Transform from VCD::Name to StreamDeviceInfo.
  StreamDeviceInfoArray devices;
  for (media::VideoCaptureDevice::Names::const_iterator it =
           device_names.begin(); it != device_names.end(); ++it) {
    devices.push_back(StreamDeviceInfo(
        stream_type, it->GetNameAndModel(), it->id()));
  }

  listener_->DevicesEnumerated(stream_type, devices);
}

bool VideoCaptureManager::IsOnDeviceThread() const {
  return device_loop_->BelongsToCurrentThread();
}

media::VideoCaptureDevice::Names
VideoCaptureManager::GetAvailableDevicesOnDeviceThread(
    MediaStreamType stream_type) {
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Media.VideoCaptureManager.GetAvailableDevicesTime");
  DCHECK(IsOnDeviceThread());
  media::VideoCaptureDevice::Names result;

  switch (stream_type) {
    case MEDIA_DEVICE_VIDEO_CAPTURE:
      // Cache the latest enumeration of video capture devices.
      // We'll refer to this list again in OnOpen to avoid having to
      // enumerate the devices again.
      if (!use_fake_device_) {
        media::VideoCaptureDevice::GetDeviceNames(&result);
      } else {
        media::FakeVideoCaptureDevice::GetDeviceNames(&result);
      }

      // TODO(nick): The correctness of device start depends on this cache being
      // maintained, but it seems a little odd to keep a cache here. Can we
      // eliminate it?
      video_capture_devices_ = result;
      break;

    case MEDIA_DESKTOP_VIDEO_CAPTURE:
      // Do nothing.
      break;

    default:
      NOTREACHED();
      break;
  }
  return result;
}

VideoCaptureManager::DeviceEntry*
VideoCaptureManager::GetDeviceEntryForMediaStreamDevice(
    const MediaStreamDevice& device_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (DeviceEntries::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    DeviceEntry* device = *it;
    if (device_info.type == device->stream_type &&
        device_info.id == device->id) {
      return device;
    }
  }
  return NULL;
}

VideoCaptureManager::DeviceEntry*
VideoCaptureManager::GetDeviceEntryForController(
    const VideoCaptureController* controller) {
  // Look up |controller| in |devices_|.
  for (DeviceEntries::iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if ((*it)->video_capture_controller.get() == controller) {
      return *it;
    }
  }
  return NULL;
}

void VideoCaptureManager::DestroyDeviceEntryIfNoClients(DeviceEntry* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Removal of the last client stops the device.
  if (entry->video_capture_controller->GetClientCount() == 0) {
    DVLOG(1) << "VideoCaptureManager stopping device (type = "
             << entry->stream_type << ", id = " << entry->id << ")";

    // The DeviceEntry is removed from |devices_| immediately. The controller is
    // deleted immediately, and the device is freed asynchronously. After this
    // point, subsequent requests to open this same device ID will create a new
    // DeviceEntry, VideoCaptureController, and VideoCaptureDevice.
    devices_.erase(entry);
    entry->video_capture_controller.reset();
    device_loop_->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureManager::DoStopDeviceOnDeviceThread, this,
                   base::Owned(entry)));
  }
}

VideoCaptureManager::DeviceEntry* VideoCaptureManager::GetOrCreateDeviceEntry(
    int capture_session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  std::map<int, MediaStreamDevice>::iterator session_it =
      sessions_.find(capture_session_id);
  if (session_it == sessions_.end()) {
    return NULL;
  }
  const MediaStreamDevice& device_info = session_it->second;

  // Check if another session has already opened this device. If so, just
  // use that opened device.
  DeviceEntry* const existing_device =
       GetDeviceEntryForMediaStreamDevice(device_info);
  if (existing_device) {
    DCHECK_EQ(device_info.type, existing_device->stream_type);
    return existing_device;
  }

  scoped_ptr<VideoCaptureController> video_capture_controller(
      new VideoCaptureController());
  DeviceEntry* new_device = new DeviceEntry(device_info.type,
                                            device_info.id,
                                            video_capture_controller.Pass());
  devices_.insert(new_device);
  return new_device;
}

}  // namespace content
