// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaStreamManager is used to open/enumerate media capture devices (video
// supported now). Call flow:
// 1. GenerateStream is called when a render process wants to use a capture
//    device.
// 2. MediaStreamManager will ask MediaStreamDeviceSettings for permission to
//    use devices and for which device to use.
// 3. MediaStreamManager will request the corresponding media device manager(s)
//    to enumerate available devices. The result will be given to
//    MediaStreamDeviceSettings.
// 4. MediaStreamDeviceSettings will, by using user settings, pick devices which
//    devices to use and let MediaStreamManager know the result.
// 5. MediaStreamManager will call the proper media device manager to open the
//    device and let the MediaStreamRequester know it has been done.

// When enumeration and open are done in separate operations,
// MediaStreamDeviceSettings is not involved as in steps.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/media_stream_settings_requester.h"
#include "content/common/media/media_stream_options.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace base {
namespace win {
class ScopedCOMInitializer;
}
}

namespace media_stream {

class AudioInputDeviceManager;
class MediaStreamDeviceSettings;
class MediaStreamRequester;
class VideoCaptureManager;

// Thread that enters MTA on windows, and is base::Thread on linux and mac.
class DeviceThread : public base::Thread {
 public:
  explicit DeviceThread(const char* name);
  virtual ~DeviceThread();

 protected:
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

 private:
  scoped_ptr<base::win::ScopedCOMInitializer> com_initializer_;
  DISALLOW_COPY_AND_ASSIGN(DeviceThread);
};

// MediaStreamManager is used to generate and close new media devices, not to
// start the media flow.
// The classes requesting new media streams are answered using
// MediaStreamManager::Listener.
class CONTENT_EXPORT MediaStreamManager
    : public MediaStreamProviderListener,
      public MessageLoop::DestructionObserver,
      public SettingsRequester {
 public:
  // This class takes the ownerships of the |audio_input_device_manager|
  // and |video_capture_manager|.
  MediaStreamManager(AudioInputDeviceManager* audio_input_device_manager,
                     VideoCaptureManager* video_capture_manager);

  virtual ~MediaStreamManager();

  // Used to access VideoCaptureManager.
  VideoCaptureManager* video_capture_manager();

  // Used to access AudioInputDeviceManager.
  AudioInputDeviceManager* audio_input_device_manager();

  // GenerateStream opens new media devices according to |components|. It
  // creates a new request which is identified by a unique |label| that's
  // returned to the caller.
  void GenerateStream(MediaStreamRequester* requester, int render_process_id,
                      int render_view_id, const StreamOptions& options,
                      const GURL& security_origin, std::string* label);

  // Cancels all non-finished GenerateStream request, i.e. request for which
  // StreamGenerated hasn't been called.
  void CancelRequests(MediaStreamRequester* requester);

  // Cancel generate stream.
  void CancelGenerateStream(const std::string& label);

  // Closes generated stream.
  void StopGeneratedStream(const std::string& label);

  // Gets a list of devices of |type|.
  // The request is identified using |label|, which is pointing to a
  // std::string.
  void EnumerateDevices(MediaStreamRequester* requester,
                        int render_process_id,
                        int render_view_id,
                        MediaStreamType type,
                        const GURL& security_origin,
                        std::string* label);

  // Open a device identified by |device_id|.
  // The request is identified using |label|, which is pointing to a
  // std::string.
  void OpenDevice(MediaStreamRequester* requester,
                  int render_process_id,
                  int render_view_id,
                  const std::string& device_id,
                  MediaStreamType type,
                  const GURL& security_origin,
                  std::string* label);

  // Implements MediaStreamProviderListener.
  virtual void Opened(MediaStreamType stream_type,
                      int capture_session_id) OVERRIDE;
  virtual void Closed(MediaStreamType stream_type,
                      int capture_session_id) OVERRIDE;
  virtual void DevicesEnumerated(MediaStreamType stream_type,
                                 const StreamDeviceInfoArray& devices) OVERRIDE;
  virtual void Error(MediaStreamType stream_type,
                     int capture_session_id,
                     MediaStreamProviderError error) OVERRIDE;

  // Implements SettingsRequester.
  virtual void DevicesAccepted(const std::string& label,
                               const StreamDeviceInfoArray& devices) OVERRIDE;
  virtual void SettingsError(const std::string& label) OVERRIDE;

  // Used by unit test to make sure fake devices are used instead of a real
  // devices, which is needed for server based testing.
  // TODO(xians): Remove this hack since we can create our own
  // MediaStreamManager in our unit tests.
  void UseFakeDevice();

  // This object gets deleted on the UI thread after the IO thread has been
  // destroyed. So we need to know when IO thread is being destroyed so that
  // we can delete VideoCaptureManager and AudioInputDeviceManager.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

 private:
  // Contains all data needed to keep track of requests.
  struct DeviceRequest;

  // Helpers for signaling the media observer that new capture devices are
  // opened/closed.
  void NotifyObserverDevicesOpened(DeviceRequest* request);
  void NotifyObserverDevicesClosed(DeviceRequest* request);
  void DevicesFromRequest(DeviceRequest* request,
                          content::MediaStreamDevices* devices);

  // Helpers.
  bool RequestDone(const MediaStreamManager::DeviceRequest& request) const;
  MediaStreamProvider* GetDeviceManager(MediaStreamType stream_type);
  void StartEnumeration(DeviceRequest* new_request,
                        std::string* label);

  // Helper to ensure the device thread and pass the message loop to device
  // managers, it also register itself as the listener to the device managers.
  void EnsureDeviceThreadAndListener();

  // Device thread shared by VideoCaptureManager and AudioInputDeviceManager.
  scoped_ptr<base::Thread> device_thread_;

  scoped_ptr<MediaStreamDeviceSettings> device_settings_;
  scoped_refptr<AudioInputDeviceManager> audio_input_device_manager_;
  scoped_refptr<VideoCaptureManager> video_capture_manager_;

  // Keeps track of device types currently being enumerated to not enumerate
  // when not necessary.
  std::vector<bool> enumeration_in_progress_;

  // All non-closed request.
  typedef std::map<std::string, DeviceRequest> DeviceRequests;
  DeviceRequests requests_;

  // Hold a pointer to the IO loop to check we delete the device thread and
  // managers on the right thread.
  MessageLoop* io_loop_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamManager);
};

}  // namespace media_stream

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_
