// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_manager.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/common/media_stream_request.h"
#include "media/base/bind_to_current_loop.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_device_factory.h"

#if defined(ENABLE_SCREEN_CAPTURE)
#include "content/browser/media/capture/desktop_capture_device.h"
#if defined(USE_AURA)
#include "content/browser/media/capture/desktop_capture_device_aura.h"
#endif
#endif

namespace {

// Compares two VideoCaptureFormat by checking smallest frame_size area, then
// by _largest_ frame_rate. Used to order a VideoCaptureFormats vector so that
// the first entry for a given resolution has the largest frame rate, as needed
// by the ConsolidateCaptureFormats() method.
bool IsCaptureFormatSmaller(const media::VideoCaptureFormat& format1,
                            const media::VideoCaptureFormat& format2) {
  if (format1.frame_size.GetArea() == format2.frame_size.GetArea())
    return format1.frame_rate > format2.frame_rate;
  return format1.frame_size.GetArea() < format2.frame_size.GetArea();
}

bool IsCaptureFormatSizeEqual(const media::VideoCaptureFormat& format1,
                              const media::VideoCaptureFormat& format2) {
  return format1.frame_size.GetArea() == format2.frame_size.GetArea();
}

// This function receives a list of capture formats, removes duplicated
// resolutions while keeping the highest frame rate for each, and forcing I420
// pixel format.
void ConsolidateCaptureFormats(media::VideoCaptureFormats* formats) {
  if (formats->empty())
    return;
  std::sort(formats->begin(), formats->end(), IsCaptureFormatSmaller);
  // Due to the ordering imposed, the largest frame_rate is kept while removing
  // duplicated resolutions.
  media::VideoCaptureFormats::iterator last =
      std::unique(formats->begin(), formats->end(), IsCaptureFormatSizeEqual);
  formats->erase(last, formats->end());
  // Mark all formats as I420, since this is what the renderer side will get
  // anyhow: the actual pixel format is decided at the device level.
  for (media::VideoCaptureFormats::iterator it = formats->begin();
       it != formats->end(); ++it) {
    it->pixel_format = media::PIXEL_FORMAT_I420;
  }
}

// The maximum number of buffers in the capture pipeline. See
// VideoCaptureController ctor comments for more details.
const int kMaxNumberOfBuffers = 3;
const int kMaxNumberOfBuffersForTabCapture = 5;

// Used for logging capture events.
// Elements in this enum should not be deleted or rearranged; the only
// permitted operation is to add new elements before NUM_VIDEO_CAPTURE_EVENT.
enum VideoCaptureEvent {
  VIDEO_CAPTURE_START_CAPTURE = 0,
  VIDEO_CAPTURE_STOP_CAPTURE_OK = 1,
  VIDEO_CAPTURE_STOP_CAPTURE_DUE_TO_ERROR = 2,
  VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DEVICE = 3,
  VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DESKTOP_OR_TAB = 4,
  NUM_VIDEO_CAPTURE_EVENT
};

void LogVideoCaptureEvent(VideoCaptureEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Media.VideoCaptureManager.Event",
                            event,
                            NUM_VIDEO_CAPTURE_EVENT);
}

// Counter used for identifying a DeviceRequest to start a capture device.
static int g_device_start_id = 0;

}  // namespace

namespace content {

VideoCaptureManager::DeviceEntry::DeviceEntry(
    MediaStreamType stream_type,
    const std::string& id,
    scoped_ptr<VideoCaptureController> controller)
    : serial_id(g_device_start_id++),
      stream_type(stream_type),
      id(id),
      video_capture_controller_(controller.Pass()) {}

VideoCaptureManager::DeviceEntry::~DeviceEntry() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // DCHECK that this DeviceEntry does not still own a
  // media::VideoCaptureDevice. media::VideoCaptureDevice must be deleted on
  // the device thread.
  DCHECK(video_capture_device_ == nullptr);
}

