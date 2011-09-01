// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_MEDIA_STREAM_OPTIONS_H_
#define CONTENT_COMMON_MEDIA_MEDIA_STREAM_OPTIONS_H_

#include <string>
#include <vector>

namespace media_stream {

// StreamOptions is a Chromium representation of WebKit's
// WebGenerateStreamOptionFlags. It describes the components in a request for a
// new media stream.
struct StreamOptions {
  enum VideoOption {
    kNoCamera = 0,
    kFacingUser,
    kFacingEnvironment,
    kFacingBoth
  };

  StreamOptions() : audio(false), video_option(kNoCamera) {}
  StreamOptions(bool audio, VideoOption option)
      : audio(audio), video_option(option) {}

  // True if the stream shall contain an audio input stream.
  bool audio;

  // Describes if a / which type of video capture device is requested.
  VideoOption video_option;
};

// Type of media stream.
enum MediaStreamType {
  kNoService = 0,
  kAudioCapture,
  kVideoCapture,
  kNumMediaStreamTypes
};

// StreamDeviceInfo describes information about a device.
struct StreamDeviceInfo {
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
