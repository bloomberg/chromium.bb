// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_manager.h"

#include <list>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/media/media_stream_device_settings.h"
#include "content/browser/renderer_host/media/media_stream_requester.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/media_stream_options.h"

namespace media_stream {

// Creates a random label used to identify requests.
static std::string RandomLabel() {
  // Alphabet according to WhatWG standard, i.e. containing 36 characters from
  // range: U+0021, U+0023 to U+0027, U+002A to U+002B, U+002D to U+002E,
  // U+0030 to U+0039, U+0041 to U+005A, U+005E to U+007E.
  static const char alphabet[] = "!#$%&\'*+-.0123456789"
      "abcdefghijklmnopqrstuvwxyz^_`ABCDEFGHIJKLMNOPQRSTUVWXYZ{|}~";

  std::string label(36, ' ');
  for (size_t i = 0; i < label.size(); ++i) {
    int random_char = base::RandGenerator(sizeof(alphabet) - 1);
    label[i] = alphabet[random_char];
  }
  return label;
}

// Helper to verify if a media stream type is part of options or not.
static bool Requested(const StreamOptions& options,
                      MediaStreamType stream_type) {
  if (stream_type == kVideoCapture &&
      (options.video_option != StreamOptions::kNoCamera)) {
    return true;
  } else if (stream_type == kAudioCapture && options.audio == true) {
    return true;
  }
  return false;
}

MediaStreamManager::MediaStreamManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          device_settings_(new MediaStreamDeviceSettings(this))),
      enumeration_in_progress_(kNumMediaStreamTypes, false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

MediaStreamManager::~MediaStreamManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

VideoCaptureManager* MediaStreamManager::video_capture_manager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!video_capture_manager_.get()) {
    video_capture_manager_.reset(new VideoCaptureManager());
    video_capture_manager_->Register(this);
  }
  return video_capture_manager_.get();
}

AudioInputDeviceManager* MediaStreamManager::audio_input_device_manager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // TODO(mflodman): Add when audio input manager is available.
  return NULL;
}

void MediaStreamManager::GenerateStream(MediaStreamRequester* requester,
                                        int render_process_id,
                                        int render_view_id,
                                        const StreamOptions& options,
                                        const std::string& security_origin,
                                        std::string* label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // TODO(mflodman): Remove next line when audio is supported.
  (const_cast<StreamOptions&>(options)).audio = false;

  // Create a new request based on options.
  DeviceRequest new_request = DeviceRequest(requester, options);
  if (Requested(new_request.options, kAudioCapture)) {
    new_request.state[kAudioCapture] = DeviceRequest::kRequested;
  }
  if (Requested(new_request.options, kVideoCapture)) {
    new_request.state[kVideoCapture] = DeviceRequest::kRequested;
  }

  // Create a label for this request and verify it is unique.
  std::string request_label;
  do {
    request_label = RandomLabel();
  } while (requests_.find(request_label) != requests_.end());

  requests_.insert(std::make_pair(request_label, new_request));

  // Get user confirmation to use capture devices.
  device_settings_->RequestCaptureDeviceUsage(request_label, render_process_id,
                                              render_view_id, options,
                                              security_origin);
  (*label) = request_label;
}

void MediaStreamManager::CancelRequests(MediaStreamRequester* requester) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DeviceRequests::iterator it = requests_.begin();
  while (it != requests_.end()) {
    if (it->second.requester == requester && !RequestDone(it->second)) {
      // The request isn't complete, but there might be some devices already
      // opened -> close them.
      DeviceRequest* request = &(it->second);
      if (request->state[kAudioCapture] == DeviceRequest::kOpening) {
        for (StreamDeviceInfoArray::iterator it =
             request->audio_devices.begin(); it != request->audio_devices.end();
             ++it) {
          if (it->in_use == true) {
            // TODO(mflodman): Add when audio input device manager is available.
          }
        }
      }
      if (request->state[kVideoCapture] == DeviceRequest::kOpening) {
        for (StreamDeviceInfoArray::iterator it =
             request->video_devices.begin(); it != request->video_devices.end();
             ++it) {
          if (it->in_use == true) {
            video_capture_manager()->Close(it->session_id);
          }
        }
      }
      requests_.erase(it++);
    } else {
      ++it;
    }
  }
}