void VideoCaptureManager::DeviceEntry::SetVideoCaptureDevice(
    scoped_ptr<media::VideoCaptureDevice> device) {
  DCHECK(thread_checker_.CalledOnValidThread());
  video_capture_device_.swap(device);
}

scoped_ptr<media::VideoCaptureDevice>
VideoCaptureManager::DeviceEntry::ReleaseVideoCaptureDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return video_capture_device_.Pass();
}

VideoCaptureController*
VideoCaptureManager::DeviceEntry::video_capture_controller() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return video_capture_controller_.get();
}

media::VideoCaptureDevice*
VideoCaptureManager::DeviceEntry::video_capture_device() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return video_capture_device_.get();
}

VideoCaptureManager::CaptureDeviceStartRequest::CaptureDeviceStartRequest(
    int serial_id,
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params)
    : serial_id_(serial_id),
      session_id_(session_id),
      params_(params),
      abort_start_(false) {
}

VideoCaptureManager::VideoCaptureManager(
    scoped_ptr<media::VideoCaptureDeviceFactory> factory)
    : listener_(NULL),
      new_capture_session_id_(1),
      video_capture_device_factory_(factory.Pass()) {
}

VideoCaptureManager::~VideoCaptureManager() {
  DCHECK(devices_.empty());
  DCHECK(device_start_queue_.empty());
}

void VideoCaptureManager::Register(
    MediaStreamProviderListener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!listener_);
  DCHECK(!device_task_runner_.get());
  listener_ = listener;
  device_task_runner_ = device_task_runner;
}

void VideoCaptureManager::Unregister() {
  DCHECK(listener_);
  listener_ = NULL;
}

void VideoCaptureManager::EnumerateDevices(MediaStreamType stream_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureManager::EnumerateDevices, type " << stream_type;
  DCHECK(listener_);
  DCHECK_EQ(stream_type, MEDIA_DEVICE_VIDEO_CAPTURE);

  // Bind a callback to ConsolidateDevicesInfoOnDeviceThread() with an argument
  // for another callback to OnDevicesInfoEnumerated() to be run in the current
  // loop, i.e. IO loop. Pass a timer for UMA histogram collection.
  base::Callback<void(scoped_ptr<media::VideoCaptureDevice::Names>)>
      devices_enumerated_callback =
          base::Bind(&VideoCaptureManager::ConsolidateDevicesInfoOnDeviceThread,
                     this,
                     media::BindToCurrentLoop(base::Bind(
                         &VideoCaptureManager::OnDevicesInfoEnumerated,
                         this,
                         stream_type,
                         base::Owned(new base::ElapsedTimer()))),
                     stream_type,
                     devices_info_cache_);
  // OK to use base::Unretained() since we own the VCDFactory and |this| is
  // bound in |devices_enumerated_callback|.
  device_task_runner_->PostTask(FROM_HERE,
      base::Bind(&media::VideoCaptureDeviceFactory::EnumerateDeviceNames,
                 base::Unretained(video_capture_device_factory_.get()),
                 devices_enumerated_callback));
}

