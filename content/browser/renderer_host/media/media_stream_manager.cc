// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_manager.h"

#include <list>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/threading/thread.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/device_request_message_filter.h"
#include "content/browser/renderer_host/media/media_stream_requester.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/renderer_host/media/web_contents_capture_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/media_stream_request.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace content {

// Creates a random label used to identify requests.
static std::string RandomLabel() {
  // An earlier PeerConnection spec,
  // http://dev.w3.org/2011/webrtc/editor/webrtc.html, specified the
  // MediaStream::label alphabet as containing 36 characters from
  // range: U+0021, U+0023 to U+0027, U+002A to U+002B, U+002D to U+002E,
  // U+0030 to U+0039, U+0041 to U+005A, U+005E to U+007E.
  // Here we use a safe subset.
  static const char kAlphabet[] = "0123456789"
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

  std::string label(36, ' ');
  for (size_t i = 0; i < label.size(); ++i) {
    int random_char = base::RandGenerator(sizeof(kAlphabet) - 1);
    label[i] = kAlphabet[random_char];
  }
  return label;
}

// Helper to verify if a media stream type is part of options or not.
static bool Requested(const MediaStreamRequest& request,
                      MediaStreamType stream_type) {
  return (request.audio_type == stream_type ||
          request.video_type == stream_type);
}

// TODO(xians): Merge DeviceRequest with MediaStreamRequest.
class MediaStreamManager::DeviceRequest {
 public:
  DeviceRequest(MediaStreamRequester* requester,
                const MediaStreamRequest& request)
      : requester(requester),
        request(request),
        state_(NUM_MEDIA_TYPES, MEDIA_REQUEST_STATE_NOT_REQUESTED) {
  }

  ~DeviceRequest() {}

  // Update the request state and notify observers.
  void SetState(MediaStreamType stream_type, MediaRequestState new_state) {
    if (stream_type == NUM_MEDIA_TYPES) {
      for (int i = MEDIA_NO_SERVICE + 1; i < NUM_MEDIA_TYPES; ++i) {
        const MediaStreamType stream_type = static_cast<MediaStreamType>(i);
        state_[stream_type] = new_state;
      }
    } else {
      state_[stream_type] = new_state;
    }

    if (request.video_type != MEDIA_TAB_VIDEO_CAPTURE &&
        request.audio_type != MEDIA_TAB_AUDIO_CAPTURE &&
        new_state != MEDIA_REQUEST_STATE_CLOSING) {
      return;
    }

    MediaObserver* media_observer =
        GetContentClient()->browser()->GetMediaObserver();
    if (media_observer == NULL)
      return;

    // If we appended a device_id scheme, we want to remove it when notifying
    // observers which may be in different modules since this scheme is only
    // used internally within the content module.
    std::string device_id =
        WebContentsCaptureUtil::StripWebContentsDeviceScheme(
            request.tab_capture_device_id);

    media_observer->OnMediaRequestStateChanged(
        request.render_process_id, request.render_view_id,
        request.page_request_id,
        MediaStreamDevice(stream_type, device_id, device_id), new_state);
  }

  MediaRequestState state(MediaStreamType stream_type) const {
    return state_[stream_type];
  }

  MediaStreamRequester* const requester;  // Can be NULL.
  MediaStreamRequest request;

  StreamDeviceInfoArray devices;

  // Callback to the requester which audio/video devices have been selected.
  // It can be null if the requester has no interest to know the result.
  // Currently it is only used by |DEVICE_ACCESS| type.
  MediaStreamManager::MediaRequestResponseCallback callback;

  scoped_ptr<MediaStreamUIProxy> ui_proxy;

 private:
  std::vector<MediaRequestState> state_;
};

MediaStreamManager::EnumerationCache::EnumerationCache()
    : valid(false) {
}

MediaStreamManager::EnumerationCache::~EnumerationCache() {
}

MediaStreamManager::MediaStreamManager()
    : audio_manager_(NULL),
      monitoring_started_(false),
      io_loop_(NULL),
      use_fake_ui_(false) {}

MediaStreamManager::MediaStreamManager(media::AudioManager* audio_manager)
    : audio_manager_(audio_manager),
      monitoring_started_(false),
      io_loop_(NULL),
      use_fake_ui_(false) {
  DCHECK(audio_manager_);
  memset(active_enumeration_ref_count_, 0,
         sizeof(active_enumeration_ref_count_));

  // Some unit tests create the MSM in the IO thread and assumes the
  // initialization is done synchronously.
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    InitializeDeviceManagersOnIOThread();
  } else {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&MediaStreamManager::InitializeDeviceManagersOnIOThread,
                   base::Unretained(this)));
  }
}

