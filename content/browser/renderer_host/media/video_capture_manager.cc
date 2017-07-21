// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_manager.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/common/media_stream_request.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/base/video_facing.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_capture_device_factory.h"

#if defined(ENABLE_SCREEN_CAPTURE)

#if BUILDFLAG(ENABLE_WEBRTC) && !defined(OS_ANDROID)
#include "content/browser/media/capture/desktop_capture_device.h"
#endif

#if defined(USE_AURA)
#include "content/browser/media/capture/desktop_capture_device_aura.h"
#endif

#if defined(OS_ANDROID)
#include "content/browser/media/capture/screen_capture_device_android.h"
#endif

#endif  // defined(ENABLE_SCREEN_CAPTURE)

namespace {

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
  UMA_HISTOGRAM_ENUMERATION("Media.VideoCaptureManager.Event", event,
                            NUM_VIDEO_CAPTURE_EVENT);
}

const media::VideoCaptureSessionId kFakeSessionId = -1;

}  // namespace

namespace content {

// Class used for queuing request for starting a device.
class VideoCaptureManager::CaptureDeviceStartRequest {
 public:
  CaptureDeviceStartRequest(VideoCaptureController* controller,
                            media::VideoCaptureSessionId session_id,
                            const media::VideoCaptureParams& params);
  VideoCaptureController* controller() const { return controller_; }
  media::VideoCaptureSessionId session_id() const { return session_id_; }
  media::VideoCaptureParams params() const { return params_; }

 private:
  VideoCaptureController* const controller_;
  const media::VideoCaptureSessionId session_id_;
  const media::VideoCaptureParams params_;
};

VideoCaptureManager::CaptureDeviceStartRequest::CaptureDeviceStartRequest(
    VideoCaptureController* controller,
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params)
    : controller_(controller), session_id_(session_id), params_(params) {}

VideoCaptureManager::VideoCaptureManager(
    std::unique_ptr<VideoCaptureProvider> video_capture_provider)
    : new_capture_session_id_(1),
      video_capture_provider_(std::move(video_capture_provider)) {}

VideoCaptureManager::~VideoCaptureManager() {
  DCHECK(controllers_.empty());
  DCHECK(device_start_request_queue_.empty());
}

void VideoCaptureManager::AddVideoCaptureObserver(
    media::VideoCaptureObserver* observer) {
  DCHECK(observer);
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  capture_observers_.AddObserver(observer);
}

void VideoCaptureManager::RemoveAllVideoCaptureObservers() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  capture_observers_.Clear();
}

void VideoCaptureManager::RegisterListener(
    MediaStreamProviderListener* listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listener);
  listeners_.AddObserver(listener);
#if defined(OS_ANDROID)
  application_state_has_running_activities_ = true;
  app_status_listener_.reset(new base::android::ApplicationStatusListener(
      base::Bind(&VideoCaptureManager::OnApplicationStateChange,
                 base::Unretained(this))));
#endif
}

void VideoCaptureManager::UnregisterListener(
    MediaStreamProviderListener* listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  listeners_.RemoveObserver(listener);
}

void VideoCaptureManager::EnumerateDevices(
    const EnumerationCallback& client_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureManager::EnumerateDevices";

  // Pass a timer for UMA histogram collection.
  video_capture_provider_->GetDeviceInfosAsync(media::BindToCurrentLoop(
      base::Bind(&VideoCaptureManager::OnDeviceInfosReceived, this,
                 base::Owned(new base::ElapsedTimer()), client_callback)));
}

int VideoCaptureManager::Open(const MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Generate a new id for the session being opened.
  const media::VideoCaptureSessionId capture_session_id =
      new_capture_session_id_++;

  DCHECK(sessions_.find(capture_session_id) == sessions_.end());
  DVLOG(1) << "VideoCaptureManager::Open, id " << capture_session_id;

  // We just save the stream info for processing later.
  sessions_[capture_session_id] = device;

  // Notify our listener asynchronously; this ensures that we return
  // |capture_session_id| to the caller of this function before using that same
  // id in a listener event.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&VideoCaptureManager::OnOpened, this, device.type,
                            capture_session_id));
  return capture_session_id;
}

