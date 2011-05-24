// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaStreamProvider is used to capture media of the types defined in
// MediaStreamType. There is only one MediaStreamProvider instance per media
// type and a MediaStreamProvider instance can have only one registered
// listener.
// The MediaStreamManager is expected to be called on Browser::IO thread and
// the listener will be called on the same thread.

#ifndef CONTENT_BROWSER_MEDIA_STREAM_MEDIA_STREAM_PROVIDER_H_
#define CONTENT_BROWSER_MEDIA_STREAM_MEDIA_STREAM_PROVIDER_H_

#include <list>
#include <string>

namespace media_stream {

// TODO(mflodman) Create a common type to use for all video capture and media
// stream classes.
typedef int MediaCaptureSessionId;

enum MediaStreamType {
  kNoService = 0,
  kAudioCapture,
  kVideoCapture
};

enum MediaStreamProviderError {
  kMediaStreamOk = 0,
  kInvalidMediaStreamType,
  kInvalidSession,
  kUnknownSession,
  kDeviceNotAvailable,
  kDeviceAlreadyInUse,
  kUnknownError
};

enum { kInvalidMediaCaptureSessionId = 0xFFFFFFFF };

struct MediaCaptureDeviceInfo {
  MediaCaptureDeviceInfo();
  MediaCaptureDeviceInfo(MediaStreamType service_param,
                         const std::string name_param,
                         const std::string device_param,
                         bool opened);

  MediaStreamType stream_type;
  std::string name;
  std::string device_id;
  bool in_use;
};

typedef std::list<MediaCaptureDeviceInfo> MediaCaptureDevices;

// Callback class used by MediaStreamProvider.
class MediaStreamProviderListener {
 public:
  // Called by a MediaStreamProvider when a stream has been opened.
  virtual void Opened(MediaStreamType stream_type,
                      MediaCaptureSessionId capture_session_id) = 0;

  // Called by a MediaStreamProvider when a stream has been closed.
  virtual void Closed(MediaStreamType stream_type,
                      MediaCaptureSessionId capture_session_id) = 0;

  // Called by a MediaStreamProvider when available devices has been enumerated.
  virtual void DevicesEnumerated(MediaStreamType stream_type,
                                 const MediaCaptureDevices& devices) = 0;

  // Called by a MediaStreamProvider when an error has occured.
  virtual void Error(MediaStreamType stream_type,
                     MediaCaptureSessionId capture_session_id,
                     MediaStreamProviderError error) = 0;

 protected:
  virtual ~MediaStreamProviderListener();
};

// Implemented by a manager class providing captured media.
class MediaStreamProvider {
 public:
  // Registers a listener, only one listener is allowed.
  virtual bool Register(MediaStreamProviderListener* listener) = 0;

  // Unregisters the previously registered listener.
  virtual void Unregister() = 0;

  // Enumerates existing capture devices and calls |DevicesEnumerated|.
  virtual void EnumerateDevices() = 0;

  // Opens the specified device. The device is not started and it is still
  // possible for other applications to open the device before the device is
  // started. |Opened| is called when the device is opened.
  // kInvalidMediaCaptureSessionId is returned on error.
  virtual MediaCaptureSessionId Open(const MediaCaptureDeviceInfo& device) = 0;

  // Closes the specified device and calls |Closed| when done.
  virtual void Close(MediaCaptureSessionId capture_session_id) = 0;

 protected:
  virtual ~MediaStreamProvider();
};

}  // namespace media_stream

#endif  // CONTENT_BROWSER_MEDIA_STREAM_MEDIA_STREAM_PROVIDER_H_