int VideoCaptureManager::Open(const StreamDeviceInfo& device_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listener_);

  // Generate a new id for the session being opened.
  const media::VideoCaptureSessionId capture_session_id =
      new_capture_session_id_++;

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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listener_);
  DVLOG(1) << "VideoCaptureManager::Close, id " << capture_session_id;

  SessionMap::iterator session_it = sessions_.find(capture_session_id);
  if (session_it == sessions_.end()) {
    NOTREACHED();
    return;
  }

  DeviceEntry* const existing_device = GetDeviceEntryForMediaStreamDevice(
      session_it->second);
  if (existing_device) {
    // Remove any client that is still using the session. This is safe to call
    // even if there are no clients using the session.
    existing_device->video_capture_controller()
        ->StopSession(capture_session_id);

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

void VideoCaptureManager::QueueStartDevice(
    media::VideoCaptureSessionId session_id,
    DeviceEntry* entry,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  device_start_queue_.push_back(
      CaptureDeviceStartRequest(entry->serial_id, session_id, params));
  if (device_start_queue_.size() == 1)
    HandleQueuedStartRequest();
}

void VideoCaptureManager::DoStopDevice(DeviceEntry* entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(std::find(devices_.begin(), devices_.end(), entry) != devices_.end());

  // Find the matching start request.
  for (DeviceStartQueue::reverse_iterator request =
           device_start_queue_.rbegin();
       request != device_start_queue_.rend(); ++request) {
    if (request->serial_id() == entry->serial_id) {
      request->set_abort_start();
      DVLOG(3) << "DoStopDevice, aborting start request for device "
               << entry->id << " serial_id = " << entry->serial_id;
      return;
    }
  }

  DVLOG(3) << "DoStopDevice. Send stop request for device = " << entry->id
           << " serial_id = " << entry->serial_id << ".";
  if (entry->video_capture_device()) {
    // |entry->video_capture_device| can be null if creating the device fails.
    device_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureManager::DoStopDeviceOnDeviceThread, this,
                   base::Passed(entry->ReleaseVideoCaptureDevice())));
  }
}

void VideoCaptureManager::HandleQueuedStartRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Remove all start requests that have been aborted.
  while (device_start_queue_.begin() != device_start_queue_.end() &&
      device_start_queue_.begin()->abort_start()) {
    device_start_queue_.pop_front();
  }
  DeviceStartQueue::iterator request = device_start_queue_.begin();
  if (request == device_start_queue_.end())
    return;

  const int serial_id = request->serial_id();
  DeviceEntries::iterator entry_it = std::find_if(
      devices_.begin(), devices_.end(),
      [serial_id] (const DeviceEntry* e) {
        return e->serial_id == serial_id;
      });
  DCHECK(entry_it != devices_.end());
  DeviceEntry* entry =  (*entry_it);

  DVLOG(3) << "HandleQueuedStartRequest, Post start to device thread, device = "
           << entry->id << " start id = " << entry->serial_id;
  base::PostTaskAndReplyWithResult(
      device_task_runner_.get(),
      FROM_HERE,
      base::Bind(
          &VideoCaptureManager::DoStartDeviceOnDeviceThread,
          this,
          request->session_id(),
          entry->id,
          entry->stream_type,
          request->params(),
          base::Passed(entry->video_capture_controller()->NewDeviceClient(
              device_task_runner_, request->params().requested_format))),
      base::Bind(&VideoCaptureManager::OnDeviceStarted, this,
                 request->serial_id()));
}

void VideoCaptureManager::OnDeviceStarted(
    int serial_id,
    scoped_ptr<media::VideoCaptureDevice> device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(serial_id == device_start_queue_.begin()->serial_id());
  DVLOG(3) << "OnDeviceStarted";
  if (device_start_queue_.front().abort_start()) {
    // |device| can be null if creation failed in DoStartDeviceOnDeviceThread.
    // The device is no longer wanted. Stop the device again.
    DVLOG(3) << "OnDeviceStarted but start request have been aborted.";
    media::VideoCaptureDevice* device_ptr = device.get();
    base::Closure closure =
        base::Bind(&VideoCaptureManager::DoStopDeviceOnDeviceThread, this,
                   base::Passed(&device));
    if (device_ptr && !device_task_runner_->PostTask(FROM_HERE, closure)) {
      // PostTask failed. The device must be stopped anyway.
      device_ptr->StopAndDeAllocate();
    }
  } else {
    DeviceEntries::iterator entry_it = std::find_if(
        devices_.begin(), devices_.end(),
        [serial_id] (const DeviceEntry* e) {
          return e->serial_id == serial_id;
        });
    DCHECK(entry_it != devices_.end());
    DeviceEntry* entry = *entry_it;
    DCHECK(!entry->video_capture_device());
    entry->SetVideoCaptureDevice(device.Pass());

    if (entry->stream_type == MEDIA_DESKTOP_VIDEO_CAPTURE) {
      const media::VideoCaptureSessionId session_id =
          device_start_queue_.front().session_id();
      MaybePostDesktopCaptureWindowId(session_id);
    }
  }

  device_start_queue_.pop_front();
  HandleQueuedStartRequest();
}