MediaStreamManager::~MediaStreamManager() {
  DCHECK(requests_.empty());
  DCHECK(!device_thread_.get());
}

VideoCaptureManager* MediaStreamManager::video_capture_manager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(video_capture_manager_.get());
  return video_capture_manager_.get();
}

AudioInputDeviceManager* MediaStreamManager::audio_input_device_manager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(audio_input_device_manager_.get());
  return audio_input_device_manager_.get();
}

std::string MediaStreamManager::MakeMediaAccessRequest(
    int render_process_id,
    int render_view_id,
    int page_request_id,
    const StreamOptions& options,
    const GURL& security_origin,
    const MediaRequestResponseCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Create a new request based on options.
  MediaStreamRequest stream_request(
      render_process_id, render_view_id, page_request_id, std::string(),
      security_origin, MEDIA_DEVICE_ACCESS, std::string(), std::string(),
      options.audio_type, options.video_type);
  DeviceRequest* request = new DeviceRequest(NULL, stream_request);
  const std::string& label = AddRequest(request);

  request->callback = callback;

  HandleRequest(label);

  return label;
}

std::string MediaStreamManager::GenerateStream(
    MediaStreamRequester* requester,
    int render_process_id,
    int render_view_id,
    int page_request_id,
    const StreamOptions& options,
    const GURL& security_origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    UseFakeDevice();
  }
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeUIForMediaStream)) {
    UseFakeUI(scoped_ptr<FakeMediaStreamUIProxy>());
  }

  int target_render_process_id = render_process_id;
  int target_render_view_id = render_view_id;
  std::string tab_capture_device_id;

  // Customize options for a WebContents based capture.
  if (options.audio_type == MEDIA_TAB_AUDIO_CAPTURE ||
      options.video_type == MEDIA_TAB_VIDEO_CAPTURE) {
    // TODO(justinlin): Can't plumb audio mirroring using stream type right
    // now, so plumbing by device_id. Will revisit once it's refactored.
    // http://crbug.com/163100
    tab_capture_device_id =
        WebContentsCaptureUtil::AppendWebContentsDeviceScheme(
            !options.video_device_id.empty() ?
            options.video_device_id : options.audio_device_id);

    bool has_valid_device_id = WebContentsCaptureUtil::ExtractTabCaptureTarget(
        tab_capture_device_id, &target_render_process_id,
        &target_render_view_id);
    if (!has_valid_device_id ||
        (options.audio_type != MEDIA_TAB_AUDIO_CAPTURE &&
         options.audio_type != MEDIA_NO_SERVICE) ||
        (options.video_type != MEDIA_TAB_VIDEO_CAPTURE &&
         options.video_type != MEDIA_NO_SERVICE)) {
      LOG(ERROR) << "Invalid request.";
      return std::string();
    }
  }

  std::string translated_audio_device_id;
  std::string translated_video_device_id;
  if (options.audio_type == MEDIA_DEVICE_AUDIO_CAPTURE) {
    bool found_match = TranslateGUIDToRawId(
        MEDIA_DEVICE_AUDIO_CAPTURE, security_origin, options.audio_device_id,
        &translated_audio_device_id);
    DCHECK(found_match || translated_audio_device_id.empty());
  }

  if (options.video_type == MEDIA_DEVICE_VIDEO_CAPTURE) {
    bool found_match = TranslateGUIDToRawId(
        MEDIA_DEVICE_VIDEO_CAPTURE, security_origin, options.video_device_id,
        &translated_video_device_id);
    DCHECK(found_match || translated_video_device_id.empty());
  }

  if (options.video_type == MEDIA_DESKTOP_VIDEO_CAPTURE ||
      options.audio_type == MEDIA_SYSTEM_AUDIO_CAPTURE) {
    // For screen capture we only support two valid combinations:
    // (1) screen video capture only, or
    // (2) screen video capture with system audio capture.
    if (options.video_type != MEDIA_DESKTOP_VIDEO_CAPTURE ||
        (options.audio_type != MEDIA_NO_SERVICE &&
         options.audio_type != MEDIA_SYSTEM_AUDIO_CAPTURE)) {
      // TODO(sergeyu): Surface error message to the calling JS code.
      LOG(ERROR) << "Invalid screen capture request.";
      return std::string();
    }
    translated_video_device_id = options.video_device_id;
  }

  // Create a new request based on options.
  MediaStreamRequest stream_request(
      target_render_process_id, target_render_view_id, page_request_id,
      tab_capture_device_id, security_origin, MEDIA_GENERATE_STREAM,
      translated_audio_device_id, translated_video_device_id,
      options.audio_type, options.video_type);
  DeviceRequest* request = new DeviceRequest(requester, stream_request);
  const std::string& label = AddRequest(request);
  HandleRequest(label);
  return label;
}