void VideoCaptureManager::Close(int capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureManager::Close, id " << capture_session_id;

  SessionMap::iterator session_it = sessions_.find(capture_session_id);
  if (session_it == sessions_.end()) {
    NOTREACHED();
    return;
  }

  VideoCaptureController* const existing_device =
      LookupControllerByMediaTypeAndDeviceId(session_it->second.type,
                                             session_it->second.id);
  if (existing_device) {
    // Remove any client that is still using the session. This is safe to call
    // even if there are no clients using the session.
    existing_device->StopSession(capture_session_id);

    // StopSession() may have removed the last client, so we might need to
    // close the device.
    DestroyControllerIfNoClients(existing_device);
  }

  // Notify listeners asynchronously, and forget the session.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&VideoCaptureManager::OnClosed, this,
                            session_it->second.type, capture_session_id));
  sessions_.erase(session_it);
}

void VideoCaptureManager::QueueStartDevice(
    media::VideoCaptureSessionId session_id,
    VideoCaptureController* controller,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  device_start_request_queue_.push_back(
      CaptureDeviceStartRequest(controller, session_id, params));
  if (device_start_request_queue_.size() == 1)
    ProcessDeviceStartRequestQueue();
}

void VideoCaptureManager::DoStopDevice(VideoCaptureController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mcasas): use a helper function https://crbug.com/624854.
  DCHECK(std::find_if(
             controllers_.begin(), controllers_.end(),
             [controller](
                 const scoped_refptr<VideoCaptureController>& device_entry) {
               return device_entry.get() == controller;
             }) != controllers_.end());

  // If start request has not yet started processing, i.e. if it is not at the
  // beginning of the queue, remove it from the queue.
  auto request_iter = device_start_request_queue_.begin();
  if (request_iter != device_start_request_queue_.end()) {
    request_iter =
        std::find_if(++request_iter, device_start_request_queue_.end(),
                     [controller](const CaptureDeviceStartRequest& request) {
                       return request.controller() == controller;
                     });
    if (request_iter != device_start_request_queue_.end()) {
      device_start_request_queue_.erase(request_iter);
      return;
    }
  }

  const media::VideoCaptureDeviceInfo* device_info =
      GetDeviceInfoById(controller->device_id());
  if (device_info != nullptr) {
    for (auto& observer : capture_observers_)
      observer.OnVideoCaptureStopped(device_info->descriptor.facing);
  }

  DVLOG(3) << "DoStopDevice. Send stop request for device = "
           << controller->device_id()
           << " serial_id = " << controller->serial_id() << ".";
  controller->OnLog(base::StringPrintf("Stopping device: id: %s",
                                       controller->device_id().c_str()));

  // Since we may be removing |controller| from |controllers_| while
  // ReleaseDeviceAsnyc() is executing, we pass it shared ownership to
  // |controller|.
  controller->ReleaseDeviceAsync(
      base::Bind([](scoped_refptr<VideoCaptureController>) {},
                 GetControllerSharedRef(controller)));
}

void VideoCaptureManager::ProcessDeviceStartRequestQueue() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeviceStartQueue::iterator request = device_start_request_queue_.begin();
  if (request == device_start_request_queue_.end())
    return;

  VideoCaptureController* const controller = request->controller();

  DVLOG(3) << "HandleQueuedStartRequest, Post start to device thread, device = "
           << controller->device_id()
           << " start id = " << controller->serial_id();
  // The unit test VideoCaptureManagerTest.OpenNotExisting requires us to fail
  // synchronously if the stream_type is MEDIA_DEVICE_VIDEO_CAPTURE and no
  // DeviceInfo matching the requested id is present (which is the case when
  // requesting a device with a bogus id). Note, that since other types of
  // failure during startup of the device are allowed to be reported
  // asynchronously, this requirement is questionable.
  // TODO(chfremer): Check if any production code actually depends on this
  // requirement. If not, relax the requirement in the test and remove the below
  // if block. See crbug.com/708251
  if (controller->stream_type() == MEDIA_DEVICE_VIDEO_CAPTURE) {
    const media::VideoCaptureDeviceInfo* device_info =
        GetDeviceInfoById(controller->device_id());
    if (!device_info) {
      OnDeviceLaunchFailed(controller);
      return;
    }
    for (auto& observer : capture_observers_)
      observer.OnVideoCaptureStarted(device_info->descriptor.facing);
  }

  // The method CreateAndStartDeviceAsync() is going to run asynchronously.
  // Since we may be removing the controller while it is executing, we need to
  // pass it shared ownership to itself so that it stays alive while executing.
  // And since the execution may make callbacks into |this|, we also need
  // to pass it shared ownership to |this|.
  // TODO(chfremer): Check if request->params() can actually be different from
  // controller->parameters, and simplify if this is not the case.
  controller->CreateAndStartDeviceAsync(
      request->params(), static_cast<VideoCaptureDeviceLaunchObserver*>(this),
      base::Bind([](scoped_refptr<VideoCaptureManager>,
                    scoped_refptr<VideoCaptureController>) {},
                 scoped_refptr<VideoCaptureManager>(this),
                 GetControllerSharedRef(controller)));
}