scoped_ptr<media::VideoCaptureDevice>
VideoCaptureManager::DoStartDeviceOnDeviceThread(
    media::VideoCaptureSessionId session_id,
    const std::string& id,
    MediaStreamType stream_type,
    const media::VideoCaptureParams& params,
    scoped_ptr<media::VideoCaptureDevice::Client> device_client) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(IsOnDeviceThread());

  scoped_ptr<media::VideoCaptureDevice> video_capture_device;
  switch (stream_type) {
    case MEDIA_DEVICE_VIDEO_CAPTURE: {
      // We look up the device id from the renderer in our local enumeration
      // since the renderer does not have all the information that might be
      // held in the browser-side VideoCaptureDevice::Name structure.
      const media::VideoCaptureDeviceInfo* found =
          FindDeviceInfoById(id, devices_info_cache_);
      if (found) {
        video_capture_device =
            video_capture_device_factory_->Create(found->name);
      }
      break;
    }
    case MEDIA_TAB_VIDEO_CAPTURE: {
      video_capture_device.reset(
          WebContentsVideoCaptureDevice::Create(id));
      break;
    }
    case MEDIA_DESKTOP_VIDEO_CAPTURE: {
#if defined(ENABLE_SCREEN_CAPTURE)
      DesktopMediaID desktop_id = DesktopMediaID::Parse(id);
#if defined(USE_AURA)
      if (desktop_id.type == DesktopMediaID::TYPE_AURA_WINDOW) {
        video_capture_device.reset(
            DesktopCaptureDeviceAura::Create(desktop_id));
      } else
#endif
      if (desktop_id.type != DesktopMediaID::TYPE_NONE &&
          desktop_id.type != DesktopMediaID::TYPE_AURA_WINDOW) {
        video_capture_device = DesktopCaptureDevice::Create(desktop_id);
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
    device_client->OnError("Could not create capture device");
    return nullptr;
  }

  video_capture_device->AllocateAndStart(params, device_client.Pass());
  return video_capture_device.Pass();
}

void VideoCaptureManager::StartCaptureForClient(
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params,
    base::ProcessHandle client_render_process,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler,
    const DoneCB& done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureManager::StartCaptureForClient, "
           << params.requested_format.frame_size.ToString() << ", "
           << params.requested_format.frame_rate << ", #" << session_id << ")";

  DeviceEntry* entry = GetOrCreateDeviceEntry(session_id);
  if (!entry) {
    done_cb.Run(base::WeakPtr<VideoCaptureController>());
    return;
  }

  DCHECK(entry->video_capture_controller());

  LogVideoCaptureEvent(VIDEO_CAPTURE_START_CAPTURE);

  // First client starts the device.
  if (entry->video_capture_controller()->GetActiveClientCount() == 0) {
    DVLOG(1) << "VideoCaptureManager starting device (type = "
             << entry->stream_type << ", id = " << entry->id << ")";
    QueueStartDevice(session_id, entry, params);
  }
  // Run the callback first, as AddClient() may trigger OnFrameInfo().
  done_cb.Run(entry->video_capture_controller()->GetWeakPtrForIOThread());
  entry->video_capture_controller()->AddClient(
      client_id, client_handler, client_render_process, session_id, params);
}

void VideoCaptureManager::StopCaptureForClient(
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler,
    bool aborted_due_to_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(controller);
  DCHECK(client_handler);

  DeviceEntry* entry = GetDeviceEntryForController(controller);
  if (!entry) {
    NOTREACHED();
    return;
  }
  if (!aborted_due_to_error) {
    if (controller->has_received_frames()) {
      LogVideoCaptureEvent(VIDEO_CAPTURE_STOP_CAPTURE_OK);
    } else if (entry->stream_type == MEDIA_DEVICE_VIDEO_CAPTURE) {
      LogVideoCaptureEvent(
          VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DEVICE);
    } else {
      LogVideoCaptureEvent(
          VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DESKTOP_OR_TAB);
    }
  } else {
    LogVideoCaptureEvent(VIDEO_CAPTURE_STOP_CAPTURE_DUE_TO_ERROR);
    SessionMap::iterator it;
    for (it = sessions_.begin(); it != sessions_.end(); ++it) {
      if (it->second.type == entry->stream_type &&
          it->second.id == entry->id) {
        listener_->Aborted(it->second.type, it->first);
        break;
      }
    }
  }

  // Detach client from controller.
  media::VideoCaptureSessionId session_id =
      controller->RemoveClient(client_id, client_handler);
  DVLOG(1) << "VideoCaptureManager::StopCaptureForClient, session_id = "
           << session_id;

  // If controller has no more clients, delete controller and device.
  DestroyDeviceEntryIfNoClients(entry);
}

void VideoCaptureManager::PauseCaptureForClient(
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(controller);
  DCHECK(client_handler);
  DeviceEntry* entry = GetDeviceEntryForController(controller);
  if (!entry) {
    NOTREACHED();
    return;
  }

  // We only pause the MEDIA_DEVICE_VIDEO_CAPTURE entry to release camera to
  // system.
  if (entry->stream_type != MEDIA_DEVICE_VIDEO_CAPTURE)
    return;

  controller->PauseOrResumeClient(client_id, client_handler, true);
  if (controller->GetActiveClientCount() != 0)
    return;

  // There is no more client, release the camera.
  DoStopDevice(entry);
}

void VideoCaptureManager::ResumeCaptureForClient(
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params,
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(controller);
  DCHECK(client_handler);

  DeviceEntry* entry = GetDeviceEntryForController(controller);
  if (!entry) {
    NOTREACHED();
    return;
  }

  // We only pause/resume the MEDIA_DEVICE_VIDEO_CAPTURE entry.
  if (entry->stream_type != MEDIA_DEVICE_VIDEO_CAPTURE)
    return;

  controller->PauseOrResumeClient(client_id, client_handler, false);
  if (controller->GetActiveClientCount() != 1)
    return;

  // This is first active client, allocate the camera.
  QueueStartDevice(session_id, entry, params);
}

bool VideoCaptureManager::GetDeviceSupportedFormats(
    media::VideoCaptureSessionId capture_session_id,
    media::VideoCaptureFormats* supported_formats) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(supported_formats->empty());

  SessionMap::iterator it = sessions_.find(capture_session_id);
  if (it == sessions_.end())
    return false;
  DVLOG(1) << "GetDeviceSupportedFormats for device: " << it->second.name;

  // Return all available formats of the device, regardless its started state.
  media::VideoCaptureDeviceInfo* existing_device =
      FindDeviceInfoById(it->second.id, devices_info_cache_);
  if (existing_device)
    *supported_formats = existing_device->supported_formats;
  return true;
}