void MediaStreamManager::CancelRequest(const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DeviceRequests::iterator it = requests_.find(label);
  if (it != requests_.end()) {
    if (!RequestDone(*it->second)) {
      // TODO(xians): update the |state| to STATE_DONE to trigger a state
      // changed notification to UI before deleting the request?
      scoped_ptr<DeviceRequest> request(it->second);
      RemoveRequest(it);
      for (int i = MEDIA_NO_SERVICE + 1; i < NUM_MEDIA_TYPES; ++i) {
        const MediaStreamType stream_type = static_cast<MediaStreamType>(i);
        MediaStreamProvider* device_manager = GetDeviceManager(stream_type);
        if (!device_manager)
          continue;
        if (request->state(stream_type) != MEDIA_REQUEST_STATE_OPENING &&
            request->state(stream_type) != MEDIA_REQUEST_STATE_DONE) {
          continue;
        }
        for (StreamDeviceInfoArray::const_iterator device_it =
                 request->devices.begin();
             device_it != request->devices.end(); ++device_it) {
          if (device_it->device.type == stream_type) {
            device_manager->Close(device_it->session_id);
          }
        }
      }
      // Cancel the request if still pending at UI side.
      request->SetState(NUM_MEDIA_TYPES, MEDIA_REQUEST_STATE_CLOSING);
    } else {
      StopGeneratedStream(label);
    }
  }
}

void MediaStreamManager::StopGeneratedStream(const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Find the request and close all open devices for the request.
  DeviceRequests::iterator it = requests_.find(label);
  if (it != requests_.end()) {
    if (it->second->request.request_type == MEDIA_ENUMERATE_DEVICES) {
      StopEnumerateDevices(label);
      return;
    }

    scoped_ptr<DeviceRequest> request(it->second);
    RemoveRequest(it);
    for (StreamDeviceInfoArray::const_iterator device_it =
             request->devices.begin();
         device_it != request->devices.end(); ++device_it) {
      GetDeviceManager(device_it->device.type)->Close(device_it->session_id);
    }
    if (request->request.request_type == MEDIA_GENERATE_STREAM &&
        RequestDone(*request)) {
      // Notify observers that this device is being closed.
      for (int i = MEDIA_NO_SERVICE + 1; i != NUM_MEDIA_TYPES; ++i) {
        if (request->state(static_cast<MediaStreamType>(i)) !=
            MEDIA_REQUEST_STATE_NOT_REQUESTED) {
          request->SetState(static_cast<MediaStreamType>(i),
                            MEDIA_REQUEST_STATE_CLOSING);
        }
      }
    }
  }
}

std::string MediaStreamManager::EnumerateDevices(
    MediaStreamRequester* requester,
    int render_process_id,
    int render_view_id,
    int page_request_id,
    MediaStreamType type,
    const GURL& security_origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         type == MEDIA_DEVICE_VIDEO_CAPTURE);

  // When the requester is NULL, the request is made by the UI to ensure MSM
  // starts monitoring devices.
  if (!requester) {
    if (!monitoring_started_)
      StartMonitoring();

    return std::string();
  }

  // Create a new request.
  StreamOptions options;
  EnumerationCache* cache = NULL;
  if (type == MEDIA_DEVICE_AUDIO_CAPTURE) {
    options.audio_type = type;
    cache = &audio_enumeration_cache_;
  } else if (type == MEDIA_DEVICE_VIDEO_CAPTURE) {
    options.video_type = type;
    cache = &video_enumeration_cache_;
  } else {
    NOTREACHED();
    return std::string();
  }

  MediaStreamRequest stream_request(
      render_process_id, render_view_id, page_request_id, std::string(),
      security_origin, MEDIA_ENUMERATE_DEVICES, std::string(), std::string(),
      options.audio_type, options.video_type);
  DeviceRequest* request = new DeviceRequest(requester, stream_request);
  const std::string& label = AddRequest(request);

  if (cache->valid) {
    // Cached device list of this type exists. Just send it out.
    request->SetState(type, MEDIA_REQUEST_STATE_REQUESTED);

    // Need to post a task since the requester won't have label till
    // this function returns.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&MediaStreamManager::SendCachedDeviceList,
                   base::Unretained(this), cache, label));
  } else {
    StartEnumeration(request);
  }

  return label;
}

