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
                        const scoped_refptr<base::SingleThreadTaskRunner>&
                            device_task_runner) OVERRIDE;

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
  // value of |session_id| controls which device is selected;
  // this value should be a session id previously returned by Open().
  //
  // If the device is not already started (i.e., no other client is currently
  // capturing from this device), this call will cause a VideoCaptureController
  // and VideoCaptureDevice to be created, possibly asynchronously.
  //
  // On success, the controller is returned via calling |done_cb|, indicating
  // that the client was successfully added. A NULL controller is passed to
  // the callback on failure.
  void StartCaptureForClient(media::VideoCaptureSessionId session_id,
                             const media::VideoCaptureParams& capture_params,
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

  // Retrieves all capture supported formats for a particular device. Returns
  // false if the |capture_session_id| is not found. The supported formats are
  // cached during device(s) enumeration, and depending on the underlying
  // implementation, could be an empty list.
  bool GetDeviceSupportedFormats(
      media::VideoCaptureSessionId capture_session_id,
      media::VideoCaptureFormats* supported_formats);

  // Retrieves the format(s) currently in use.  Returns false if the
  // |capture_session_id| is not found. Returns true and |formats_in_use|
  // otherwise. |formats_in_use| is empty if the device is not in use.
  bool GetDeviceFormatsInUse(media::VideoCaptureSessionId capture_session_id,
                             media::VideoCaptureFormats* formats_in_use);

 private:
  virtual ~VideoCaptureManager();
  struct DeviceEntry;

  // This data structure is a convenient wrap of a devices' name and associated
  // video capture supported formats.
  struct DeviceInfo {
    DeviceInfo();
    DeviceInfo(const media::VideoCaptureDevice::Name& name,
               const media::VideoCaptureFormats& supported_formats);
    ~DeviceInfo();

    media::VideoCaptureDevice::Name name;
    media::VideoCaptureFormats supported_formats;
  };
  typedef std::vector<DeviceInfo> DeviceInfos;

  // Check to see if |entry| has no clients left on its controller. If so,
  // remove it from the list of devices, and delete it asynchronously. |entry|
  // may be freed by this function.
  void DestroyDeviceEntryIfNoClients(DeviceEntry* entry);

  // Helpers to report an event to our Listener.
  void OnOpened(MediaStreamType type,
                media::VideoCaptureSessionId capture_session_id);
  void OnClosed(MediaStreamType type,
                media::VideoCaptureSessionId capture_session_id);
  void OnDevicesInfoEnumerated(
      MediaStreamType stream_type,
      const DeviceInfos& new_devices_info_cache);

  // Find a DeviceEntry by its device ID and type, if it is already opened.
  DeviceEntry* GetDeviceEntryForMediaStreamDevice(
      const MediaStreamDevice& device_info);

  // Find a DeviceEntry entry for the indicated session, creating a fresh one
  // if necessary. Returns NULL if the session id is invalid.
  DeviceEntry* GetOrCreateDeviceEntry(
      media::VideoCaptureSessionId capture_session_id);

  // Find the DeviceEntry that owns a particular controller pointer.
  DeviceEntry* GetDeviceEntryForController(
      const VideoCaptureController* controller);

  bool IsOnDeviceThread() const;

  // Queries the Names of the devices in the system; the formats supported by
  // the new devices are also queried, and consolidated with the copy of the
  // local device info cache passed. The consolidated list of devices and
  // supported formats is returned.
  DeviceInfos GetAvailableDevicesInfoOnDeviceThread(
      MediaStreamType stream_type,
      const DeviceInfos& old_device_info_cache);

  // Create and Start a new VideoCaptureDevice, storing the result in
  // |entry->video_capture_device|. Ownership of |client| passes to
  // the device.
  void DoStartDeviceOnDeviceThread(
      DeviceEntry* entry,
      const media::VideoCaptureParams& params,
      scoped_ptr<media::VideoCaptureDevice::Client> client);

  // Stop and destroy the VideoCaptureDevice held in
  // |entry->video_capture_device|.
  void DoStopDeviceOnDeviceThread(DeviceEntry* entry);

  DeviceInfo* FindDeviceInfoById(const std::string& id,
                                 DeviceInfos& device_vector);

  // The message loop of media stream device thread, where VCD's live.
  scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  // Only accessed on Browser::IO thread.
  MediaStreamProviderListener* listener_;
  media::VideoCaptureSessionId new_capture_session_id_;

  // An entry is kept in this map for every session that has been created via
  // the Open() entry point. The keys are session_id's. This map is used to
  // determine which device to use when StartCaptureForClient() occurs. Used
  // only on the IO thread.
  std::map<media::VideoCaptureSessionId, MediaStreamDevice> sessions_;

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

  // Local cache of the enumerated video capture devices' names and capture
  // supported formats. A snapshot of the current devices and their capabilities
  // is composed in GetAvailableDevicesInfoOnDeviceThread() --coming
  // from EnumerateDevices()--, and this snapshot is used to update this list in
  // OnDevicesInfoEnumerated(). GetDeviceSupportedFormats() will
  // use this list if the device is not started, otherwise it will retrieve the
  // active device capture format from the VideoCaptureController associated.
  DeviceInfos devices_info_cache_;

  // For unit testing and for performance/quality tests, a test device can be
  // used instead of a real one. The device can be a simple fake device (a
  // rolling pacman), or a file that is played in a loop continuously. This only
  // applies to the MEDIA_DEVICE_VIDEO_CAPTURE device type.
  enum {
    DISABLED,
    TEST_PATTERN,
    Y4M_FILE
  } artificial_device_source_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_
