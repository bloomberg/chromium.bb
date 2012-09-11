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

typedef content::MediaStreamDeviceType MediaStreamType;

// StreamOptions is a Chromium representation of WebKit's
// WebUserMediaRequest Options. It describes the components
// in a request for a new media stream.
struct CONTENT_EXPORT StreamOptions {
  StreamOptions();
  // TODO(miu): Remove the 2-bools ctor in later clean-up CL.
  StreamOptions(bool user_audio, bool user_video);
  StreamOptions(MediaStreamType audio_type, MediaStreamType video_type);

  // If not NO_SERVICE, the stream shall contain an audio input stream.
  MediaStreamType audio_type;

  // If not NO_SERVICE, the stream shall contain a video input stream.
  MediaStreamType video_type;
};

// StreamDeviceInfo describes information about a device.
struct CONTENT_EXPORT StreamDeviceInfo {
  static const int kNoId;

  StreamDeviceInfo();
  StreamDeviceInfo(MediaStreamType service_param,
                   const std::string& name_param,
                   const std::string& device_param,
                   bool opened);
  static bool IsEqual(const StreamDeviceInfo& first,
                      const StreamDeviceInfo& second);

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