void MediaStreamManager::StopGeneratedStream(const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Find the request and close all open devices for the request.
  DeviceRequests::iterator it = requests_.find(label);
  if (it != requests_.end()) {
    for (StreamDeviceInfoArray::iterator audio_it =
         it->second.audio_devices.begin();
         audio_it != it->second.audio_devices.end(); ++audio_it) {
      // TODO(mflodman): Add code when audio input manager exists.
      NOTREACHED();
    }
    for (StreamDeviceInfoArray::iterator video_it =
         it->second.video_devices.begin();
         video_it != it->second.video_devices.end(); ++video_it) {
      video_capture_manager()->Close(video_it->session_id);
    }
    requests_.erase(it);
    return;
  }
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
    if (stream_type == kAudioCapture) {
      devices = &(request_it->second.audio_devices);
    } else if (stream_type == kVideoCapture) {
      devices = &(request_it->second.video_devices);
    } else {
      NOTREACHED();
    }

    for (StreamDeviceInfoArray::iterator device_it = devices->begin();
         device_it != devices->end(); ++device_it) {
      if (device_it->session_id == capture_session_id) {
        // We've found the request.
        device_it->in_use = true;
        label = request_it->first;
        request = &(request_it->second);
        break;
      }
    }
  }
  if (request == NULL) {
    // The request doesn't exist.
    return;
  }

  DCHECK_NE(request->state[stream_type], DeviceRequest::kRequested);

  // Check if all devices for this stream type are opened. Update the state if
  // they are.
  for (StreamDeviceInfoArray::iterator device_it = devices->begin();
       device_it != devices->end(); ++device_it) {
    if (device_it->in_use == false) {
      // Wait for more devices to be opened before we're done.
      return;
    }
  }
  request->state[stream_type] = DeviceRequest::kDone;

  if (!RequestDone(*request)) {
    // This stream_type is done, but not the other type.
    return;
  }

  request->requester->StreamGenerated(label, request->audio_devices,
                                      request->video_devices);
}

void MediaStreamManager::Closed(MediaStreamType stream_type,
                                int capture_session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void MediaStreamManager::DevicesEnumerated(
    MediaStreamType stream_type, const StreamDeviceInfoArray& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Publish the result for all requests waiting for device list(s).
  // Find the requests waiting for this device list, store their labels and
  // release the iterator before calling device settings. We might get a call
  // back from device_settings that will need to iterate through devices.
  std::list<std::string> label_list;
  for (DeviceRequests::iterator it = requests_.begin(); it != requests_.end();
       ++it) {
    if (it->second.state[stream_type] == DeviceRequest::kRequested &&
        Requested(it->second.options, stream_type)) {
      label_list.push_back(it->first);
    }
  }
  for (std::list<std::string>::iterator it = label_list.begin();
       it != label_list.end(); ++it) {
    device_settings_->AvailableDevices(*it, stream_type, devices);
  }
  label_list.clear();
  enumeration_in_progress_[stream_type] = false;
}

void MediaStreamManager::Error(MediaStreamType stream_type,
                               int capture_session_id,
                               MediaStreamProviderError error) {
  // Find the device for the error call.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  for (DeviceRequests::iterator it = requests_.begin(); it != requests_.end();
       ++it) {
    StreamDeviceInfoArray* devices = NULL;
    if (stream_type == kAudioCapture) {
      devices = &(it->second.audio_devices);
    } else if (stream_type == kVideoCapture) {
      devices = &(it->second.video_devices);
    } else {
      NOTREACHED();
    }

    int device_idx = 0;
    for (StreamDeviceInfoArray::iterator device_it = devices->begin();
         device_it != devices->end(); ++device_it, ++device_idx) {
      if (device_it->session_id == capture_session_id) {
        // We've found the failing device. Find the error case:
        if (it->second.state[stream_type] == DeviceRequest::kDone) {
          // 1. Already opened -> signal device failure and close device.
          //    Use device_idx to signal which of the devices encountered an
          //    error.
          if (stream_type == kAudioCapture) {
            it->second.requester->AudioDeviceFailed(it->first, device_idx);
          } else if (stream_type == kVideoCapture) {
            it->second.requester->VideoDeviceFailed(it->first, device_idx);
          }
          GetDeviceManager(stream_type)->Close(capture_session_id);
          devices->erase(device_it);
        } else if (it->second.audio_devices.size()
            + it->second.video_devices.size() <= 1) {
          // 2. Device not opened and no other devices for this request ->
          //    signal stream error and remove the request.
          it->second.requester->StreamGenerationFailed(it->first);
          requests_.erase(it);
        } else {
          // 3. Not opened but other devices exists for this request -> remove
          //    device from list, but don't signal an error.
          devices->erase(device_it);
        }
        return;
      }
    }
  }
}

