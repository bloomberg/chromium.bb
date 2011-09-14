// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaStreamManager is used to open media capture devices (video supported
// now). Call flow:
// 1. GenerateStream is called when a render process wants to use a capture
//    device.
// 2. MediaStreamManager will ask MediaStreamDeviceSettings for permission to
//    use devices and for which device to use.
// 3. MediaStreamDeviceSettings will request list(s) of available devices, the
//    requests will be relayed to the corresponding media device manager and the
//    result will be given to MediaStreamDeviceSettings.
// 4. MediaStreamDeviceSettings will, by using user settings, pick devices which
//    devices to use and let MediaStreamManager know the result.
// 5. MediaStreamManager will call the proper media device manager to open the
//    device and let the MediaStreamRequester know it has been done.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/media_stream_settings_requester.h"
#include "content/common/media/media_stream_options.h"

namespace media_stream {

class MediaStreamDeviceSettings;
class MediaStreamRequester;
class VideoCaptureManager;

// MediaStreamManager is used to generate and close new media devices, not to
// start the media flow.
// The classes requesting new media streams are answered using
// MediaStreamManager::Listener.
class MediaStreamManager
    : public MediaStreamProviderListener,
      public SettingsRequester {
 public:
  MediaStreamManager();
  virtual ~MediaStreamManager();

  // Used to access VideoCaptureManager.
  VideoCaptureManager* video_capture_manager();

  // GenerateStream opens new media devices according to |components|. The
  // request is identified using |label|, which is pointing to an already
  // created std::string.
  void GenerateStream(MediaStreamRequester* requester, int render_process_id,
                      int render_view_id, const StreamOptions& options,
                      const std::string& security_origin, std::string* label);

  // Cancels all non-finished GenerateStream request, i.e. request for which
  // StreamGenerated hasn't been called.
  void CancelRequests(MediaStreamRequester* requester);

  // Closes generated stream.
  void StopGeneratedStream(const std::string& label);

  // Implements MediaStreamProviderListener.
  virtual void Opened(MediaStreamType stream_type, int capture_session_id);
  virtual void Closed(MediaStreamType stream_type, int capture_session_id);
  virtual void DevicesEnumerated(MediaStreamType stream_type,
                                 const StreamDeviceInfoArray& devices);
  virtual void Error(MediaStreamType stream_type, int capture_session_id,
                     MediaStreamProviderError error);

  // Implements SettingsRequester.
  virtual void GetDevices(const std::string& label,
                          MediaStreamType stream_type);
  virtual void DevicesAccepted(const std::string& label,
                               const StreamDeviceInfoArray& devices);
  virtual void SettingsError(const std::string& label);

  // Used by unit test to make sure fake devices are used instead of a real
  // devices, which is needed for server based testing.
  void UseFakeDevice();

 private:
  // Contains all data needed to keep track of requests.
  struct DeviceRequest {
    DeviceRequest();
    DeviceRequest(MediaStreamRequester* requester,
                  const StreamOptions& request_options);
    ~DeviceRequest();
    enum RequestState {
      kNotRequested = 0,
      kRequested,
      kOpening,
      kDone,
      kError
    };

    MediaStreamRequester* requester;
    StreamOptions options;
    std::vector<RequestState> state;
    StreamDeviceInfoArray audio_devices;
    StreamDeviceInfoArray video_devices;
  };

  // Helpers.
  bool RequestDone(const MediaStreamManager::DeviceRequest& request) const;
  MediaStreamProvider* GetDeviceManager(MediaStreamType stream_type);

  scoped_ptr<MediaStreamDeviceSettings> device_settings_;
  scoped_ptr<VideoCaptureManager> video_capture_manager_;
  // TODO(mflodman) Add AudioInputManager.

  // Keeps track of device types currently being enumerated to not enumerate
  // when not necessary.
  std::vector<bool> enumeration_in_progress_;

  // All non-closed request.
  typedef std::map<std::string, DeviceRequest> DeviceRequests;
  DeviceRequests requests_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamManager);
};

}  // namespace media_stream

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_