void MediaStreamManager::StopEnumerateDevices(const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DeviceRequests::iterator it = requests_.find(label);
  if (it != requests_.end()) {
    DCHECK_EQ(it->second->request.request_type, MEDIA_ENUMERATE_DEVICES);
    // Delete the DeviceRequest.
    scoped_ptr<DeviceRequest> request(it->second);
    RemoveRequest(it);
  }
}

std::string MediaStreamManager::OpenDevice(
    MediaStreamRequester* requester,
    int render_process_id,
    int render_view_id,
    int page_request_id,
    const std::string& device_id,
    MediaStreamType type,
    const GURL& security_origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         type == MEDIA_DEVICE_VIDEO_CAPTURE);

  // Create a new request.
  StreamOptions options;
  if (IsAudioMediaType(type)) {
    options.audio_type = type;
    options.audio_device_id = device_id;
  } else if (IsVideoMediaType(type)) {
    options.video_type = type;
    options.video_device_id = device_id;
  } else {
    NOTREACHED();
    return std::string();
  }

  MediaStreamRequest stream_request(
      render_process_id, render_view_id, page_request_id, std::string(),
      security_origin, MEDIA_OPEN_DEVICE, options.audio_device_id,
      options.video_device_id, options.audio_type, options.video_type);
  DeviceRequest* request = new DeviceRequest(requester, stream_request);
  const std::string& label = AddRequest(request);
  StartEnumeration(request);

  return label;
}

void MediaStreamManager::SendCachedDeviceList(
    EnumerationCache* cache,
    const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (cache->valid) {
    DeviceRequests::iterator it = requests_.find(label);
    if (it != requests_.end()) {
      it->second->requester->DevicesEnumerated(label, cache->devices);
    }
  }
}

void MediaStreamManager::StartMonitoring() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!base::SystemMonitor::Get())
    return;

  if (!monitoring_started_) {
    monitoring_started_ = true;
    base::SystemMonitor::Get()->AddDevicesChangedObserver(this);

    // Enumerate both the audio and video devices to cache the device lists
    // and send them to media observer.
    ++active_enumeration_ref_count_[MEDIA_DEVICE_AUDIO_CAPTURE];
    audio_input_device_manager_->EnumerateDevices(MEDIA_DEVICE_AUDIO_CAPTURE);
    ++active_enumeration_ref_count_[MEDIA_DEVICE_VIDEO_CAPTURE];
    video_capture_manager_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);
  }
}

void MediaStreamManager::StopMonitoring() {
  DCHECK_EQ(base::MessageLoop::current(), io_loop_);
  if (monitoring_started_) {
    base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
    monitoring_started_ = false;
    ClearEnumerationCache(&audio_enumeration_cache_);
    ClearEnumerationCache(&video_enumeration_cache_);
  }
}

bool MediaStreamManager::TranslateGUIDToRawId(MediaStreamType stream_type,
                                              const GURL& security_origin,
                                              const std::string& device_guid,
                                              std::string* raw_device_id) {
  DCHECK(stream_type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         stream_type == MEDIA_DEVICE_VIDEO_CAPTURE);
  if (device_guid.empty())
    return false;

  EnumerationCache* cache =
      stream_type == MEDIA_DEVICE_AUDIO_CAPTURE ?
      &audio_enumeration_cache_ : &video_enumeration_cache_;

  // If device monitoring hasn't started, the |device_guid| is not valid.
  if (!cache->valid)
    return false;

  for (StreamDeviceInfoArray::const_iterator it = cache->devices.begin();
       it != cache->devices.end();
       ++it) {
    if (DeviceRequestMessageFilter::DoesRawIdMatchGuid(
        security_origin, device_guid, it->device.id)) {
      *raw_device_id = it->device.id;
      return true;
    }
  }
  return false;
}

void MediaStreamManager::ClearEnumerationCache(EnumerationCache* cache) {
  DCHECK_EQ(base::MessageLoop::current(), io_loop_);
  cache->valid = false;
}

void MediaStreamManager::StartEnumeration(DeviceRequest* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Start monitoring the devices when doing the first enumeration.
  if (!monitoring_started_ && base::SystemMonitor::Get()) {
    StartMonitoring();
  }

  // Start enumeration for devices of all requested device types.
  for (int i = MEDIA_NO_SERVICE + 1; i < NUM_MEDIA_TYPES; ++i) {
    const MediaStreamType stream_type = static_cast<MediaStreamType>(i);
    if (Requested(request->request, stream_type)) {
      request->SetState(stream_type, MEDIA_REQUEST_STATE_REQUESTED);
      DCHECK_GE(active_enumeration_ref_count_[stream_type], 0);
      if (active_enumeration_ref_count_[stream_type] == 0) {
        ++active_enumeration_ref_count_[stream_type];
        GetDeviceManager(stream_type)->EnumerateDevices(stream_type);
      }
    }
  }
}

