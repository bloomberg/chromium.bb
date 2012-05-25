// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MEDIA_STREAM_REQUEST_H_
#define CONTENT_PUBLIC_COMMON_MEDIA_STREAM_REQUEST_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"

namespace content {

// Types of media streams.
enum MediaStreamDeviceType {
  MEDIA_STREAM_DEVICE_TYPE_NO_SERVICE = 0,
  MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE,
  MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE,
  NUM_MEDIA_STREAM_DEVICE_TYPES
};

// Represents one device in a request for media stream(s).
struct CONTENT_EXPORT MediaStreamDevice {
  MediaStreamDevice(
      MediaStreamDeviceType type,
      const std::string& device_id,
      const std::string& name);

  // The device's type.
  MediaStreamDeviceType type;

  // The device's unique ID.
  std::string device_id;

  // The device's "friendly" name. Not guaranteed to be unique.
  std::string name;
};

typedef std::vector<MediaStreamDevice> MediaStreamDevices;

typedef std::map<MediaStreamDeviceType, MediaStreamDevices>
    MediaStreamDeviceMap;

// Represents a request for media streams (audio/video).
struct CONTENT_EXPORT MediaStreamRequest {
  MediaStreamRequest(
      int render_process_id,
      int render_view_id,
      const GURL& security_origin);

  ~MediaStreamRequest();

  // The render process id generating this request.
  int render_process_id;

  // The render view id generating this request.
  int render_view_id;

  // The WebKit security origin for the current request (e.g. "html5rocks.com").
  GURL security_origin;

  // A list of devices present on the user's computer, for each device type
  // requested.
  MediaStreamDeviceMap devices;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MEDIA_STREAM_REQUEST_H_