bool VideoCaptureManager::GetDeviceFormatsInUse(
    media::VideoCaptureSessionId capture_session_id,
    media::VideoCaptureFormats* formats_in_use) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(formats_in_use->empty());

  SessionMap::iterator it = sessions_.find(capture_session_id);
  if (it == sessions_.end())
    return false;
  DVLOG(1) << "GetDeviceFormatsInUse for device: " << it->second.name;

  // Return the currently in-use format(s) of the device, if it's started.
  DeviceEntry* device_in_use =
      GetDeviceEntryForMediaStreamDevice(it->second);
  if (device_in_use) {
    // Currently only one format-in-use is supported at the VCC level.
    formats_in_use->push_back(
        device_in_use->video_capture_controller()->GetVideoCaptureFormat());
  }
  return true;
}

void VideoCaptureManager::SetDesktopCaptureWindowId(
    media::VideoCaptureSessionId session_id,
    gfx::NativeViewId window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(2) << "SetDesktopCaptureWindowId called for session " << session_id;

  notification_window_ids_[session_id] = window_id;
  MaybePostDesktopCaptureWindowId(session_id);
}

void VideoCaptureManager::MaybePostDesktopCaptureWindowId(
    media::VideoCaptureSessionId session_id) {
  SessionMap::iterator session_it = sessions_.find(session_id);
  if (session_it == sessions_.end()) {
    return;
  }

  DeviceEntry* const existing_device =
      GetDeviceEntryForMediaStreamDevice(session_it->second);
  if (!existing_device) {
    DVLOG(2) << "Failed to find an existing screen capture device.";
    return;
  }

  if (!existing_device->video_capture_device()) {
    DVLOG(2) << "Screen capture device not yet started.";
    return;
  }

  DCHECK_EQ(MEDIA_DESKTOP_VIDEO_CAPTURE, existing_device->stream_type);
  DesktopMediaID id = DesktopMediaID::Parse(existing_device->id);
  if (id.type == DesktopMediaID::TYPE_NONE ||
      id.type == DesktopMediaID::TYPE_AURA_WINDOW) {
    VLOG(2) << "Video capture device type mismatch.";
    return;
  }

  auto window_id_it = notification_window_ids_.find(session_id);
  if (window_id_it == notification_window_ids_.end()) {
    DVLOG(2) << "Notification window id not set for screen capture.";
    return;
  }

  // Post |existing_device->video_capture_device| to the VideoCaptureDevice to
  // the device_task_runner_. This is safe since the device is destroyed on the
  // device_task_runner_.
  device_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureManager::SetDesktopCaptureWindowIdOnDeviceThread,
                 this,
                 existing_device->video_capture_device(),
                 window_id_it->second));

  notification_window_ids_.erase(window_id_it);
}