std::string MediaStreamManager::AddRequest(DeviceRequest* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Create a label for this request and verify it is unique.
  std::string unique_label;
  do {
    unique_label = RandomLabel();
  } while (requests_.find(unique_label) != requests_.end());

  requests_.insert(std::make_pair(unique_label, request));

  return unique_label;
}

void MediaStreamManager::RemoveRequest(DeviceRequests::iterator it) {
  requests_.erase(it);
}

void MediaStreamManager::PostRequestToUI(const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DeviceRequest* request = requests_[label];

  if (use_fake_ui_) {
    if (!fake_ui_)
      fake_ui_.reset(new FakeMediaStreamUIProxy());

    MediaStreamDevices devices;
    if (audio_enumeration_cache_.valid) {
      for (StreamDeviceInfoArray::const_iterator it =
               audio_enumeration_cache_.devices.begin();
           it != audio_enumeration_cache_.devices.end(); ++it) {
        devices.push_back(it->device);
      }
    }
    if (video_enumeration_cache_.valid) {
      for (StreamDeviceInfoArray::const_iterator it =
               video_enumeration_cache_.devices.begin();
           it != video_enumeration_cache_.devices.end(); ++it) {
        devices.push_back(it->device);
      }
    }

    fake_ui_->SetAvailableDevices(devices);

    request->ui_proxy = fake_ui_.Pass();
  } else {
    request->ui_proxy = MediaStreamUIProxy::Create();
  }

  request->ui_proxy->RequestAccess(
      request->request,
      base::Bind(&MediaStreamManager::HandleAccessRequestResponse,
                 base::Unretained(this), label));
}

void MediaStreamManager::HandleRequest(const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DeviceRequest* request = requests_[label];

  const MediaStreamType audio_type = request->request.audio_type;
  const MediaStreamType video_type = request->request.video_type;

  bool is_web_contents_capture =
      audio_type == MEDIA_TAB_AUDIO_CAPTURE ||
      video_type == MEDIA_TAB_VIDEO_CAPTURE;

  bool is_screen_capture =
      video_type == MEDIA_DESKTOP_VIDEO_CAPTURE;

  if (!is_web_contents_capture &&
      !is_screen_capture &&
      ((IsAudioMediaType(audio_type) && !audio_enumeration_cache_.valid) ||
       (IsVideoMediaType(video_type) && !video_enumeration_cache_.valid))) {
    // Enumerate the devices if there is no valid device lists to be used.
    StartEnumeration(request);
    return;
  }

  // No need to do new device enumerations, post the request to UI
  // immediately.
  if (IsAudioMediaType(audio_type))
    request->SetState(audio_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);
  if (IsVideoMediaType(video_type))
    request->SetState(video_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);

  PostRequestToUI(label);
}

void MediaStreamManager::InitializeDeviceManagersOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (device_thread_)
    return;

  device_thread_.reset(new base::Thread("MediaStreamDeviceThread"));
#if defined(OS_WIN)
  device_thread_->init_com_with_mta(true);
#endif
  CHECK(device_thread_->Start());

  audio_input_device_manager_ = new AudioInputDeviceManager(audio_manager_);
  audio_input_device_manager_->Register(
      this, device_thread_->message_loop_proxy().get());

  video_capture_manager_ = new VideoCaptureManager();
  video_capture_manager_->Register(this,
                                   device_thread_->message_loop_proxy().get());

  // We want to be notified of IO message loop destruction to delete the thread
  // and the device managers.
  io_loop_ = base::MessageLoop::current();
  io_loop_->AddDestructionObserver(this);
}

