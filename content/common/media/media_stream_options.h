// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_MEDIA_STREAM_OPTIONS_H_
#define CONTENT_COMMON_MEDIA_MEDIA_STREAM_OPTIONS_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"

namespace media_stream {

// StreamOptions is a Chromium representation of WebKit's
// WebUserMediaRequest Options. It describes the components
// in a request for a new media stream.
struct CONTENT_EXPORT StreamOptions {
  StreamOptions() : audio(false), video(false) {}
  StreamOptions(bool audio, bool video)
      : audio(audio), video(video) {}

  // True if the stream shall contain an audio input stream.
  bool audio;

  // True if the stream shall contain a video input stream.
  bool video;
};

typedef content::MediaStreamDeviceType MediaStreamType;

// StreamDeviceInfo describes information about a device.
struct CONTENT_EXPORT StreamDeviceInfo {
  static const int kNoId;

  StreamDeviceInfo();
  StreamDeviceInfo(MediaStreamType service_param,
                   const std::string& name_param,
                   const std::string& device_param,
                   bool opened);

  // Describes the capture type.
  MediaStreamType stream_type;
  // Friendly name of the device.
  std::string name;
  // Unique name of a device. Even if there are multiple devices with the same
  // friendly name connected to the computer, this will be unique.
  std::string device_id;
  // Set to true if the device has been opened, false otherwise.
  bool in_use;
  // Id for this capture session. Unique for all sessions of the same type.
  int session_id;
};

typedef std::vector<StreamDeviceInfo> StreamDeviceInfoArray;

}  // namespace media_stream

#endif  // CONTENT_COMMON_MEDIA_MEDIA_STREAM_OPTIONS_H_
