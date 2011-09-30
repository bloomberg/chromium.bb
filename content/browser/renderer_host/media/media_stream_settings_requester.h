// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_SETTINGS_REQUESTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_SETTINGS_REQUESTER_H_

#include <string>

#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"

namespace media_stream {

// Implemented by the class requesting media capture device usage.
class CONTENT_EXPORT SettingsRequester {
 public:
  // Called to get available devices for a certain media type. A call to
  // |AvailableDevices| with the currently available capture devices is
  // expected.
  virtual void GetDevices(const std::string& label,
                          MediaStreamType stream_type) = 0;

  // If no error occurred, this call will deliver the result and the request
  // is considered answered.
  virtual void DevicesAccepted(const std::string& label,
                               const StreamDeviceInfoArray& devices) = 0;

  // An error for specified |request_id| has occurred.
  virtual void SettingsError(const std::string& label) = 0;

 protected:
  virtual ~SettingsRequester() {}
};

}  // namespace media_stream

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_SETTINGS_REQUESTER_H_