void MediaStreamManager::Opened(MediaStreamType stream_type,
                                int capture_session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Find the request containing this device and mark it as used.
  DeviceRequest* request = NULL;
  StreamDeviceInfoArray* devices = NULL;
  std::string label;
  for (DeviceRequests::iterator request_it = requests_.begin();
       request_it != requests_.end() && request == NULL; ++request_it) {
    devices = &(request_it->second->devices);
    for (StreamDeviceInfoArray::iterator device_it = devices->begin();
         device_it != devices->end(); ++device_it) {
      if (device_it->device.type == stream_type &&
          device_it->session_id == capture_session_id) {
        // We've found the request.
        device_it->in_use = true;
        label = request_it->first;
        request = request_it->second;
        break;
      }
    }
  }
  if (request == NULL) {
    // The request doesn't exist.
    return;
  }

  DCHECK_NE(request->state(stream_type), MEDIA_REQUEST_STATE_REQUESTED);

  // Check if all devices for this stream type are opened. Update the state if
  // they are.
  for (StreamDeviceInfoArray::iterator device_it = devices->begin();
       device_it != devices->end(); ++device_it) {
    if (device_it->device.type != stream_type) {
      continue;
    }
    if (device_it->in_use == false) {
      // Wait for more devices to be opened before we're done.
      return;
    }
  }

  request->SetState(stream_type, MEDIA_REQUEST_STATE_DONE);

  if (!RequestDone(*request)) {
    // This stream_type is done, but not the other type.
    return;
  }

  switch (request->request.request_type) {
    case MEDIA_OPEN_DEVICE:
      request->requester->DeviceOpened(label, devices->front());
      break;
    case MEDIA_GENERATE_STREAM: {
      // Partition the array of devices into audio vs video.
      StreamDeviceInfoArray audio_devices, video_devices;
      for (StreamDeviceInfoArray::iterator device_it = devices->begin();
           device_it != devices->end(); ++device_it) {
        if (IsAudioMediaType(device_it->device.type)) {
          // Store the native audio parameters in the device struct.
          // TODO(xians): Handle the tab capture sample rate/channel layout
          // in AudioInputDeviceManager::Open().
          if (device_it->device.type != content::MEDIA_TAB_AUDIO_CAPTURE) {
            const StreamDeviceInfo* info =
                audio_input_device_manager_->GetOpenedDeviceInfoById(
                    device_it->session_id);
            DCHECK_EQ(info->device.id, device_it->device.id);
            device_it->device.sample_rate = info->device.sample_rate;
            device_it->device.channel_layout = info->device.channel_layout;
          }
          audio_devices.push_back(*device_it);
        } else if (IsVideoMediaType(device_it->device.type)) {
          video_devices.push_back(*device_it);
        } else {
          NOTREACHED();
        }
      }

      request->requester->StreamGenerated(label, audio_devices, video_devices);
      request->ui_proxy->OnStarted(
          base::Bind(&MediaStreamManager::StopStreamFromUI,
                     base::Unretained(this), label));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void MediaStreamManager::Closed(MediaStreamType stream_type,
                                int capture_session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void MediaStreamManager::DevicesEnumerated(
    MediaStreamType stream_type, const StreamDeviceInfoArray& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Only cache the device list when the device list has been changed.
  bool need_update_clients = false;
  EnumerationCache* cache =
      stream_type == MEDIA_DEVICE_AUDIO_CAPTURE ?
      &audio_enumeration_cache_ : &video_enumeration_cache_;
  if (!cache->valid ||
      devices.size() != cache->devices.size() ||
      !std::equal(devices.begin(), devices.end(), cache->devices.begin(),
                  StreamDeviceInfo::IsEqual)) {
    cache->valid = true;
    cache->devices = devices;
    need_update_clients = true;
  }

  if (need_update_clients && monitoring_started_)
    NotifyDevicesChanged(stream_type, devices);

  // Publish the result for all requests waiting for device list(s).
  // Find the requests waiting for this device list, store their labels and
  // release the iterator before calling device settings. We might get a call
  // back from device_settings that will need to iterate through devices.
  std::list<std::string> label_list;
  for (DeviceRequests::iterator it = requests_.begin(); it != requests_.end();
       ++it) {
    if (it->second->state(stream_type) == MEDIA_REQUEST_STATE_REQUESTED &&
        Requested(it->second->request, stream_type)) {
      if (it->second->request.request_type != MEDIA_ENUMERATE_DEVICES)
        it->second->SetState(stream_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);
      label_list.push_back(it->first);
    }
  }
  for (std::list<std::string>::iterator it = label_list.begin();
       it != label_list.end(); ++it) {
    DeviceRequest* request = requests_[*it];
    switch (request->request.request_type) {
      case MEDIA_ENUMERATE_DEVICES:
        if (need_update_clients && request->requester)
          request->requester->DevicesEnumerated(*it, devices);
        break;
      default:
        if (request->state(request->request.audio_type) ==
                MEDIA_REQUEST_STATE_REQUESTED ||
            request->state(request->request.video_type) ==
                MEDIA_REQUEST_STATE_REQUESTED) {
          // We are doing enumeration for other type of media, wait until it is
          // all done before posting the request to UI because UI needs
          // the device lists to handle the request.
          break;
        }

        // Post the request to UI for permission approval.
        PostRequestToUI(*it);
        break;
    }
  }
  label_list.clear();
  --active_enumeration_ref_count_[stream_type];
  DCHECK_GE(active_enumeration_ref_count_[stream_type], 0);
}

void MediaStreamManager::Error(MediaStreamType stream_type,
                               int capture_session_id,
                               MediaStreamProviderError error) {
  // Find the device for the error call.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (DeviceRequests::iterator it = requests_.begin(); it != requests_.end();
       ++it) {
    StreamDeviceInfoArray& devices = it->second->devices;

    // TODO(miu): BUG.  It's possible for the audio (or video) device array in
    // the "requester" to become out-of-sync with the order of devices we have
    // here.  See http://crbug.com/147650
    int audio_device_idx = -1;
    int video_device_idx = -1;
    for (StreamDeviceInfoArray::iterator device_it = devices.begin();
         device_it != devices.end(); ++device_it) {
      if (IsAudioMediaType(device_it->device.type)) {
        ++audio_device_idx;
      } else if (IsVideoMediaType(device_it->device.type)) {
        ++video_device_idx;
      } else {
        NOTREACHED();
        continue;
      }
      if (device_it->device.type != stream_type ||
          device_it->session_id != capture_session_id) {
        continue;
      }
      // We've found the failing device. Find the error case:
      // An error should only be reported to the MediaStreamManager if
      // the request has not been fulfilled yet.
      DCHECK(it->second->state(stream_type) != MEDIA_REQUEST_STATE_DONE);
      if (it->second->state(stream_type) != MEDIA_REQUEST_STATE_DONE) {
        // Request is not done, devices are not opened in this case.
        if (devices.size() <= 1) {
          scoped_ptr<DeviceRequest> request(it->second);
          // 1. Device not opened and no other devices for this request ->
          //    signal stream error and remove the request.
          if (request->requester)
            request->requester->StreamGenerationFailed(it->first);

          RemoveRequest(it);
        } else {
          // 2. Not opened but other devices exists for this request -> remove
          //    device from list, but don't signal an error.
          devices.erase(device_it);  // NOTE: This invalidates device_it!
        }
      }
      return;
    }
  }
}

void MediaStreamManager::HandleAccessRequestResponse(
    const std::string& label,
    const MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DeviceRequests::iterator request_it = requests_.find(label);
  if (request_it == requests_.end()) {
    return;
  }

  // Handle the case when the request was denied.
  if (devices.empty()) {
    // Notify the users about the request result.
    scoped_ptr<DeviceRequest> request(request_it->second);
    if (request->requester)
      request->requester->StreamGenerationFailed(label);

    if (request->request.request_type == MEDIA_DEVICE_ACCESS &&
        !request->callback.is_null()) {
      request->callback.Run(MediaStreamDevices(), request->ui_proxy.Pass());
    }

    RemoveRequest(request_it);
    return;
  }

  if (request_it->second->request.request_type == MEDIA_DEVICE_ACCESS) {
    scoped_ptr<DeviceRequest> request(request_it->second);
    if (!request->callback.is_null())
      request->callback.Run(devices, request->ui_proxy.Pass());

    // Delete the request since it is done.
    RemoveRequest(request_it);
    return;
  }

  // Process all newly-accepted devices for this request.
  DeviceRequest* request = request_it->second;
  bool found_audio = false;
  bool found_video = false;
  for (MediaStreamDevices::const_iterator device_it = devices.begin();
       device_it != devices.end(); ++device_it) {
    StreamDeviceInfo device_info;
    device_info.device = *device_it;

    // TODO(justinlin): Nicer way to do this?
    // Re-append the device's id since we lost it when posting request to UI.
    if (device_info.device.type == content::MEDIA_TAB_VIDEO_CAPTURE ||
        device_info.device.type == content::MEDIA_TAB_AUDIO_CAPTURE) {
      device_info.device.id = request->request.tab_capture_device_id;

      // Initialize the sample_rate and channel_layout here since for audio
      // mirroring, we don't go through EnumerateDevices where these are usually
      // initialized.
      if (device_info.device.type == content::MEDIA_TAB_AUDIO_CAPTURE) {
        const media::AudioParameters parameters =
            audio_manager_->GetDefaultOutputStreamParameters();
        int sample_rate = parameters.sample_rate();
        // If we weren't able to get the native sampling rate or the sample_rate
        // is outside the valid range for input devices set reasonable defaults.
        if (sample_rate <= 0 || sample_rate > 96000)
          sample_rate = 44100;

        device_info.device.sample_rate = sample_rate;
        device_info.device.channel_layout = media::CHANNEL_LAYOUT_STEREO;
      }
    }

    // Set in_use to false to be able to track if this device has been
    // opened. in_use might be true if the device type can be used in more
    // than one session.
    device_info.in_use = false;

    device_info.session_id =
        GetDeviceManager(device_info.device.type)->Open(device_info);
    request->SetState(device_info.device.type, MEDIA_REQUEST_STATE_OPENING);
    request->devices.push_back(device_info);

    if (device_info.device.type == request->request.audio_type) {
      found_audio = true;
    } else if (device_info.device.type == request->request.video_type) {
      found_video = true;
    }
  }

  // Check whether we've received all stream types requested.
  if (!found_audio && IsAudioMediaType(request->request.audio_type))
    request->SetState(request->request.audio_type, MEDIA_REQUEST_STATE_ERROR);

  if (!found_video && IsVideoMediaType(request->request.video_type))
    request->SetState(request->request.video_type, MEDIA_REQUEST_STATE_ERROR);
}

void MediaStreamManager::StopStreamFromUI(const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DeviceRequests::iterator it = requests_.find(label);
  if (it == requests_.end())
    return;

  // Notify renderers that the stream has been stopped.
  if (it->second->requester)
    it->second->requester->StopGeneratedStream(label);

  StopGeneratedStream(label);
}

void MediaStreamManager::UseFakeDevice() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  video_capture_manager()->UseFakeDevice();
  audio_input_device_manager()->UseFakeDevice();
}

void MediaStreamManager::UseFakeUI(scoped_ptr<FakeMediaStreamUIProxy> fake_ui) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  use_fake_ui_ = true;
  fake_ui_ = fake_ui.Pass();
}

void MediaStreamManager::WillDestroyCurrentMessageLoop() {
  DCHECK_EQ(base::MessageLoop::current(), io_loop_);
  DCHECK(requests_.empty());
  if (device_thread_) {
    StopMonitoring();

    video_capture_manager_->Unregister();
    audio_input_device_manager_->Unregister();
    device_thread_.reset();
  }

  audio_input_device_manager_ = NULL;
  video_capture_manager_ = NULL;
}

void MediaStreamManager::NotifyDevicesChanged(
    MediaStreamType stream_type,
    const StreamDeviceInfoArray& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  MediaObserver* media_observer =
      GetContentClient()->browser()->GetMediaObserver();
  if (media_observer == NULL)
    return;

  // Map the devices to MediaStreamDevices.
  MediaStreamDevices new_devices;
  for (StreamDeviceInfoArray::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    new_devices.push_back(it->device);
  }

  if (IsAudioMediaType(stream_type)) {
    media_observer->OnAudioCaptureDevicesChanged(new_devices);
  } else if (IsVideoMediaType(stream_type)) {
    media_observer->OnVideoCaptureDevicesChanged(new_devices);
  } else {
    NOTREACHED();
  }
}

bool MediaStreamManager::RequestDone(const DeviceRequest& request) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  const bool requested_audio = IsAudioMediaType(request.request.audio_type);
  const bool requested_video = IsVideoMediaType(request.request.video_type);

  const bool audio_done =
      !requested_audio ||
      request.state(request.request.audio_type) ==
      MEDIA_REQUEST_STATE_DONE ||
      request.state(request.request.audio_type) ==
      MEDIA_REQUEST_STATE_ERROR;
  if (!audio_done)
    return false;

  const bool video_done =
      !requested_video ||
      request.state(request.request.video_type) ==
      MEDIA_REQUEST_STATE_DONE ||
      request.state(request.request.video_type) ==
      MEDIA_REQUEST_STATE_ERROR;
  if (!video_done)
    return false;

  for (StreamDeviceInfoArray::const_iterator it = request.devices.begin();
       it != request.devices.end(); ++it) {
    if (it->in_use == false)
      return false;
  }

  return true;
}

MediaStreamProvider* MediaStreamManager::GetDeviceManager(
    MediaStreamType stream_type) {
  if (IsVideoMediaType(stream_type)) {
    return video_capture_manager();
  } else if (IsAudioMediaType(stream_type)) {
    return audio_input_device_manager();
  }
  NOTREACHED();
  return NULL;
}

void MediaStreamManager::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // NOTE: This method is only called in response to physical audio/video device
  // changes (from the operating system).

  MediaStreamType stream_type;
  if (device_type == base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE) {
    stream_type = MEDIA_DEVICE_AUDIO_CAPTURE;
  } else if (device_type == base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE) {
    stream_type = MEDIA_DEVICE_VIDEO_CAPTURE;
  } else {
    return;  // Uninteresting device change.
  }

  // Always do enumeration even though some enumeration is in progress,
  // because those enumeration commands could be sent before these devices
  // change.
  ++active_enumeration_ref_count_[stream_type];
  GetDeviceManager(stream_type)->EnumerateDevices(stream_type);
}

}  // namespace content