void VideoCaptureManager::OnDeviceLaunched(VideoCaptureController* controller) {
  DVLOG(3) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!device_start_request_queue_.empty());
  DCHECK_EQ(controller, device_start_request_queue_.begin()->controller());
  DCHECK(controller);

  if (controller->stream_type() == MEDIA_DESKTOP_VIDEO_CAPTURE) {
    const media::VideoCaptureSessionId session_id =
        device_start_request_queue_.front().session_id();
    DCHECK(session_id != kFakeSessionId);
    MaybePostDesktopCaptureWindowId(session_id);
  }

  auto it = photo_request_queue_.begin();
  while (it != photo_request_queue_.end()) {
    auto request = it++;
    VideoCaptureController* maybe_entry =
        LookupControllerBySessionId(request->first);
    if (maybe_entry && maybe_entry->IsDeviceAlive()) {
      request->second.Run();
      photo_request_queue_.erase(request);
    }
  }

  device_start_request_queue_.pop_front();
  ProcessDeviceStartRequestQueue();
}

void VideoCaptureManager::OnDeviceLaunchFailed(
    VideoCaptureController* controller) {
  const std::string log_message = base::StringPrintf(
      "Starting device %s has failed. Maybe recently disconnected?",
      controller->device_id().c_str());
  DLOG(ERROR) << log_message;
  controller->OnLog(log_message);
  controller->OnError();

  device_start_request_queue_.pop_front();
  ProcessDeviceStartRequestQueue();
}

void VideoCaptureManager::OnDeviceLaunchAborted() {
  device_start_request_queue_.pop_front();
  ProcessDeviceStartRequestQueue();
}

void VideoCaptureManager::OnDeviceConnectionLost(
    VideoCaptureController* controller) {
  const std::string log_message = base::StringPrintf(
      "Lost connection to device %d.", controller->serial_id());
  DLOG(WARNING) << log_message;
  controller->OnLog(log_message);
  controller->OnError();
}

void VideoCaptureManager::ConnectClient(
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler,
    const DoneCB& done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << __func__ << ", session_id = " << session_id << ", request: "
           << media::VideoCaptureFormat::ToString(params.requested_format);

  VideoCaptureController* controller =
      GetOrCreateController(session_id, params);
  if (!controller) {
    done_cb.Run(base::WeakPtr<VideoCaptureController>());
    return;
  }

  LogVideoCaptureEvent(VIDEO_CAPTURE_START_CAPTURE);

  // First client starts the device.
  if (!controller->HasActiveClient() && !controller->HasPausedClient()) {
    DVLOG(1) << "VideoCaptureManager starting device (id = "
             << controller->device_id() << ")";
    QueueStartDevice(session_id, controller, params);
  }
  // Run the callback first, as AddClient() may trigger OnFrameInfo().
  done_cb.Run(controller->GetWeakPtrForIOThread());
  controller->AddClient(client_id, client_handler, session_id, params);
}

