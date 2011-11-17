// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureManager is used to open/close, start/stop, enumerate available
// video capture devices, and manage VideoCaptureController's.
// All functions are expected to be called from Browser::IO thread.
// VideoCaptureManager will open OS dependent instances of VideoCaptureDevice.
// A device can only be opened once.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_

#include <list>
#include <map>

#include "base/threading/thread.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

class VideoCaptureController;
class VideoCaptureControllerEventHandler;

namespace media_stream {

// VideoCaptureManager opens/closes and start/stops video capture devices.
class CONTENT_EXPORT VideoCaptureManager : public MediaStreamProvider {
 public:
  // Calling |Start| of this id will open the first device, even though open has
  // not been called. This is used to be able to use video capture devices
  // before MediaStream is implemented in Chrome and WebKit.
  enum { kStartOpenSessionId = 1 };

  VideoCaptureManager();
  virtual ~VideoCaptureManager();

  // Implements MediaStreamProvider.
  virtual void Register(MediaStreamProviderListener* listener) OVERRIDE;

  virtual void Unregister() OVERRIDE;

  virtual void EnumerateDevices() OVERRIDE;

  virtual int Open(const StreamDeviceInfo& device) OVERRIDE;

  virtual void Close(int capture_session_id) OVERRIDE;

  // Functions used to start and stop media flow.
  // Start allocates the device and no other application can use the device
  // before Stop is called. Captured video frames will be delivered to
  // video_capture_receiver.
  void Start(const media::VideoCaptureParams& capture_params,
             media::VideoCaptureDevice::EventHandler* video_capture_receiver);

  // Stops capture device referenced by |capture_session_id|. No more frames
  // will be delivered to the frame receiver, and |stopped_cb| will be called.
  // |stopped_cb| can be NULL.
  void Stop(const media::VideoCaptureSessionId& capture_session_id,
            base::Closure stopped_cb);

  // A capture device error has occurred for |capture_session_id|. The device
  // won't stream any more captured frames.
  void Error(const media::VideoCaptureSessionId& capture_session_id);

  // Used by unit test to make sure a fake device is used instead of a real
  // video capture device. Due to timing requirements, the function must be
  // called before EnumerateDevices and Open.
  void UseFakeDevice();
  MessageLoop* GetMessageLoop();

  // Called by VideoCaptureHost to get a controller for |capture_params|.
  // The controller is returned via calling |added_cb|.
  void AddController(
      const media::VideoCaptureParams& capture_params,
      VideoCaptureControllerEventHandler* handler,
      base::Callback<void(VideoCaptureController*)> added_cb);
  // Called by VideoCaptureHost to remove the |controller|.
  void RemoveController(
      VideoCaptureController* controller,
      VideoCaptureControllerEventHandler* handler);

 private:
  typedef std::list<VideoCaptureControllerEventHandler*> Handlers;
  struct Controller;

  // Called by the public functions, executed on vc_device_thread_.
  void OnEnumerateDevices();
  void OnOpen(int capture_session_id, const StreamDeviceInfo& device);
  void OnClose(int capture_session_id);
  void OnStart(const media::VideoCaptureParams capture_params,
               media::VideoCaptureDevice::EventHandler* video_capture_receiver);
  void OnStop(const media::VideoCaptureSessionId capture_session_id,
              base::Closure stopped_cb);
  void DoAddControllerOnDeviceThread(
      const media::VideoCaptureParams capture_params,
      VideoCaptureControllerEventHandler* handler,
      base::Callback<void(VideoCaptureController*)> added_cb);
  void DoRemoveControllerOnDeviceThread(
      VideoCaptureController* controller,
      VideoCaptureControllerEventHandler* handler);

  // Executed on Browser::IO thread to call Listener.
  void OnOpened(int capture_session_id);
  void OnClosed(int capture_session_id);
  void OnDevicesEnumerated(const StreamDeviceInfoArray& devices);
  void OnError(int capture_session_id, MediaStreamProviderError error);

  // Executed on vc_device_thread_ to make sure Listener is called from
  // Browser::IO thread.
  void PostOnOpened(int capture_session_id);
  void PostOnClosed(int capture_session_id);
  void PostOnDevicesEnumerated(const StreamDeviceInfoArray& devices);
  void PostOnError(int capture_session_id, MediaStreamProviderError error);

  // Helpers
  void GetAvailableDevices(media::VideoCaptureDevice::Names* device_names);
  bool DeviceOpened(const media::VideoCaptureDevice::Name& device_name);
  bool DeviceInUse(const media::VideoCaptureDevice* video_capture_device);
  media::VideoCaptureDevice* GetOpenedDevice(
      const StreamDeviceInfo& device_info);
  bool IsOnCaptureDeviceThread() const;
  media::VideoCaptureDevice* GetDeviceInternal(int capture_session_id);

  // Thread for all calls to VideoCaptureDevice.
  base::Thread vc_device_thread_;

  // Only accessed on Browser::IO thread.
  MediaStreamProviderListener* listener_;
  int new_capture_session_id_;

  // Only accessed from vc_device_thread_.
  // VideoCaptureManager owns all VideoCaptureDevices and is responsible for
  // deleting the instances when they are not used any longer.
  typedef std::map<int, media::VideoCaptureDevice*> VideoCaptureDevices;
  VideoCaptureDevices devices_;

  // Set to true if using fake devices for testing, false by default.
  bool use_fake_device_;

  // Only accessed from vc_device_thread_.
  // VideoCaptureManager owns all VideoCaptureController's and is responsible
  // for deleting the instances when they are not used any longer.
  // VideoCaptureDevice is one-to-one mapped to VideoCaptureController.
  typedef std::map<media::VideoCaptureDevice*, Controller*> Controllers;
  Controllers controllers_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureManager);
};

}  // namespace media_stream

DISABLE_RUNNABLE_METHOD_REFCOUNT(media_stream::VideoCaptureManager);

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_
