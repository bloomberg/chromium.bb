// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_SETTINGS_REQUESTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_SETTINGS_REQUESTER_H_

#include <string>

#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"

namespace content {

// Implemented by the class requesting media capture device usage.
class CONTENT_EXPORT SettingsRequester {
 public:
  // If no error occurred, this call will deliver the result and the request
  // is considered answered.
  virtual void DevicesAccepted(const std::string& label,
                               const StreamDeviceInfoArray& devices) = 0;

  // An error for specified |request_id| has occurred.
  virtual void SettingsError(const std::string& label) = 0;

  // Called when user requested the stream with the specified |label| to be
  // stopped.
  virtual void StopStreamFromUI(const std::string& label) = 0;

  // Gets a list of available devices stored in the requester.
  virtual void GetAvailableDevices(MediaStreamDevices* devices) = 0;

 protected:
  virtual ~SettingsRequester() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_SETTINGS_REQUESTER_H_