void VideoCaptureManager::DoStopDeviceOnDeviceThread(
    scoped_ptr<media::VideoCaptureDevice> device) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StopDeviceTime");
  DCHECK(IsOnDeviceThread());
  device->StopAndDeAllocate();
  DVLOG(3) << "DoStopDeviceOnDeviceThread";
}

void VideoCaptureManager::OnOpened(
    MediaStreamType stream_type,
    media::VideoCaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->Opened(stream_type, capture_session_id);
}

void VideoCaptureManager::OnClosed(
    MediaStreamType stream_type,
    media::VideoCaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->Closed(stream_type, capture_session_id);
}

void VideoCaptureManager::OnDevicesInfoEnumerated(
    MediaStreamType stream_type,
    base::ElapsedTimer* timer,
    const media::VideoCaptureDeviceInfos& new_devices_info_cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UMA_HISTOGRAM_TIMES(
      "Media.VideoCaptureManager.GetAvailableDevicesInfoOnDeviceThreadTime",
      timer->Elapsed());
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  devices_info_cache_ = new_devices_info_cache;

  MediaInternals::GetInstance()->UpdateVideoCaptureDeviceCapabilities(
      devices_info_cache_);

  // Walk the |devices_info_cache_| and transform from VCD::Name to
  // StreamDeviceInfo for return purposes.
  StreamDeviceInfoArray devices;
  for (const auto& it : devices_info_cache_) {
    devices.push_back(StreamDeviceInfo(
        stream_type, it.name.GetNameAndModel(), it.name.id()));
  }
  listener_->DevicesEnumerated(stream_type, devices);
}

bool VideoCaptureManager::IsOnDeviceThread() const {
  return device_task_runner_->BelongsToCurrentThread();
}

