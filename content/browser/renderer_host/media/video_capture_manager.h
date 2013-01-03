// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

namespace content {
class MockVideoCaptureManager;
class VideoCaptureController;
class VideoCaptureControllerEventHandler;

// VideoCaptureManager opens/closes and start/stops video capture devices.
class CONTENT_EXPORT VideoCaptureManager : public MediaStreamProvider {
 public:
  // Calling |Start| of this id will open the first device, even though open has
  // not been called. This is used to be able to use video capture devices
  // before MediaStream is implemented in Chrome and WebKit.
  enum { kStartOpenSessionId = 1 };

  VideoCaptureManager();

  // Implements MediaStreamProvider.
  virtual void Register(MediaStreamProviderListener* listener,
                        base::MessageLoopProxy* device_thread_loop) OVERRIDE;

  virtual void Unregister() OVERRIDE;

  virtual void EnumerateDevices(MediaStreamType stream_type) OVERRIDE;

  virtual int Open(const StreamDeviceInfo& device) OVERRIDE;

  virtual void Close(int capture_session_id) OVERRIDE;

  // Functions used to start and stop media flow.
  // Start allocates the device and no other application can use the device
  // before Stop is called. Captured video frames will be delivered to
  // video_capture_receiver.
  virtual void Start(const media::VideoCaptureParams& capture_params,
             media::VideoCaptureDevice::EventHandler* video_capture_receiver);

  // Stops capture device referenced by |capture_session_id|. No more frames
  // will be delivered to the frame receiver, and |stopped_cb| will be called.
  // |stopped_cb| can be NULL.
  virtual void Stop(const media::VideoCaptureSessionId& capture_session_id,
            base::Closure stopped_cb);

  // Used by unit test to make sure a fake device is used instead of a real
  // video capture device. Due to timing requirements, the function must be
  // called before EnumerateDevices and Open.
  void UseFakeDevice();

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
  friend class MockVideoCaptureManager;

  virtual ~VideoCaptureManager();

  typedef std::list<VideoCaptureControllerEventHandler*> Handlers;
  struct Controller;

  // Called by the public functions, executed on device thread.
  void OnEnumerateDevices(MediaStreamType stream_type);
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
  void OnOpened(MediaStreamType type, int capture_session_id);
  void OnClosed(MediaStreamType type, int capture_session_id);
  void OnDevicesEnumerated(MediaStreamType stream_type,
                           scoped_ptr<StreamDeviceInfoArray> devices);
  void OnError(MediaStreamType type, int capture_session_id,
               MediaStreamProviderError error);

  // Executed on device thread to make sure Listener is called from
  // Browser::IO thread.
  void PostOnOpened(MediaStreamType type, int capture_session_id);
  void PostOnClosed(MediaStreamType type, int capture_session_id);
  void PostOnDevicesEnumerated(MediaStreamType stream_type,
                               scoped_ptr<StreamDeviceInfoArray> devices);
  void PostOnError(int capture_session_id, MediaStreamProviderError error);

  // Helpers
  void GetAvailableDevices(MediaStreamType stream_type,
                           media::VideoCaptureDevice::Names* device_names);
  bool DeviceOpened(const media::VideoCaptureDevice::Name& device_name);
  bool DeviceInUse(const media::VideoCaptureDevice* video_capture_device);
  media::VideoCaptureDevice* GetOpenedDevice(
      const StreamDeviceInfo& device_info);
  bool IsOnDeviceThread() const;
  media::VideoCaptureDevice* GetDeviceInternal(int capture_session_id);

  // The message loop of media stream device thread that this object runs on.
  scoped_refptr<base::MessageLoopProxy> device_loop_;

  // Only accessed on Browser::IO thread.
  MediaStreamProviderListener* listener_;
  int new_capture_session_id_;

  // Only accessed from device thread.
  // VideoCaptureManager owns all VideoCaptureDevices and is responsible for
  // deleting the instances when they are not used any longer.
  struct DeviceEntry {
    MediaStreamType stream_type;
    media::VideoCaptureDevice* capture_device;  // Maybe shared across sessions.
  };
  typedef std::map<int, DeviceEntry> VideoCaptureDevices;
  VideoCaptureDevices devices_;  // Maps capture_session_id to DeviceEntry.

  // Set to true if using fake devices for testing, false by default.
  bool use_fake_device_;

  // Only accessed from device thread.
  // VideoCaptureManager owns all VideoCaptureController's and is responsible
  // for deleting the instances when they are not used any longer.
  // VideoCaptureDevice is one-to-one mapped to VideoCaptureController.
  typedef std::map<media::VideoCaptureDevice*, Controller*> Controllers;
  Controllers controllers_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_
