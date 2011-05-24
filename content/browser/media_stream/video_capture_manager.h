// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureManager is used to open/close, start/stop as well as enumerate
// available video capture devices. All functions are expected to be called from
// the Browser::IO thread. VideoCaptureManager will open OS dependent instances
// of VideoCaptureDevice. A device can only be opened once.

#ifndef CONTENT_BROWSER_MEDIA_STREAM_VIDEO_CAPTURE_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_STREAM_VIDEO_CAPTURE_MANAGER_H_

#include <map>

#include "base/lazy_instance.h"
#include "base/threading/thread.h"
#include "content/browser/media_stream/media_stream_provider.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

namespace media_stream {

// VideoCaptureManager opens/closes and start/stops video capture devices.
class VideoCaptureManager : public MediaStreamProvider {
 public:
  // Calling |Start| of this id will open the first device, even though open has
  // not been called. This is used to be able to use video capture devices
  // before MediaStream is implemented in Chrome and WebKit.
  enum { kStartOpenSessionId = 1 };

  // Called to get a pointer to the singleton
  static VideoCaptureManager* Get();

  // Implements MediaStreamProvider.
  virtual bool Register(MediaStreamProviderListener* listener);

  virtual void Unregister();

  virtual void EnumerateDevices();

  virtual MediaCaptureSessionId Open(const MediaCaptureDeviceInfo& device);

  virtual void Close(MediaCaptureSessionId capture_session_id);

  // Functions used to start and stop media flow.
  // Start allocates the device and no other application can use the device
  // before Stop is called. Captured video frames will be delivered to
  // video_capture_receiver.
  void Start(const media::VideoCaptureParams& capture_params,
             media::VideoCaptureDevice::EventHandler* video_capture_receiver);

  // Stops capture device referenced by |capture_session_id|. No more frames
  // will be delivered to the frame receiver, and |stopped_task| will be called.
  void Stop(const media::VideoCaptureSessionId capture_session_id,
            Task* stopped_task);

  virtual ~VideoCaptureManager();

  // Used by unit test to make sure a fake device is used instead of a real
  // video capture device. Due to timing requirements, the function must be
  // called before EnumerateDevices and Open.
  void UseFakeDevice();
  MessageLoop* GetMessageLoop();

 private:
  friend struct base::DefaultLazyInstanceTraits<VideoCaptureManager>;

  VideoCaptureManager();

  // Called by the public functions, executed on vc_device_thread_.
  void OnEnumerateDevices();
  void OnOpen(MediaCaptureSessionId capture_session_id,
              const MediaCaptureDeviceInfo device);
  void OnClose(MediaCaptureSessionId capture_session_id);
  void OnStart(const media::VideoCaptureParams capture_params,
               media::VideoCaptureDevice::EventHandler* video_capture_receiver);
  void OnStop(const media::VideoCaptureSessionId capture_session_id,
              Task* stopped_task);


  // Executed on Browser::IO thread to call Listener.
  void OnOpened(MediaCaptureSessionId capture_session_id);
  void OnClosed(MediaCaptureSessionId capture_session_id);
  void OnDevicesEnumerated(const MediaCaptureDevices& devices);
  void OnError(MediaCaptureSessionId capture_session_id,
               MediaStreamProviderError error);

  // Executed on vc_device_thread_ to make sure Listener is called from
  // Browser::IO thread.
  void PostOnOpened(MediaCaptureSessionId capture_session_id);
  void PostOnClosed(MediaCaptureSessionId capture_session_id);
  void PostOnDevicesEnumerated(MediaCaptureDevices devices);
  void PostOnError(MediaCaptureSessionId capture_session_id,
                   MediaStreamProviderError error);

  // Helpers
  void GetAvailableDevices(media::VideoCaptureDevice::Names* device_names);
  bool DeviceOpened(const media::VideoCaptureDevice::Name& device_name);
  bool DeviceOpened(const MediaCaptureDeviceInfo& device_info);
  bool IsOnCaptureDeviceThread() const;

  // Thread for all calls to VideoCaptureDevice
  base::Thread vc_device_thread_;

  // Only accessed on Browser::IO thread
  MediaStreamProviderListener* listener_;
  MediaCaptureSessionId new_capture_session_id_;

  // Only accessed from vc_device_thread_
  // TODO(mflodman) Change map key type when changing typedef for
  // MediaCaptureSessionId.
  typedef std::map<int, media::VideoCaptureDevice*> VideoCaptureDevices;
  VideoCaptureDevices devices_;

  // Set to true if using fake devices for testing, false by default.
  bool use_fake_device_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureManager);
};

}  // namespace media_stream

DISABLE_RUNNABLE_METHOD_REFCOUNT(media_stream::VideoCaptureManager);

#endif  // CONTENT_BROWSER_MEDIA_STREAM_VIDEO_CAPTURE_MANAGER_H_