void MediaStreamManager::GetDevices(const std::string& label,
                                    MediaStreamType stream_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enumeration_in_progress_[stream_type]) {
    enumeration_in_progress_[stream_type] = true;
    GetDeviceManager(stream_type)->EnumerateDevices();
  }
}

void MediaStreamManager::DevicesAccepted(const std::string& label,
                                         const StreamDeviceInfoArray& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DeviceRequests::iterator request_it = requests_.find(label);
  if (request_it != requests_.end()) {
    if (devices.empty()) {
      // No available devices or user didn't accept device usage.
      request_it->second.requester->StreamGenerationFailed(request_it->first);
      requests_.erase(request_it);
      return;
    }

    // Loop through all device types for this request.
    for (StreamDeviceInfoArray::const_iterator device_it = devices.begin();
         device_it != devices.end(); ++device_it) {
      StreamDeviceInfo device_info = *device_it;

      // Set in_use to false to be able to track if this device has been
      // opened. in_use might be true if the device type can be used in more
      // than one session.
      device_info.in_use = false;
      device_info.session_id =
          GetDeviceManager(device_info.stream_type)->Open(device_info);
      request_it->second.state[device_it->stream_type] =
          DeviceRequest::kOpening;
      if (device_info.stream_type == kAudioCapture) {
        request_it->second.audio_devices.push_back(device_info);
      } else if (device_info.stream_type == kVideoCapture) {
        request_it->second.video_devices.push_back(device_info);
      } else {
        NOTREACHED();
      }
    }
    // Check if we received all stream types requested.
    if (Requested(request_it->second.options, kAudioCapture) &&
        request_it->second.audio_devices.size() == 0) {
      request_it->second.state[kAudioCapture] = DeviceRequest::kError;
    }
    if (Requested(request_it->second.options, kVideoCapture) &&
        request_it->second.video_devices.size() == 0) {
      request_it->second.state[kVideoCapture] = DeviceRequest::kError;
    }
    return;
  }
}

void MediaStreamManager::SettingsError(const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Erase this request and report an error.
  DeviceRequests::iterator it = requests_.find(label);
  if (it != requests_.end()) {
    it->second.requester->StreamGenerationFailed(label);
    requests_.erase(it);
    return;
  }
}

void MediaStreamManager::UseFakeDevice() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  video_capture_manager()->UseFakeDevice();
  device_settings_->UseFakeUI();
  // TODO(mflodman): Add audio manager when available.
}

bool MediaStreamManager::RequestDone(const DeviceRequest& request) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Check if all devices are opened.
  if (Requested(request.options, kAudioCapture)) {
    for (StreamDeviceInfoArray::const_iterator it =
         request.audio_devices.begin(); it != request.audio_devices.end();
         ++it) {
      if (it->in_use == false) {
        return false;
      }
    }
  }
  if (Requested(request.options, kVideoCapture)) {
    for (StreamDeviceInfoArray::const_iterator it =
         request.video_devices.begin(); it != request.video_devices.end();
         ++it) {
      if (it->in_use == false) {
        return false;
      }
    }
  }
  return true;
}

// Called to get media capture device manager of specified type.
MediaStreamProvider* MediaStreamManager::GetDeviceManager(
    MediaStreamType stream_type) {
  if (stream_type == kVideoCapture) {
    return video_capture_manager();
  } else if (stream_type == kAudioCapture) {
    // TODO(mflodman): Add support when audio input manager is available.
    NOTREACHED();
    return NULL;
  }
  NOTREACHED();
  return NULL;
}

MediaStreamManager::DeviceRequest::DeviceRequest()
    : requester(NULL),
      state(kNumMediaStreamTypes, kNotRequested) {
  options.audio = false;
  options.video_option = StreamOptions::kNoCamera;
}

MediaStreamManager::DeviceRequest::DeviceRequest(
    MediaStreamRequester* requester, const StreamOptions& request_options)
    : requester(requester),
      options(request_options),
      state(kNumMediaStreamTypes, kNotRequested) {
  DCHECK(requester);
}

MediaStreamManager::DeviceRequest::~DeviceRequest() {}

}  // namespace media_stream