void VideoCaptureManager::DisconnectClient(
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler,
    bool aborted_due_to_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(controller);
  DCHECK(client_handler);

  if (!IsControllerPointerValid(controller)) {
    NOTREACHED();
    return;
  }
  if (!aborted_due_to_error) {
    if (controller->has_received_frames()) {
      LogVideoCaptureEvent(VIDEO_CAPTURE_STOP_CAPTURE_OK);
    } else if (controller->stream_type() == MEDIA_DEVICE_VIDEO_CAPTURE) {
      LogVideoCaptureEvent(
          VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DEVICE);
    } else {
      LogVideoCaptureEvent(
          VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DESKTOP_OR_TAB);
    }
  } else {
    LogVideoCaptureEvent(VIDEO_CAPTURE_STOP_CAPTURE_DUE_TO_ERROR);
    for (auto it : sessions_) {
      if (it.second.type == controller->stream_type() &&
          it.second.id == controller->device_id()) {
        for (auto& listener : listeners_)
          listener.Aborted(it.second.type, it.first);
        // Aborted() call might synchronously destroy |controller|, recheck.
        if (!IsControllerPointerValid(controller))
          return;
        break;
      }
    }
  }

  // Detach client from controller.
  const media::VideoCaptureSessionId session_id =
      controller->RemoveClient(client_id, client_handler);
  DVLOG(1) << __func__ << ", session_id = " << session_id;

  // If controller has no more clients, delete controller and device.
  DestroyControllerIfNoClients(controller);
}

void VideoCaptureManager::PauseCaptureForClient(
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(controller);
  DCHECK(client_handler);
  if (!IsControllerPointerValid(controller))
    NOTREACHED() << "Got Null controller while pausing capture";

  const bool had_active_client = controller->HasActiveClient();
  controller->PauseClient(client_id, client_handler);
  if (!had_active_client || controller->HasActiveClient())
    return;
  if (!controller->IsDeviceAlive())
    return;
  controller->MaybeSuspend();
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

  if (!IsControllerPointerValid(controller))
    NOTREACHED() << "Got Null controller while resuming capture";

  const bool had_active_client = controller->HasActiveClient();
  controller->ResumeClient(client_id, client_handler);
  if (had_active_client || !controller->HasActiveClient())
    return;
  if (!controller->IsDeviceAlive())
    return;
  controller->Resume();
}

void VideoCaptureManager::RequestRefreshFrameForClient(
    VideoCaptureController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (IsControllerPointerValid(controller)) {
    if (!controller->IsDeviceAlive())
      return;
    controller->RequestRefreshFrame();
  }
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

  return GetDeviceSupportedFormats(it->second.id, supported_formats);
}

bool VideoCaptureManager::GetDeviceSupportedFormats(
    const std::string& device_id,
    media::VideoCaptureFormats* supported_formats) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(supported_formats->empty());

  // Return all available formats of the device, regardless its started state.
  media::VideoCaptureDeviceInfo* existing_device = GetDeviceInfoById(device_id);
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

  base::Optional<media::VideoCaptureFormat> format =
      GetDeviceFormatInUse(it->second.type, it->second.id);
  if (format.has_value())
    formats_in_use->push_back(format.value());

  return true;
}

base::Optional<media::VideoCaptureFormat>
VideoCaptureManager::GetDeviceFormatInUse(MediaStreamType stream_type,
                                          const std::string& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Return the currently in-use format of the device, if it's started.
  VideoCaptureController* device_in_use =
      LookupControllerByMediaTypeAndDeviceId(stream_type, device_id);
  return device_in_use ? device_in_use->GetVideoCaptureFormat() : base::nullopt;
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
  if (session_it == sessions_.end())
    return;

  VideoCaptureController* const existing_device =
      LookupControllerByMediaTypeAndDeviceId(session_it->second.type,
                                             session_it->second.id);
  if (!existing_device) {
    DVLOG(2) << "Failed to find an existing screen capture device.";
    return;
  }

  if (!existing_device->IsDeviceAlive()) {
    DVLOG(2) << "Screen capture device not yet started.";
    return;
  }

  DCHECK_EQ(MEDIA_DESKTOP_VIDEO_CAPTURE, existing_device->stream_type());
  DesktopMediaID id = DesktopMediaID::Parse(existing_device->device_id());
  if (id.is_null())
    return;

  auto window_id_it = notification_window_ids_.find(session_id);
  if (window_id_it == notification_window_ids_.end()) {
    DVLOG(2) << "Notification window id not set for screen capture.";
    return;
  }

  existing_device->SetDesktopCaptureWindowIdAsync(
      window_id_it->second,
      base::Bind([](scoped_refptr<VideoCaptureManager>) {},
                 scoped_refptr<VideoCaptureManager>(this)));
  notification_window_ids_.erase(window_id_it);
}

