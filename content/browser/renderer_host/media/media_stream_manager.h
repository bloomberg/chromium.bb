// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaStreamManager is used to open/enumerate media capture devices (video
// supported now). Call flow:
// 1. GenerateStream is called when a render process wants to use a capture
//    device.
// 2. MediaStreamManager will ask MediaStreamUIController for permission to
//    use devices and for which device to use.
// 3. MediaStreamManager will request the corresponding media device manager(s)
//    to enumerate available devices. The result will be given to
//    MediaStreamUIController.
// 4. MediaStreamUIController will, by posting the request to UI, let the
//    users to select which devices to use and send callback to
//    MediaStreamManager with the result.
// 5. MediaStreamManager will call the proper media device manager to open the
//    device and let the MediaStreamRequester know it has been done.

// When enumeration and open are done in separate operations,
// MediaStreamUIController is not involved as in steps.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/system_monitor/system_monitor.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"

namespace base {
class Thread;
}

namespace media {
class AudioManager;
}

namespace content {

class AudioInputDeviceManager;
class FakeMediaStreamUIProxy;
class MediaStreamDeviceSettings;
class MediaStreamRequester;
class MediaStreamUIProxy;
class VideoCaptureManager;

// MediaStreamManager is used to generate and close new media devices, not to
// start the media flow.
// The classes requesting new media streams are answered using
// MediaStreamManager::Listener.
class CONTENT_EXPORT MediaStreamManager
    : public MediaStreamProviderListener,
      public base::MessageLoop::DestructionObserver,
      public base::SystemMonitor::DevicesChangedObserver {
 public:
  // Callback to deliver the result of a media request. |label| is the string
  // to identify the request,
  typedef base::Callback<void(const MediaStreamDevices& devices,
                              scoped_ptr<MediaStreamUIProxy> ui)>
      MediaRequestResponseCallback;

  explicit MediaStreamManager(media::AudioManager* audio_manager);
  virtual ~MediaStreamManager();

  // Used to access VideoCaptureManager.
  VideoCaptureManager* video_capture_manager();

  // Used to access AudioInputDeviceManager.
  AudioInputDeviceManager* audio_input_device_manager();

  // Creates a new media access request which is identified by a unique string
  // that's returned to the caller. This will trigger the infobar and ask users
  // for access to the device. |render_process_id| and |render_view_id| refer
  // to the view where the infobar will appear to the user. |callback| is
  // used to send the selected device to the clients. An empty list of device
  // will be returned if the users deny the access.
  std::string MakeMediaAccessRequest(
      int render_process_id,
      int render_view_id,
      int page_request_id,
      const StreamOptions& components,
      const GURL& security_origin,
      const MediaRequestResponseCallback& callback);

  // GenerateStream opens new media devices according to |components|.  It
  // creates a new request which is identified by a unique string that's
  // returned to the caller.  |render_process_id| and |render_view_id| refer to
  // the view where the infobar will appear to the user.
  std::string GenerateStream(MediaStreamRequester* requester,
                             int render_process_id,
                             int render_view_id,
                             int page_request_id,
                             const StreamOptions& components,
                             const GURL& security_origin);

  void CancelRequest(const std::string& label);

  // Closes generated stream.
  virtual void StopGeneratedStream(const std::string& label);

  // Gets a list of devices of |type|, which must be MEDIA_DEVICE_AUDIO_CAPTURE
  // or MEDIA_DEVICE_VIDEO_CAPTURE.
  // The request is identified using the string returned to the caller.
  // When the |requester| is NULL, MediaStreamManager will enumerate both audio
  // and video devices and also start monitoring device changes, such as
  // plug/unplug. The new device lists will be delivered via media observer to
  // MediaCaptureDevicesDispatcher.
  virtual std::string EnumerateDevices(MediaStreamRequester* requester,
                                       int render_process_id,
                                       int render_view_id,
                                       int page_request_id,
                                       MediaStreamType type,
                                       const GURL& security_origin);

  // Open a device identified by |device_id|.  |type| must be either
  // MEDIA_DEVICE_AUDIO_CAPTURE or MEDIA_DEVICE_VIDEO_CAPTURE.
  // The request is identified using string returned to the caller.
  std::string OpenDevice(MediaStreamRequester* requester,
                         int render_process_id,
                         int render_view_id,
                         int page_request_id,
                         const std::string& device_id,
                         MediaStreamType type,
                         const GURL& security_origin);

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

  // Implements base::SystemMonitor::DevicesChangedObserver.
  virtual void OnDevicesChanged(
      base::SystemMonitor::DeviceType device_type) OVERRIDE;

  // Used by unit test to make sure fake devices are used instead of a real
  // devices, which is needed for server based testing or certain tests (which
  // can pass --use-fake-device-for-media-stream).
  void UseFakeDevice();

  // Called by the tests to specify a fake UI that should be used for next
  // generated stream (or when using --use-fake-ui-for-media-stream).
  void UseFakeUI(scoped_ptr<FakeMediaStreamUIProxy> fake_ui);

  // This object gets deleted on the UI thread after the IO thread has been
  // destroyed. So we need to know when IO thread is being destroyed so that
  // we can delete VideoCaptureManager and AudioInputDeviceManager.
  // We also must call this function explicitly in tests which use
  // TestBrowserThreadBundle, because the notification happens too late in that
  // case (see http://crbug.com/247525#c14).
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

 protected:
  // Used for testing.
  MediaStreamManager();

 private:
  // Contains all data needed to keep track of requests.
  class DeviceRequest;

  // Cache enumerated device list.
  struct EnumerationCache {
    EnumerationCache();
    ~EnumerationCache();

    bool valid;
    StreamDeviceInfoArray devices;
  };

  typedef std::map<std::string, DeviceRequest*> DeviceRequests;

  // Initializes the device managers on IO thread.  Auto-starts the device
  // thread and registers this as a listener with the device managers.
  void InitializeDeviceManagersOnIOThread();

  // Helper for sending up-to-date device lists to media observer when a
  // capture device is plugged in or unplugged.
  void NotifyDevicesChanged(MediaStreamType stream_type,
                            const StreamDeviceInfoArray& devices);


  void HandleAccessRequestResponse(const std::string& label,
                                   const MediaStreamDevices& devices);
  void StopStreamFromUI(const std::string& label);

  // Helpers.
  bool RequestDone(const DeviceRequest& request) const;
  MediaStreamProvider* GetDeviceManager(MediaStreamType stream_type);
  void StartEnumeration(DeviceRequest* request);
  std::string AddRequest(DeviceRequest* request);
  void RemoveRequest(DeviceRequests::iterator it);
  void ClearEnumerationCache(EnumerationCache* cache);
  void PostRequestToUI(const std::string& label);
  void HandleRequest(const std::string& label);

  // Sends cached device list to a client corresponding to the request
  // identified by |label|.
  void SendCachedDeviceList(EnumerationCache* cache, const std::string& label);

  // Stop the request of enumerating devices indentified by |label|.
  void StopEnumerateDevices(const std::string& label);

  // Helpers to start and stop monitoring devices.
  void StartMonitoring();
  void StopMonitoring();

  // Finds and returns the raw device id corresponding to the given
  // |device_guid|. Returns true if there was a raw device id that matched the
  // given |device_guid|, false if nothing matched it.
  bool TranslateGUIDToRawId(
      MediaStreamType stream_type,
      const GURL& security_origin,
      const std::string& device_guid,
      std::string* raw_device_id);

  // Device thread shared by VideoCaptureManager and AudioInputDeviceManager.
  scoped_ptr<base::Thread> device_thread_;

  media::AudioManager* const audio_manager_;  // not owned
  scoped_refptr<AudioInputDeviceManager> audio_input_device_manager_;
  scoped_refptr<VideoCaptureManager> video_capture_manager_;

  // Indicator of device monitoring state.
  bool monitoring_started_;

  // Stores most recently enumerated device lists. The cache is cleared when
  // monitoring is stopped or there is no request for that type of device.
  EnumerationCache audio_enumeration_cache_;
  EnumerationCache video_enumeration_cache_;

  // Keeps track of live enumeration commands sent to VideoCaptureManager or
  // AudioInputDeviceManager, in order to only enumerate when necessary.
  int active_enumeration_ref_count_[NUM_MEDIA_TYPES];

  // All non-closed request.
  DeviceRequests requests_;

  // Hold a pointer to the IO loop to check we delete the device thread and
  // managers on the right thread.
  base::MessageLoop* io_loop_;

  bool screen_capture_active_;

  bool use_fake_ui_;
  scoped_ptr<FakeMediaStreamUIProxy> fake_ui_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_