void VideoCaptureManager::ConsolidateDevicesInfoOnDeviceThread(
    base::Callback<void(const media::VideoCaptureDeviceInfos&)>
        on_devices_enumerated_callback,
    MediaStreamType stream_type,
    const media::VideoCaptureDeviceInfos& old_device_info_cache,
    scoped_ptr<media::VideoCaptureDevice::Names> names_snapshot) {
  DCHECK(IsOnDeviceThread());
  // Construct |new_devices_info_cache| with the cached devices that are still
  // present in the system, and remove their names from |names_snapshot|, so we
  // keep there the truly new devices.
  media::VideoCaptureDeviceInfos new_devices_info_cache;
  for (const auto& device_info : old_device_info_cache) {
     for (media::VideoCaptureDevice::Names::iterator it =
         names_snapshot->begin(); it != names_snapshot->end(); ++it) {
      if (device_info.name.id() == it->id()) {
        new_devices_info_cache.push_back(device_info);
        names_snapshot->erase(it);
        break;
      }
    }
  }

  // Get the supported capture formats for the new devices in |names_snapshot|.
  for (const auto& it : *names_snapshot) {
    media::VideoCaptureDeviceInfo device_info(it, media::VideoCaptureFormats());
    video_capture_device_factory_->GetDeviceSupportedFormats(
        it, &(device_info.supported_formats));
    ConsolidateCaptureFormats(&device_info.supported_formats);
    new_devices_info_cache.push_back(device_info);
  }

  on_devices_enumerated_callback.Run(new_devices_info_cache);
}

VideoCaptureManager::DeviceEntry*
VideoCaptureManager::GetDeviceEntryForMediaStreamDevice(
    const MediaStreamDevice& device_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

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
    const VideoCaptureController* controller) const {
  // Look up |controller| in |devices_|.
  for (DeviceEntries::const_iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if ((*it)->video_capture_controller() == controller) {
      return *it;
    }
  }
  return NULL;
}

void VideoCaptureManager::DestroyDeviceEntryIfNoClients(DeviceEntry* entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Removal of the last client stops the device.
  if (entry->video_capture_controller()->GetClientCount() == 0) {
    DVLOG(1) << "VideoCaptureManager stopping device (type = "
             << entry->stream_type << ", id = " << entry->id << ")";

    // The DeviceEntry is removed from |devices_| immediately. The controller is
    // deleted immediately, and the device is freed asynchronously. After this
    // point, subsequent requests to open this same device ID will create a new
    // DeviceEntry, VideoCaptureController, and VideoCaptureDevice.
    DoStopDevice(entry);
    DeviceEntries::iterator device_it = std::find(devices_.begin(),
                                                  devices_.end(),
                                                  entry);
    devices_.erase(device_it);
  }
}

VideoCaptureManager::DeviceEntry* VideoCaptureManager::GetOrCreateDeviceEntry(
    media::VideoCaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  SessionMap::iterator session_it = sessions_.find(capture_session_id);
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

  const int max_buffers = device_info.type == MEDIA_TAB_VIDEO_CAPTURE ?
      kMaxNumberOfBuffersForTabCapture : kMaxNumberOfBuffers;
  scoped_ptr<VideoCaptureController> video_capture_controller(
      new VideoCaptureController(max_buffers));
  DeviceEntry* new_device = new DeviceEntry(device_info.type,
                                            device_info.id,
                                            video_capture_controller.Pass());
  devices_.push_back(new_device);
  return new_device;
}

media::VideoCaptureDeviceInfo* VideoCaptureManager::FindDeviceInfoById(
    const std::string& id,
    media::VideoCaptureDeviceInfos& device_vector) {
  for (auto& it : device_vector) {
    if (it.name.id() == id)
      return &(it);
  }
  return NULL;
}

void VideoCaptureManager::SetDesktopCaptureWindowIdOnDeviceThread(
    media::VideoCaptureDevice* device,
    gfx::NativeViewId window_id) {
  DCHECK(IsOnDeviceThread());
#if defined(ENABLE_SCREEN_CAPTURE)
  DesktopCaptureDevice* desktop_device =
      static_cast<DesktopCaptureDevice*>(device);
  desktop_device->SetNotificationWindowId(window_id);
  VLOG(2) << "Screen capture notification window passed on device thread.";
#endif
}

}  // namespace content