void VideoCaptureManager::GetPhotoState(
    int session_id,
    media::VideoCaptureDevice::GetPhotoStateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const VideoCaptureController* controller =
      LookupControllerBySessionId(session_id);
  if (!controller)
    return;
  if (controller->IsDeviceAlive()) {
    controller->GetPhotoState(std::move(callback));
    return;
  }
  // Queue up a request for later.
  photo_request_queue_.emplace_back(
      session_id,
      base::Bind(&VideoCaptureController::GetPhotoState,
                 base::Unretained(controller), base::Passed(&callback)));
}

void VideoCaptureManager::SetPhotoOptions(
    int session_id,
    media::mojom::PhotoSettingsPtr settings,
    media::VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureController* controller = LookupControllerBySessionId(session_id);
  if (!controller)
    return;
  if (controller->IsDeviceAlive()) {
    controller->SetPhotoOptions(std::move(settings), std::move(callback));
    return;
  }
  // Queue up a request for later.
  photo_request_queue_.emplace_back(
      session_id, base::Bind(&VideoCaptureController::SetPhotoOptions,
                             base::Unretained(controller),
                             base::Passed(&settings), base::Passed(&callback)));
}

void VideoCaptureManager::TakePhoto(
    int session_id,
    media::VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureController* controller = LookupControllerBySessionId(session_id);
  if (!controller)
    return;
  if (controller->IsDeviceAlive()) {
    controller->TakePhoto(std::move(callback));
    return;
  }
  // Queue up a request for later.
  photo_request_queue_.emplace_back(
      session_id,
      base::Bind(&VideoCaptureController::TakePhoto,
                 base::Unretained(controller), base::Passed(&callback)));
}

void VideoCaptureManager::OnOpened(
    MediaStreamType stream_type,
    media::VideoCaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (auto& listener : listeners_)
    listener.Opened(stream_type, capture_session_id);
}

void VideoCaptureManager::OnClosed(
    MediaStreamType stream_type,
    media::VideoCaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (auto& listener : listeners_)
    listener.Closed(stream_type, capture_session_id);
}

void VideoCaptureManager::OnDeviceInfosReceived(
    base::ElapsedTimer* timer,
    const EnumerationCallback& client_callback,
    const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UMA_HISTOGRAM_TIMES(
      "Media.VideoCaptureManager.GetAvailableDevicesInfoOnDeviceThreadTime",
      timer->Elapsed());
  devices_info_cache_ = device_infos;

  // Walk the |devices_info_cache_| and produce a
  // media::VideoCaptureDeviceDescriptors for |client_callback|.
  media::VideoCaptureDeviceDescriptors devices;
  std::vector<std::tuple<media::VideoCaptureDeviceDescriptor,
                         media::VideoCaptureFormats>>
      descriptors_and_formats;
  for (const auto& it : devices_info_cache_) {
    devices.emplace_back(it.descriptor);
    descriptors_and_formats.emplace_back(it.descriptor, it.supported_formats);
    MediaInternals::GetInstance()->UpdateVideoCaptureDeviceCapabilities(
        descriptors_and_formats);
  }

  client_callback.Run(devices);
}

void VideoCaptureManager::DestroyControllerIfNoClients(
    VideoCaptureController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Removal of the last client stops the device.
  if (!controller->HasActiveClient() && !controller->HasPausedClient()) {
    DVLOG(1) << "VideoCaptureManager stopping device (type = "
             << controller->stream_type()
             << ", id = " << controller->device_id() << ")";

    // The VideoCaptureController is removed from |controllers_| immediately.
    // The controller is deleted immediately, and the device is freed
    // asynchronously. After this point, subsequent requests to open this same
    // device ID will create a new VideoCaptureController,
    // VideoCaptureController, and VideoCaptureDevice.
    DoStopDevice(controller);
    // TODO(mcasas): use a helper function https://crbug.com/624854.
    auto controller_iter = std::find_if(
        controllers_.begin(), controllers_.end(),
        [controller](
            const scoped_refptr<VideoCaptureController>& device_entry) {
          return device_entry.get() == controller;
        });
    controllers_.erase(controller_iter);
  }
}

