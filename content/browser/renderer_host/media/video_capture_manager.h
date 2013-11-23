// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureManager is used to open/close, start/stop, enumerate available
// video capture devices, and manage VideoCaptureController's.
// All functions are expected to be called from Browser::IO thread. Some helper
// functions (*OnDeviceThread) will dispatch operations to the device thread.
// VideoCaptureManager will open OS dependent instances of VideoCaptureDevice.
// A device can only be opened once.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

namespace content {
class VideoCaptureController;
class VideoCaptureControllerEventHandler;

// VideoCaptureManager opens/closes and start/stops video capture devices.
class CONTENT_EXPORT VideoCaptureManager : public MediaStreamProvider {
 public:
  // Callback used to signal the completion of a controller lookup.
  typedef base::Callback<
      void(const base::WeakPtr<VideoCaptureController>&)> DoneCB;

  VideoCaptureManager();

  // Implements MediaStreamProvider.
  virtual void Register(MediaStreamProviderListener* listener,
                        base::MessageLoopProxy* device_thread_loop) OVERRIDE;

  virtual void Unregister() OVERRIDE;

  virtual void EnumerateDevices(MediaStreamType stream_type) OVERRIDE;

  virtual int Open(const StreamDeviceInfo& device) OVERRIDE;

  virtual void Close(int capture_session_id) OVERRIDE;

  // Used by unit test to make sure a fake device is used instead of a real
  // video capture device. Due to timing requirements, the function must be
  // called before EnumerateDevices and Open.
  void UseFakeDevice();

  // Called by VideoCaptureHost to locate a capture device for |capture_params|,
  // adding the Host as a client of the device's controller if successful. The
  // value of |capture_params.session_id| controls which device is selected;
  // this value should be a session id previously returned by Open().
  //
  // If the device is not already started (i.e., no other client is currently
  // capturing from this device), this call will cause a VideoCaptureController
  // and VideoCaptureDevice to be created, possibly asynchronously.
  //
  // On success, the controller is returned via calling |done_cb|, indicating
  // that the client was successfully added. A NULL controller is passed to
  // the callback on failure.
  void StartCaptureForClient(const media::VideoCaptureParams& capture_params,
                             base::ProcessHandle client_render_process,
                             VideoCaptureControllerID client_id,
                             VideoCaptureControllerEventHandler* client_handler,
                             const DoneCB& done_cb);

  // Called by VideoCaptureHost to remove |client_handler|. If this is the last
  // client of the device, the |controller| and its VideoCaptureDevice may be
  // destroyed. The client must not access |controller| after calling this
  // function.
  void StopCaptureForClient(VideoCaptureController* controller,
                            VideoCaptureControllerID client_id,
                            VideoCaptureControllerEventHandler* client_handler);

 private:
  virtual ~VideoCaptureManager();
  struct DeviceEntry;

  // Check to see if |entry| has no clients left on its controller. If so,
  // remove it from the list of devices, and delete it asynchronously. |entry|
  // may be freed by this function.
  void DestroyDeviceEntryIfNoClients(DeviceEntry* entry);

  // Helpers to report an event to our Listener.
  void OnOpened(MediaStreamType type, int capture_session_id);
  void OnClosed(MediaStreamType type, int capture_session_id);
  void OnDevicesEnumerated(MediaStreamType stream_type,
                           const media::VideoCaptureDevice::Names& names);

  // Find a DeviceEntry by its device ID and type, if it is already opened.
  DeviceEntry* GetDeviceEntryForMediaStreamDevice(
      const MediaStreamDevice& device_info);

  // Find a DeviceEntry entry for the indicated session, creating a fresh one
  // if necessary. Returns NULL if the session id is invalid.
  DeviceEntry* GetOrCreateDeviceEntry(int capture_session_id);

  // Find the DeviceEntry that owns a particular controller pointer.
  DeviceEntry* GetDeviceEntryForController(
      const VideoCaptureController* controller);

  bool IsOnDeviceThread() const;

  // Queries and returns the available device IDs.
  media::VideoCaptureDevice::Names GetAvailableDevicesOnDeviceThread(
      MediaStreamType stream_type);

  // Create and Start a new VideoCaptureDevice, storing the result in
  // |entry->video_capture_device|. Ownership of |client| passes to
  // the device.
  void DoStartDeviceOnDeviceThread(
      DeviceEntry* entry,
      const media::VideoCaptureCapability& capture_params,
      scoped_ptr<media::VideoCaptureDevice::Client> client);

  // Stop and destroy the VideoCaptureDevice held in
  // |entry->video_capture_device|.
  void DoStopDeviceOnDeviceThread(DeviceEntry* entry);

  // The message loop of media stream device thread, where VCD's live.
  scoped_refptr<base::MessageLoopProxy> device_loop_;

  // Only accessed on Browser::IO thread.
  MediaStreamProviderListener* listener_;
  int new_capture_session_id_;

  // An entry is kept in this map for every session that has been created via
  // the Open() entry point. The keys are session_id's. This map is used to
  // determine which device to use when StartCaptureForClient() occurs. Used
  // only on the IO thread.
  std::map<int, MediaStreamDevice> sessions_;

  // An entry, kept in a map, that owns a VideoCaptureDevice and its associated
  // VideoCaptureController. VideoCaptureManager owns all VideoCaptureDevices
  // and VideoCaptureControllers and is responsible for deleting the instances
  // when they are not used any longer.
  //
  // The set of currently started VideoCaptureDevice and VideoCaptureController
  // objects is only accessed from IO thread, though the DeviceEntry instances
  // themselves may visit to the device thread for device creation and
  // destruction.
  struct DeviceEntry {
    DeviceEntry(MediaStreamType stream_type,
                const std::string& id,
                scoped_ptr<VideoCaptureController> controller);
    ~DeviceEntry();

    const MediaStreamType stream_type;
    const std::string id;

    // The controller. Only used from the IO thread.
    scoped_ptr<VideoCaptureController> video_capture_controller;

    // The capture device. Only used from the device thread.
    scoped_ptr<media::VideoCaptureDevice> video_capture_device;
  };
  typedef std::set<DeviceEntry*> DeviceEntries;
  DeviceEntries devices_;

  // Set to true if using fake video capture devices for testing, false by
  // default. This is only used for the MEDIA_DEVICE_VIDEO_CAPTURE device type.
  bool use_fake_device_;

  // We cache the enumerated video capture devices in
  // GetAvailableDevicesOnDeviceThread() and then later look up the requested ID
  // when a device is created in DoStartDeviceOnDeviceThread(). Used only on the
  // device thread.
  media::VideoCaptureDevice::Names video_capture_devices_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_
