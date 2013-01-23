// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_MEDIA_STREAM_OPTIONS_H_
#define CONTENT_COMMON_MEDIA_MEDIA_STREAM_OPTIONS_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"

namespace content {

// MediaStreamConstraint keys for constraints that are passed to getUserMedia.
CONTENT_EXPORT extern const char kMediaStreamSource[];
CONTENT_EXPORT extern const char kMediaStreamSourceId[];
CONTENT_EXPORT extern const char kMediaStreamSourceTab[];

// Callback to deliver the result of a media request. |label| is the string
// to identify the request,
typedef base::Callback< void(const std::string&, const MediaStreamDevices&) >
    MediaRequestResponseCallback;

// StreamOptions is a Chromium representation of WebKit's
// WebUserMediaRequest Options. It describes the components
// in a request for a new media stream.
struct CONTENT_EXPORT StreamOptions {
  StreamOptions();
  StreamOptions(MediaStreamType audio_type, MediaStreamType video_type);

  // If not NO_SERVICE, the stream shall contain an audio input stream.
  MediaStreamType audio_type;
  std::string audio_device_id;

  // If not NO_SERVICE, the stream shall contain a video input stream.
  MediaStreamType video_type;
  std::string video_device_id;
};

// StreamDeviceInfo describes information about a device.
struct CONTENT_EXPORT StreamDeviceInfo {
  static const int kNoId;

  StreamDeviceInfo();
  StreamDeviceInfo(MediaStreamType service_param,
                   const std::string& name_param,
                   const std::string& device_param,
                   bool opened);
  StreamDeviceInfo(MediaStreamType service_param,
                   const std::string& name_param,
                   const std::string& device_param,
                   int sample_rate,
                   int channel_layout,
                   bool opened);
  static bool IsEqual(const StreamDeviceInfo& first,
                      const StreamDeviceInfo& second);

  MediaStreamDevice device;
  // Set to true if the device has been opened, false otherwise.
  bool in_use;
  // Id for this capture session. Unique for all sessions of the same type.
  int session_id;
};

typedef std::vector<StreamDeviceInfo> StreamDeviceInfoArray;

}  // namespace content

#endif  // CONTENT_COMMON_MEDIA_MEDIA_STREAM_OPTIONS_H_