VideoCaptureController* VideoCaptureManager::LookupControllerBySessionId(
    int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SessionMap::const_iterator session_it = sessions_.find(session_id);
  if (session_it == sessions_.end())
    return nullptr;

  return LookupControllerByMediaTypeAndDeviceId(session_it->second.type,
                                                session_it->second.id);
}

VideoCaptureController*
VideoCaptureManager::LookupControllerByMediaTypeAndDeviceId(
    MediaStreamType type,
    const std::string& device_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const auto& entry : controllers_) {
    if (type == entry->stream_type() && device_id == entry->device_id())
      return entry.get();
  }
  return nullptr;
}

bool VideoCaptureManager::IsControllerPointerValid(
    const VideoCaptureController* controller) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return base::ContainsValue(controllers_, controller);
}

scoped_refptr<VideoCaptureController>
VideoCaptureManager::GetControllerSharedRef(
    VideoCaptureController* controller) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const auto& entry : controllers_) {
    if (entry.get() == controller)
      return entry;
  }
  return nullptr;
}

media::VideoCaptureDeviceInfo* VideoCaptureManager::GetDeviceInfoById(
    const std::string& id) {
  for (auto& it : devices_info_cache_) {
    if (it.descriptor.device_id == id)
      return &it;
  }
  return nullptr;
}

VideoCaptureController* VideoCaptureManager::GetOrCreateController(
    media::VideoCaptureSessionId capture_session_id,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  SessionMap::iterator session_it = sessions_.find(capture_session_id);
  if (session_it == sessions_.end())
    return nullptr;
  const MediaStreamDevice& device_info = session_it->second;

  // Check if another session has already opened this device. If so, just
  // use that opened device.
  VideoCaptureController* const existing_device =
      LookupControllerByMediaTypeAndDeviceId(device_info.type, device_info.id);
  if (existing_device) {
    DCHECK_EQ(device_info.type, existing_device->stream_type());
    return existing_device;
  }

  VideoCaptureController* new_controller = new VideoCaptureController(
      device_info.id, device_info.type, params,
      video_capture_provider_->CreateDeviceLauncher());
  controllers_.emplace_back(new_controller);
  return new_controller;
}

base::Optional<CameraCalibration> VideoCaptureManager::GetCameraCalibration(
    const std::string& device_id) {
  media::VideoCaptureDeviceInfo* info = GetDeviceInfoById(device_id);
  if (!info)
    return base::Optional<CameraCalibration>();
  return info->descriptor.camera_calibration;
}

#if defined(OS_ANDROID)
void VideoCaptureManager::OnApplicationStateChange(
    base::android::ApplicationState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Only release/resume devices when the Application state changes from
  // RUNNING->STOPPED->RUNNING.
  if (state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES &&
      !application_state_has_running_activities_) {
    ResumeDevices();
    application_state_has_running_activities_ = true;
  } else if (state == base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES) {
    ReleaseDevices();
    application_state_has_running_activities_ = false;
  }
}

void VideoCaptureManager::ReleaseDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (auto& controller : controllers_) {
    // Do not stop Content Video Capture devices, e.g. Tab or Screen capture.
    if (controller->stream_type() != MEDIA_DEVICE_VIDEO_CAPTURE)
      continue;

    DoStopDevice(controller.get());
  }
}

void VideoCaptureManager::ResumeDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (auto& controller : controllers_) {
    // Do not resume Content Video Capture devices, e.g. Tab or Screen capture.
    // Do not try to restart already running devices.
    if (controller->stream_type() != MEDIA_DEVICE_VIDEO_CAPTURE ||
        controller->IsDeviceAlive())
      continue;

    // Check if the device is already in the start queue.
    bool device_in_queue = false;
    for (auto& request : device_start_request_queue_) {
      if (request.controller() == controller.get()) {
        device_in_queue = true;
        break;
      }
    }

    if (!device_in_queue) {
      // Session ID is only valid for Screen capture. So we can fake it to
      // resume video capture devices here.
      QueueStartDevice(kFakeSessionId, controller.get(),
                       controller->parameters());
    }
  }
}
#endif  // defined(OS_ANDROID)

}  // namespace content
